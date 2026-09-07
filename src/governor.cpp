// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/governor.hpp"

#include "contention/checked.hpp"
#include "contention/persistence.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <utility>

namespace contention {

namespace {

bool is_disruptive_class(InterventionClass c) {
  switch (c) {
    case InterventionClass::THROTTLE:
    case InterventionClass::REDUCE_CONCURRENCY:
    case InterventionClass::REDUCE_BATCH:
    case InterventionClass::SERIALIZE:
    case InterventionClass::TIME_SLICE:
    case InterventionClass::REQUEST_PREEMPTION:
    case InterventionClass::REQUEST_MIGRATION:
    case InterventionClass::REQUEST_REPLACEMENT:
    case InterventionClass::CHANGE_PLACEMENT:
    case InterventionClass::REQUEST_BANDWIDTH_REALLOCATION:
    case InterventionClass::REQUEST_COMMUNICATION_REPLAN:
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE:
    case InterventionClass::RELEASE_OPTIONAL_RESIDENCY:
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
    case InterventionClass::SHED_LOW_VALUE_WORK:
    case InterventionClass::DEFER_NEW_WORK:
      return true;
    default:
      return false;
  }
}

}  // namespace

struct Governor::Impl {
  std::mutex mu;
  GovernorState state;
  Clock* clock {nullptr};
  AdapterRegistry* adapters {nullptr};
  std::string persistPath;
  bool persistEnable {false};
};

Governor::Governor() : Governor(Options{}) {}

Governor::Governor(Options opts) : impl_(std::make_unique<Impl>()) {
  impl_->clock = opts.clock ? opts.clock : new SystemClock();
  impl_->adapters = opts.adapters;
  impl_->persistEnable = opts.persistEnable;
  impl_->persistPath = opts.persistPath;
}

Governor::~Governor() = default;
Governor::Governor(Governor&&) noexcept = default;
Governor& Governor::operator=(Governor&&) noexcept = default;

Status Governor::open() {
  if (!impl_->persistEnable || impl_->persistPath.empty()) return Status::good();
  Status st = load_state(impl_->persistPath, impl_->state);
  return st;
}

Status Governor::set_policy(const ContentionPolicy& p) {
  Status v = validate_policy(p);
  if (!v.ok()) return v;
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->state.engine.policy = p;
  impl_->state.engine.authority.policyGeneration = p.generation;
  return Status::good();
}

Status Governor::set_authority(const AuthorityContext& a) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->state.engine.authority = a;
  return Status::good();
}

Status Governor::record_worker_boot(WorkloadId w, WorkerBootId boot) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->state.engine.authority.workloadBoots[w] = boot;
  return Status::good();
}

Status Governor::record_workload_generation(WorkloadId w, WorkloadGeneration g) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->state.engine.authority.workloadGenerations[w] = g;
  return Status::good();
}

Status Governor::advance_epoch() {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->state.engine.authority.coordinatorEpoch.advance();
  for (auto& [id, e] : impl_->state.engine.evidence) {
    if (e.status == EvidenceStatus::CURRENT) {
      e.status = EvidenceStatus::REVALIDATION_REQUIRED;
    }
  }
  for (auto& [id, c] : impl_->state.engine.conflicts) {
    if (c.state != ConflictState::RESOLVED && c.state != ConflictState::SUPPRESSED &&
        c.state != ConflictState::SUPERSEDED) {
      c.state = ConflictState::REVALIDATION_REQUIRED;
    }
  }
  for (auto& [id, i] : impl_->state.engine.interventions) {
    if (i.state == LifecycleState::DISPATCHED || i.state == LifecycleState::ACKNOWLEDGED ||
        i.state == LifecycleState::AUTHORIZED || i.state == LifecycleState::PROPOSED) {
      i.state = LifecycleState::OUTCOME_UNKNOWN;
    }
  }
  for (auto& [id, f] : impl_->state.engine.fairness) {
    (void)id;
    f.consecutiveInterventions = 0;
  }
  return Status::good();
}

Status Governor::ingest_evidence(const InterferenceEvidence& input) {
  InterferenceEvidence e = input;
  Status v = validate_evidence(e);
  if (!v.ok()) return v;
  e.degradation = compute_degradation(e.metric, e.isolatedBaseline, e.coRunObservation);
  e.improvement = compute_improvement(e.metric, e.isolatedBaseline, e.coRunObservation);

  std::lock_guard<std::mutex> lock(impl_->mu);
  auto& st = impl_->state;
  if (e.id.value() == 0) {
    e.id = InterferenceEvidenceId(st.nextEvidenceId);
    ++st.nextEvidenceId;
  }
  st.engine.evidence[e.id] = e;

  // Locate or create a conflict for (victim, domain).
  ConflictId cid;
  ContentionConflict* conflict = nullptr;
  for (auto& [id, c] : st.engine.conflicts) {
    if (c.domain == e.domain && std::find(c.affected.begin(), c.affected.end(), e.victim) !=
                                    c.affected.end()) {
      conflict = &c;
      cid = c.id;
      break;
    }
  }
  if (conflict == nullptr) {
    ContentionConflict c;
    c.id = ConflictId(st.nextConflictId);
    ++st.nextConflictId;
    c.generation = ConflictGeneration{1};
    c.affected = {e.victim};
    c.associated = e.neighbors;
    c.domain = e.domain;
    c.baseline = e.isolatedBaseline;
    c.degradation = e.degradation;
    c.confidence = e.confidence;
    c.binding = e.metric == MetricDirection::HIGHER_IS_BETTER
                    ? BindingObjective::THROUGHPUT
                    : BindingObjective::LATENCY;
    c.severity = classify_severity(e.degradation);
    c.evidenceIds = {e.id};
    c.state = ConflictState::DETECTED;
    c.firstDetectedAt = e.sampledAt;
    c.lastUpdatedAt = e.sampledAt;
    c.activeUntil = e.activeUntil;
    c.unresolvedConfounders = e.unresolvedConfounders;
    c.coordinatorEpoch = st.engine.authority.coordinatorEpoch;
    c.workerBoot = e.workerBoot;
    c.policyGeneration = st.engine.policy.generation;
    c.sloGeneration = st.engine.authority.sloGeneration;
    c.valueGeneration = st.engine.authority.valueGeneration;
    c.fairnessGeneration = st.engine.authority.fairnessGeneration;
    c.resourceGeneration = st.engine.authority.resourceGeneration;
    c.placementGeneration = st.engine.authority.placementGeneration;
    cid = c.id;
    st.engine.conflicts[c.id] = c;
    conflict = &st.engine.conflicts[c.id];
  } else {
    // Update the existing conflict.
    conflict->associated = e.neighbors;
    conflict->degradation = std::max(conflict->degradation, e.degradation);
    conflict->confidence = std::max(conflict->confidence, e.confidence);
    conflict->evidenceIds.push_back(e.id);
    conflict->lastUpdatedAt = e.sampledAt;
    conflict->unresolvedConfounders = e.unresolvedConfounders;
  }

  // Conflict state progression based on evidence health.
  if (e.status == EvidenceStatus::CURRENT && conflict->degradation >= st.engine.policy.actionabilityThreshold) {
    conflict->state = ConflictState::ACTIONABLE;
    conflict->inActionableState = true;
  } else if (e.status == EvidenceStatus::CURRENT) {
    conflict->state = ConflictState::VALIDATING;
  } else if (e.status == EvidenceStatus::CONFOUNDED || e.status == EvidenceStatus::INSUFFICIENT_EVIDENCE) {
    conflict->state = ConflictState::UNRESOLVED;
  } else if (e.status == EvidenceStatus::REVALIDATION_REQUIRED) {
    conflict->state = ConflictState::REVALIDATION_REQUIRED;
  }

  return Status::good();
}

ConflictEvaluation Governor::evaluate(ConflictId id) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return evaluate_conflict(impl_->state.engine, id, impl_->state.engine.authority);
}

namespace {

void consume_budget(ActionBudget& b, InterventionClass c, TimeMs now) {
  if (now - b.windowStart > b.windowMs) {
    b.used = ActionBudgetStats{};
    b.windowStart = now;
  }
  switch (c) {
    case InterventionClass::REQUEST_PREEMPTION: b.used.preemptions++; break;
    case InterventionClass::REQUEST_MIGRATION: b.used.migrations++; break;
    case InterventionClass::CHANGE_PLACEMENT: b.used.placementChanges++; break;
    case InterventionClass::THROTTLE:
    case InterventionClass::REDUCE_CONCURRENCY:
    case InterventionClass::REDUCE_BATCH:
    case InterventionClass::SERIALIZE:
    case InterventionClass::TIME_SLICE:
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
      b.used.throttles++; break;
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE: b.used.collectiveReschedules++; break;
    default: break;
  }
  b.used.all++;
}

bool budget_exhausted(const ActionBudget& b, InterventionClass c) {
  switch (c) {
    case InterventionClass::REQUEST_PREEMPTION: return b.used.preemptions >= b.maxPreemptionsPerWindow;
    case InterventionClass::REQUEST_MIGRATION: return b.used.migrations >= b.maxMigrationsPerWindow;
    case InterventionClass::CHANGE_PLACEMENT: return b.used.placementChanges >= b.maxPlacementChangesPerWindow;
    case InterventionClass::THROTTLE:
    case InterventionClass::REDUCE_CONCURRENCY:
    case InterventionClass::REDUCE_BATCH:
    case InterventionClass::SERIALIZE:
    case InterventionClass::TIME_SLICE:
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
      return b.used.throttles >= b.maxThrottlesPerWindow;
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE:
      return b.used.collectiveReschedules >= b.maxCollectiveReschedulesPerWindow;
    default: return false;
  }
}

}  // namespace

std::optional<InterventionId> Governor::authorize(ConflictId id, InterventionClass cls) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto& st = impl_->state;
  auto cit = st.engine.conflicts.find(id);
  if (cit == st.engine.conflicts.end())
    return std::nullopt;

  ConflictEvaluation ev = evaluate_conflict(st.engine, id, st.engine.authority);
  const InterventionCandidate* picked = nullptr;
  if (ev.decision) {
    picked = &ev.decision->chosen;
  } else {
    for (const auto& cand : ev.candidates) {
      if (cand.feasible == FeasibilityStatus::FEASIBLE) {
        picked = &cand;
        break;
      }
    }
  }
  if (picked == nullptr) {
    return std::nullopt;
  }
  InterventionClass chosen = picked->cls;
  WorkloadId target = picked->target;
  if (cls != InterventionClass::NO_ACTION) {
    chosen = cls;
  }

  // Pre-dispatch revalidation & hard feasibility.
  Fence f = make_fence(st.engine.authority, cit->second.generation,
                       st.engine.evidence.find(cit->second.evidenceIds.front())->second.generation,
                       target, InterventionGeneration{1});
  if (revalidate(st.engine, f, target).code != StatusCode::OK) {
    return std::nullopt;
  }
  FeasibilityResult feas = check_feasibility(st.engine, target, chosen, f);
  if (feas.status != FeasibilityStatus::FEASIBLE) {
    return std::nullopt;
  }
  if (budget_exhausted(st.engine.policy.budget, chosen))
    return std::nullopt;
  const TimeMs now = impl_->clock->now_ms();
  if (is_disruptive_class(chosen)) {
    auto fit = st.engine.fairness.find(target);
    if (fit != st.engine.fairness.end() &&
        fit->second.lastInterventionAt != 0 &&
        now - fit->second.lastInterventionAt < st.engine.policy.cooldownMs) {
      return std::nullopt;
    }
  }

    // Create the intervention record.
    InterventionRecord ir;
    ir.id = InterventionId(st.nextInterventionId);
    ++st.nextInterventionId;
    ir.generation = InterventionGeneration{1};
    ir.cls = chosen;
    ir.conflictId = id;
    ir.target = target;
    ir.state = LifecycleState::AUTHORIZED;
    ir.fence = f;
    ir.adapter = ev.decision ? ev.decision->chosen.adapter : "";
    ir.authorizedAt = now;
    ir.dispatchId = DispatchId(st.nextDispatchId);
    ++st.nextDispatchId;
    ir.attemptId = AttemptId(st.nextAttemptId);
    ++st.nextAttemptId;
    const InterventionId interId = ir.id;

    // Exact accounting: budget + fairness.
    consume_budget(st.engine.policy.budget, chosen, now);

    auto& fr = st.engine.fairness[target];
    if (fr.workload.value() == 0) fr.workload = target;
    fr.interventionCount = saturating_add(fr.interventionCount, 1);
    fr.consecutiveInterventions =
        fr.consecutiveInterventions == UINT32_MAX ? UINT32_MAX : fr.consecutiveInterventions + 1;
    fr.lastInterventionAt = now;

    st.engine.interventions[interId] = ir;
    cit->second.state = ConflictState::INTERVENTION_AUTHORIZED;
    cit->second.selectedIds.push_back(interId);
    cit->second.lastActionAt = now;
  return interId;
}

Status Governor::dispatch(InterventionId id) {
  // Pre-dispatch revalidation (inside lock).
  InterventionClass cls;
  WorkloadId target;
  AttemptId attempt;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto& st = impl_->state;
    auto it = st.engine.interventions.find(id);
    if (it == st.engine.interventions.end())
      return Status::bad(StatusCode::INVALID_INPUT, "unknown intervention");
    if (it->second.state != LifecycleState::AUTHORIZED)
      return Status::bad(StatusCode::STALE_AUTHORITY, "intervention not in AUTHORIZED state");
    Status rv = revalidate(st.engine, it->second.fence, it->second.target);
    if (rv.code != StatusCode::OK) {
      it->second.state = LifecycleState::SUPERSEDED;
      return rv;
    }
    it->second.state = LifecycleState::DISPATCHED;
    it->second.dispatchId = DispatchId(st.nextDispatchId);
    ++st.nextDispatchId;
    it->second.dispatchedAt = impl_->clock->now_ms();
    cls = it->second.cls;
    target = it->second.target;
    attempt = it->second.attemptId;
  }

  // Adapter call OUTSIDE the lock.
  AdapterResult res;
  if (impl_->adapters != nullptr) {
    switch (cls) {
      case InterventionClass::THROTTLE:
      case InterventionClass::REDUCE_CONCURRENCY:
      case InterventionClass::REDUCE_BATCH:
      case InterventionClass::SERIALIZE:
      case InterventionClass::TIME_SLICE:
      case InterventionClass::REDUCE_MEMORY_PRESSURE: {
        auto* thr = impl_->adapters->throttle();
        if (thr) res = thr->apply(attempt, target, cls, 1.0);
        else res = {AdapterOutcome::UNAVAILABLE, "no throttle adapter", attempt};
        break;
      }
      default:
        res = {AdapterOutcome::UNAVAILABLE, "no registered adapter for class", attempt};
        break;
    }
  } else {
    res = {AdapterOutcome::UNAVAILABLE, "no adapter registry", attempt};
  }

  // Apply the adapter outcome (inside lock).
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto it = impl_->state.engine.interventions.find(id);
    if (it == impl_->state.engine.interventions.end())
      return Status::bad(StatusCode::INVALID_INPUT, "intervention vanished");
    if (res.outcome == AdapterOutcome::ACCEPTED) {
      it->second.state = LifecycleState::ACKNOWLEDGED;
      it->second.ackNote = res.note;
    } else if (res.outcome == AdapterOutcome::UNKNOWN) {
      it->second.state = LifecycleState::OUTCOME_UNKNOWN;
    } else {
      it->second.state = LifecycleState::FAILED;
      it->second.interimStatus =
          Status::bad(StatusCode::INTERVENTION_FAILED, res.note.empty() ? "adapter rejected" : res.note);
    }
  }
  return Status::good();
}

Status Governor::authorize_and_dispatch(ConflictId id, InterventionClass cls) {
  if (cls != InterventionClass::NO_ACTION) {
    std::lock_guard<std::mutex> lock(impl_->mu);
    if (budget_exhausted(impl_->state.engine.policy.budget, cls))
      return Status::bad(StatusCode::ACTION_BUDGET_EXHAUSTED, "action budget exhausted");
  }
  auto interId = authorize(id, cls);
  if (!interId) return Status::bad(StatusCode::NO_LEGAL_INTERVENTION, "no legal intervention");
  return dispatch(*interId);
}

Status Governor::on_acknowledge(InterventionId id, AttemptId attempt) {
  (void)attempt;
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->state.engine.interventions.find(id);
  if (it == impl_->state.engine.interventions.end())
    return Status::bad(StatusCode::INVALID_INPUT, "unknown intervention");
  if (it->second.state != LifecycleState::DISPATCHED)
    return Status::bad(StatusCode::STALE_AUTHORITY, "intervention not DISPATCHED");
  it->second.state = LifecycleState::ACKNOWLEDGED;
  it->second.acknowledgedAt = impl_->clock->now_ms();
  return Status::good();
}

Status Governor::on_action_failed(InterventionId id, AttemptId attempt, const std::string& note) {
  (void)attempt;
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->state.engine.interventions.find(id);
  if (it == impl_->state.engine.interventions.end())
    return Status::bad(StatusCode::INVALID_INPUT, "unknown intervention");
  it->second.state = LifecycleState::FAILED;
  it->second.interimStatus = Status::bad(StatusCode::INTERVENTION_FAILED, note);
  return Status::good();
}

Status Governor::submit_post_action_evidence(const InterferenceEvidence& input) {
  InterferenceEvidence e = input;
  Status v = validate_evidence(e);
  if (!v.ok()) return v;
  e.degradation = compute_degradation(e.metric, e.isolatedBaseline, e.coRunObservation);
  e.improvement = compute_improvement(e.metric, e.isolatedBaseline, e.coRunObservation);

  std::lock_guard<std::mutex> lock(impl_->mu);
  auto& st = impl_->state;
  if (e.id.value() == 0) {
    e.id = InterferenceEvidenceId(st.nextEvidenceId);
    ++st.nextEvidenceId;
  }
  st.engine.evidence[e.id] = e;

  // Locate the conflict and any non-terminal intervention targeting it.
  ContentionConflict* conflict = nullptr;
  for (auto& [id, c] : st.engine.conflicts) {
    if (c.domain == e.domain && std::find(c.affected.begin(), c.affected.end(), e.victim) !=
                                    c.affected.end()) {
      conflict = &c;
      break;
    }
  }
  if (conflict == nullptr) return Status::good();
  const double before = conflict->degradation;
  conflict->degradation = e.degradation;
  conflict->lastUpdatedAt = e.sampledAt;
  conflict->evidenceIds.push_back(e.id);

  // Pick a non-terminal intervention on this conflict.
  InterventionRecord* iv = nullptr;
  for (auto& [id, i2] : st.engine.interventions) {
    if (i2.conflictId == conflict->id &&
        (i2.state == LifecycleState::DISPATCHED || i2.state == LifecycleState::ACKNOWLEDGED ||
         i2.state == LifecycleState::AUTHORIZED)) {
      iv = &i2;
      break;
    }
  }

  // Detect a secondary hard violation: any other conflict where the workload we
  // slowed now exceeds its hard floor.
  bool secondaryHard = false;
  if (iv != nullptr) {
    const auto twp = workload_policy_for(st.engine.policy, iv->target);
    for (const auto& [id, c] : st.engine.conflicts) {
      if (id == conflict->id) continue;
      for (auto& victim : c.affected) {
        if (victim == iv->target && c.degradation > (1.0 - twp.hardSloFloor)) {
          secondaryHard = true;
        }
      }
    }
  }

  const double after = conflict->degradation;
  VerificationOutcome outcome = VerificationOutcome::OUTCOME_UNKNOWN;
  if (secondaryHard) {
    outcome = after < st.engine.policy.actionabilityThreshold
                  ? VerificationOutcome::SHIFTED_CONTENTION
                  : VerificationOutcome::CREATED_SECONDARY_VIOLATION;
  } else if (after <= st.engine.policy.actionabilityThreshold) {
    outcome = VerificationOutcome::RESOLVED;
  } else if (after < before) {
    outcome = VerificationOutcome::PARTIALLY_RESOLVED;
  } else if (after > before) {
    outcome = VerificationOutcome::WORSENED;
  } else {
    outcome = VerificationOutcome::INEFFECTIVE;
  }

  if (iv != nullptr) {
    iv->result = outcome;
    iv->completedAt = e.sampledAt;
    iv->verificationReported = true;
    switch (outcome) {
      case VerificationOutcome::RESOLVED:
        iv->state = LifecycleState::EFFECTIVE;
        conflict->state = ConflictState::RESOLVED;
        {
          auto rit = st.engine.fairness.find(iv->target);
          if (rit != st.engine.fairness.end()) rit->second.consecutiveInterventions = 0;
        }
        break;
      case VerificationOutcome::PARTIALLY_RESOLVED:
        iv->state = LifecycleState::PARTIALLY_EFFECTIVE;
        conflict->state = ConflictState::PARTIALLY_RESOLVED;
        break;
      case VerificationOutcome::INEFFECTIVE:
        iv->state = LifecycleState::INEFFECTIVE;
        conflict->state = ConflictState::UNRESOLVED;
        break;
      case VerificationOutcome::WORSENED:
      case VerificationOutcome::SHIFTED_CONTENTION:
      case VerificationOutcome::CREATED_SECONDARY_VIOLATION:
      case VerificationOutcome::CREATED_FAIRNESS_VIOLATION:
        iv->state = LifecycleState::INEFFECTIVE;
        conflict->state = ConflictState::UNRESOLVED;
        break;
      default:
        iv->state = LifecycleState::OUTCOME_UNKNOWN;
        conflict->state = ConflictState::UNRESOLVED;
        break;
    }
  }
  return Status::good();
}

std::vector<ConflictId> Governor::conflict_ids() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  std::vector<ConflictId> ids;
  for (const auto& [id, c] : impl_->state.engine.conflicts) ids.push_back(id);
  return ids;
}

std::optional<ContentionConflict> Governor::conflict(ConflictId id) const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->state.engine.conflicts.find(id);
  if (it == impl_->state.engine.conflicts.end()) return std::nullopt;
  return it->second;
}

std::optional<InterventionRecord> Governor::intervention(InterventionId id) const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->state.engine.interventions.find(id);
  if (it == impl_->state.engine.interventions.end()) return std::nullopt;
  return it->second;
}

std::optional<FairnessRecord> Governor::fairness(WorkloadId w) const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->state.engine.fairness.find(w);
  if (it == impl_->state.engine.fairness.end()) return std::nullopt;
  return it->second;
}

ActionBudget Governor::action_budget() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return impl_->state.engine.policy.budget;
}

Status Governor::save() {
  if (!impl_->persistEnable || impl_->persistPath.empty()) return Status::good();
  std::lock_guard<std::mutex> lock(impl_->mu);
  Status st;
  save_state(impl_->persistPath, impl_->state, st);
  return st;
}

Status Governor::load() {
  if (!impl_->persistEnable || impl_->persistPath.empty()) return Status::good();
  return load_state(impl_->persistPath, impl_->state);
}

Digest64 Governor::digest() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  GovernorState copy = impl_->state;
  copy.lastDigest = 0;
  std::vector<std::uint8_t> bytes = save_to_bytes(copy);
  Digest64 h = fnv1a_bytes(bytes.data(), bytes.size());
  return h;
}

std::size_t Governor::persistence_bytes() const {
  std::lock_guard<std::mutex> lock(impl_->mu);
  return save_to_bytes(impl_->state).size();
}

const GovernorState* Governor::snapshot() const { return &impl_->state; }

}  // namespace contention
