// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/engine.hpp"

#include "contention/checked.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace contention {

namespace {

bool is_disruptive(InterventionClass c) {
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

// A conservative fixed order used only to break ties between equally-ranked
// candidates (determinism, not a policy preference beyond the tie-break).
const std::vector<InterventionClass>& class_order() {
  static const std::vector<InterventionClass> order = {
      InterventionClass::THROTTLE,
      InterventionClass::REDUCE_CONCURRENCY,
      InterventionClass::REDUCE_BATCH,
      InterventionClass::SERIALIZE,
      InterventionClass::TIME_SLICE,
      InterventionClass::REQUEST_PREEMPTION,
      InterventionClass::REQUEST_MIGRATION,
      InterventionClass::REQUEST_REPLACEMENT,
      InterventionClass::CHANGE_PLACEMENT,
      InterventionClass::REQUEST_BANDWIDTH_REALLOCATION,
      InterventionClass::REQUEST_COMMUNICATION_REPLAN,
      InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE,
      InterventionClass::RELEASE_OPTIONAL_RESIDENCY,
      InterventionClass::REDUCE_MEMORY_PRESSURE,
      InterventionClass::SHED_LOW_VALUE_WORK,
      InterventionClass::PROTECT_HIGH_VALUE_WORK,
      InterventionClass::DEFER_NEW_WORK,
      InterventionClass::ESCALATE,
      InterventionClass::NO_ACTION,
      InterventionClass::OBSERVE_MORE,
      InterventionClass::MANUAL_INTERVENTION_REQUIRED};
  return order;
}

std::size_t class_index(InterventionClass c) {
  const auto& order = class_order();
  for (std::size_t i = 0; i < order.size(); ++i) {
    if (order[i] == c) return i;
  }
  return order.size();
}

// Effect model defaults for a given class. These are conservative, deterministic
// placeholders that the caller may override with derived evidence.
void default_effect(InterventionClass cls, double conflictDegradation,
                    InterventionCandidate& c) {
  (void)conflictDegradation;
  switch (cls) {
    case InterventionClass::THROTTLE:
      c.expectedConflictReduction = 0.5;
      c.expectedNeighborDegradation = 0.15;
      c.reversibility = 0.9;
      c.timeToEffect = 50;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::REDUCE_CONCURRENCY:
      c.expectedConflictReduction = 0.4;
      c.expectedNeighborDegradation = 0.20;
      c.reversibility = 0.85;
      c.timeToEffect = 100;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::REDUCE_BATCH:
      c.expectedConflictReduction = 0.35;
      c.expectedNeighborDegradation = 0.12;
      c.reversibility = 0.9;
      c.timeToEffect = 50;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::SERIALIZE:
      c.expectedConflictReduction = 0.55;
      c.expectedNeighborDegradation = 0.25;
      c.reversibility = 0.9;
      c.timeToEffect = 20;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::TIME_SLICE:
      c.expectedConflictReduction = 0.3;
      c.expectedNeighborDegradation = 0.1;
      c.reversibility = 0.95;
      c.timeToEffect = 40;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::REQUEST_PREEMPTION:
      c.expectedConflictReduction = 0.8;
      c.expectedNeighborDegradation = 0.5;
      c.reversibility = 0.1;
      c.timeToEffect = 200;
      c.adapter = "IPreemptionAdapter";
      break;
    case InterventionClass::REQUEST_MIGRATION:
      c.expectedConflictReduction = 0.85;
      c.expectedNeighborDegradation = 0.05;
      c.reversibility = 0.3;
      c.timeToEffect = 500;
      c.adapter = "IPlacementAdapter";
      break;
    case InterventionClass::CHANGE_PLACEMENT:
      c.expectedConflictReduction = 0.7;
      c.expectedNeighborDegradation = 0.1;
      c.reversibility = 0.3;
      c.timeToEffect = 300;
      c.adapter = "IPlacementAdapter";
      break;
    case InterventionClass::REQUEST_BANDWIDTH_REALLOCATION:
      c.expectedConflictReduction = 0.6;
      c.expectedNeighborDegradation = 0.08;
      c.reversibility = 0.8;
      c.timeToEffect = 100;
      c.adapter = "IBandwidthAdapter";
      break;
    case InterventionClass::REQUEST_COMMUNICATION_REPLAN:
      c.expectedConflictReduction = 0.5;
      c.expectedNeighborDegradation = 0.05;
      c.reversibility = 0.4;
      c.timeToEffect = 200;
      c.adapter = "ICommunicationAdapter";
      break;
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE:
      c.expectedConflictReduction = 0.55;
      c.expectedNeighborDegradation = 0.05;
      c.reversibility = 0.5;
      c.timeToEffect = 150;
      c.adapter = "ICollectiveAdapter";
      break;
    case InterventionClass::RELEASE_OPTIONAL_RESIDENCY:
      c.expectedConflictReduction = 0.2;
      c.expectedNeighborDegradation = 0.05;
      c.reversibility = 0.6;
      c.timeToEffect = 30;
      c.adapter = "IPlacementAdapter";
      break;
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
      c.expectedConflictReduction = 0.4;
      c.expectedNeighborDegradation = 0.1;
      c.reversibility = 0.8;
      c.timeToEffect = 80;
      c.adapter = "IThrottleAdapter";
      break;
    case InterventionClass::DEFER_NEW_WORK:
      c.expectedConflictReduction = 0.3;
      c.expectedNeighborDegradation = 0.02;
      c.reversibility = 0.9;
      c.timeToEffect = 30;
      c.adapter = "IAdmissionAdapter";
      break;
    case InterventionClass::SHED_LOW_VALUE_WORK:
      c.expectedConflictReduction = 0.45;
      c.expectedNeighborDegradation = 0.35;
      c.reversibility = 0.1;
      c.timeToEffect = 120;
      c.adapter = "IAdmissionAdapter";
      break;
    case InterventionClass::PROTECT_HIGH_VALUE_WORK:
      c.expectedConflictReduction = 0.35;
      c.expectedNeighborDegradation = 0.1;
      c.reversibility = 0.8;
      c.timeToEffect = 60;
      c.adapter = "IPreemptionAdapter";
      break;
    default:
      c.expectedConflictReduction = 0.0;
      c.expectedNeighborDegradation = 0.0;
      c.reversibility = 0.5;
      c.timeToEffect = 0;
      c.adapter = "";
      break;
  }
  c.expectedTargetImprovement =
      std::max(0.0, std::min(1.0, c.expectedConflictReduction));
  c.expectedRecoveryCost = (cls == InterventionClass::REQUEST_PREEMPTION) ? 1.0 : 0.0;
  c.expectedCost = 0.1 * c.expectedNeighborDegradation;
  c.confidence = 0.5;
}

}  // namespace

// ---------------------------------------------------------------------------
// Feasibility
// ---------------------------------------------------------------------------
FeasibilityResult check_feasibility(const EngineState& s, WorkloadId target,
                                    InterventionClass cls, const Fence& f) {
  FeasibilityResult r;
  r.status = FeasibilityStatus::FEASIBLE;
  const auto& wp = workload_policy_for(s.policy, target);

  // Authority fence must be current before a candidate is even legal.
  Status rv = revalidate(s, f, target);
  if (!rv.ok()) {
    r.status = FeasibilityStatus::INFEASIBLE;
    r.reasons.push_back(std::string(to_string(rv.code)));
    return r;
  }

  switch (cls) {
    case InterventionClass::THROTTLE:
    case InterventionClass::REDUCE_CONCURRENCY:
    case InterventionClass::REDUCE_BATCH:
    case InterventionClass::SERIALIZE:
    case InterventionClass::TIME_SLICE:
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
      if (!wp.throttlingAllowed || !s.policy.throttleAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("throttling disabled");
      }
      if (wp.hardSloFloor >= 1.0 - 1e-9) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("would violate hard SLO floor");
      }
      break;
    case InterventionClass::REQUEST_PREEMPTION:
    case InterventionClass::REQUEST_REPLACEMENT:
      if (wp.nonPreemptible) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("non-preemptible workload");
      }
      if (!wp.preemptionAllowed || !s.policy.preemptionAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("preemption disabled");
      }
      break;
    case InterventionClass::REQUEST_MIGRATION:
      if (wp.nonPreemptible) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("non-preemptible workload");
      }
      if (!wp.migrationAllowed || !s.policy.migrationAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("migration disabled");
      }
      break;
    case InterventionClass::DEFER_NEW_WORK:
      if (!wp.deferralAllowed || !s.policy.admissionRestrictionAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("deferral disabled");
      }
      break;
    case InterventionClass::CHANGE_PLACEMENT:
      if (!wp.placementChangeAllowed || !s.policy.placementChangeAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("placement change disabled");
      }
      break;
    case InterventionClass::REQUEST_BANDWIDTH_REALLOCATION:
      if (!wp.bandwidthRequestAllowed || !s.policy.bandwidthRequestAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("bandwidth request disabled");
      }
      break;
    case InterventionClass::REQUEST_COMMUNICATION_REPLAN:
      if (!s.policy.bandwidthRequestAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("communication replan disabled");
      }
      break;
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE:
      if (!wp.collectiveSchedulingAllowed || !s.policy.collectiveSchedulingAllowed) {
        r.status = FeasibilityStatus::INFEASIBLE;
        r.reasons.push_back("collective reschedule disabled");
      }
      break;
    default:
      break;
  }

  // Fairness limit: never target a workload that is at/over the consecutive
  // intervention cap (prevents starvation).
  auto fit = s.fairness.find(target);
  if (fit != s.fairness.end() &&
      fit->second.consecutiveInterventions >= s.policy.maxConsecutiveInterventionsPerWorkload) {
    r.status = FeasibilityStatus::INFEASIBLE;
    r.reasons.push_back("fairness limit reached for target");
  }

  return r;
}

// ---------------------------------------------------------------------------
// Authority fencing
// ---------------------------------------------------------------------------
Fence make_fence(const AuthorityContext& ctx, ConflictGeneration conflictGeneration,
                 InterferenceEvidenceGeneration evidenceGeneration, WorkloadId target,
                 InterventionGeneration interventionGeneration) {
  Fence f;
  f.coordinatorEpoch = ctx.coordinatorEpoch;
  f.governorGeneration = ctx.governorGeneration;
  f.policyGeneration = ctx.policyGeneration;
  f.sloGeneration = ctx.sloGeneration;
  f.valueGeneration = ctx.valueGeneration;
  f.fairnessGeneration = ctx.fairnessGeneration;
  f.resourceGeneration = ctx.resourceGeneration;
  f.placementGeneration = ctx.placementGeneration;
  f.topologyGeneration = ctx.topologyGeneration;
  f.conflictGeneration = conflictGeneration;
  f.evidenceGeneration = evidenceGeneration;
  f.interventionGeneration = interventionGeneration;
  auto wg = ctx.workloadGenerations.find(target);
  f.workloadGeneration = (wg != ctx.workloadGenerations.end()) ? wg->second : WorkloadGeneration{};
  auto wb = ctx.workloadBoots.find(target);
  f.workerBoot = (wb != ctx.workloadBoots.end()) ? wb->second : WorkerBootId{};
  return f;
}

Status revalidate(const EngineState& s, const Fence& f, WorkloadId target) {
  const auto& a = s.authority;
  if (f.coordinatorEpoch != a.coordinatorEpoch)
    return Status::bad(StatusCode::STALE_AUTHORITY, "coordinator epoch stale");
  if (f.governorGeneration != a.governorGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "governor generation stale");
  if (f.policyGeneration != a.policyGeneration)
    return Status::bad(StatusCode::POLICY_SUPERSEDED, "policy generation stale");
  if (f.sloGeneration != a.sloGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "SLO generation stale");
  if (f.valueGeneration != a.valueGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "value generation stale");
  if (f.fairnessGeneration != a.fairnessGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "fairness generation stale");
  if (f.resourceGeneration != a.resourceGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "resource generation stale");
  if (f.placementGeneration != a.placementGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "placement generation stale");
  if (f.topologyGeneration != a.topologyGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "topology generation stale");

  auto wg = a.workloadGenerations.find(target);
  if (wg == a.workloadGenerations.end() || wg->second != f.workloadGeneration)
    return Status::bad(StatusCode::STALE_AUTHORITY, "workload generation stale");
  auto wb = a.workloadBoots.find(target);
  if (wb == a.workloadBoots.end() || wb->second != f.workerBoot)
    return Status::bad(StatusCode::STALE_AUTHORITY, "worker boot stale");
  return Status::good();
}

// ---------------------------------------------------------------------------
// Evaluate (deterministic pipeline)
// ---------------------------------------------------------------------------
ConflictEvaluation evaluate_conflict(const EngineState& s, ConflictId conflictId,
                                     const AuthorityContext& ctx) {
  ConflictEvaluation ev;
  ev.conflictId = conflictId;
  ev.status = Status::good();

  auto cit = s.conflicts.find(conflictId);
  if (cit == s.conflicts.end()) {
    ev.state = ConflictState::UNKNOWN;
    ev.status = Status::bad(StatusCode::INVALID_INPUT, "unknown conflict");
    return ev;
  }
  const ContentionConflict& c = cit->second;
  ev.state = c.state;
  ev.binding = c.binding;
  ev.degradation = c.degradation;
  ev.severity = c.severity;

  // 1. Validate evidence. If the conflict has no current evidence, no control.
  const InterferenceEvidence* primary = nullptr;
  std::vector<const InterferenceEvidence*> evids;
  for (auto eid : c.evidenceIds) {
    auto eit = s.evidence.find(eid);
    if (eit == s.evidence.end()) continue;
    const InterferenceEvidence& e = eit->second;
    if (e.status != EvidenceStatus::CURRENT) {
      ev.rejected.push_back({InterventionClass::NO_ACTION,
                             std::string("evidence not current: ") + to_string(e.status)});
    } else if (!structurally_valid(e)) {
      ev.rejected.push_back({InterventionClass::NO_ACTION, "evidence structurally invalid"});
    } else {
      evids.push_back(&e);
    }
  }

  if (evids.empty()) {
    ev.rejected.push_back({InterventionClass::NO_ACTION, "no current valid evidence"});
    ev.state = ConflictState::UNKNOWN;
    ev.status = Status::bad(StatusCode::INSUFFICIENT_EVIDENCE,
                            "no CURRENT, structurally valid evidence");
    return ev;
  }
  primary = evids.front();

  // 2. Attribution gating for destructive action.
  bool destructive_ok = true;
  if (primary->attribution < s.policy.minAttributionForDisruptive) {
    destructive_ok = false;
    ev.rejected.push_back({InterventionClass::NO_ACTION,
                           "attribution below disruptive threshold: " +
                               std::string(to_string(primary->attribution))});
  }
  if (primary->confidence < s.policy.minConfidenceForDisruptive) {
    destructive_ok = false;
    ev.rejected.push_back({InterventionClass::NO_ACTION, "confidence below disruptive threshold"});
  }
  if (!primary->unresolvedConfounders.empty()) {
    destructive_ok = false;
    ev.rejected.push_back({InterventionClass::NO_ACTION, "evidence has unresolved confounders"});
  }

  // 3. Actionability threshold (hysteresis entered).
  if (c.degradation < s.policy.actionabilityThreshold) {
    ev.rejected.push_back({InterventionClass::NO_ACTION, "degradation below actionability threshold"});
    ev.state = ConflictState::UNKNOWN;
    ev.status = Status::bad(StatusCode::INSUFFICIENT_EVIDENCE, "not actionable");
    return ev;
  }

  // 4. Targets = associated workloads (interferers). No associated workload =>
  //    no justified aggressor => cannot act.
  std::vector<WorkloadId> targets = c.associated;
  std::sort(targets.begin(), targets.end());

  if (targets.empty()) {
    ev.rejected.push_back({InterventionClass::OBSERVE_MORE,
                           "no justified associated workload; correlation alone is not enough"});
    ev.state = ConflictState::UNRESOLVED;
    ev.status = Status::bad(StatusCode::CONFOUNDED, "no associated workload identified");
    return ev;
  }

  // Enforce the destructive-action gate: attribution/confidence/confounders
  // must satisfy policy. Low-confidence evidence may produce NO_ACTION but never
  // a destructive decision.
  if (!destructive_ok) {
    ev.state = ConflictState::UNRESOLVED;
    ev.status = Status::bad(StatusCode::INSUFFICIENT_EVIDENCE,
                            "insufficient attribution/confidence for destructive action");
    return ev;
  }

  // 5. Build candidate interventions over all (target, eligible class) pairs.
  std::vector<InterventionCandidate> feasible;
  std::vector<InterventionCandidate> all;
  const auto eligible_classes = std::vector<InterventionClass>{
      InterventionClass::THROTTLE,
      InterventionClass::REDUCE_CONCURRENCY,
      InterventionClass::REDUCE_BATCH,
      InterventionClass::SERIALIZE,
      InterventionClass::TIME_SLICE,
      InterventionClass::REQUEST_PREEMPTION,
      InterventionClass::REQUEST_MIGRATION,
      InterventionClass::REQUEST_REPLACEMENT,
      InterventionClass::CHANGE_PLACEMENT,
      InterventionClass::REQUEST_BANDWIDTH_REALLOCATION,
      InterventionClass::REQUEST_COMMUNICATION_REPLAN,
      InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE,
      InterventionClass::RELEASE_OPTIONAL_RESIDENCY,
      InterventionClass::REDUCE_MEMORY_PRESSURE,
      InterventionClass::DEFER_NEW_WORK,
      InterventionClass::SHED_LOW_VALUE_WORK,
      InterventionClass::PROTECT_HIGH_VALUE_WORK};

  for (WorkloadId target : targets) {
    for (InterventionClass cls : eligible_classes) {
      if (!class_eligible(s.policy, cls)) continue;
      InterventionCandidate cand;
      cand.cls = cls;
      cand.target = target;
      default_effect(cls, c.degradation, cand);

      // Build a candidate fence as if authorized now.
      Fence f = make_fence(ctx, c.generation, primary->generation, target,
                           InterventionGeneration{1});
      FeasibilityResult fr = check_feasibility(s, target, cls, f);
      cand.feasible = fr.status;
      cand.infeasibilityReasons = fr.reasons;
      all.push_back(cand);
      if (fr.status == FeasibilityStatus::FEASIBLE) feasible.push_back(cand);
    }
  }
  ev.candidates = all;

  // 6. Deterministic ranking of feasible candidates.
  if (!feasible.empty()) {
    auto better = [&](const InterventionCandidate& a, const InterventionCandidate& b) {
      if (a.expectedConflictReduction != b.expectedConflictReduction)
        return a.expectedConflictReduction > b.expectedConflictReduction;
      if (a.expectedNeighborDegradation != b.expectedNeighborDegradation)
        return a.expectedNeighborDegradation < b.expectedNeighborDegradation;
      double va = workload_policy_for(s.policy, a.target).value;
      double vb = workload_policy_for(s.policy, b.target).value;
      if (va != vb) return va < vb;
      if (a.expectedCost != b.expectedCost) return a.expectedCost < b.expectedCost;
      if (a.reversibility != b.reversibility) return a.reversibility > b.reversibility;
      std::size_t ia = class_index(a.cls);
      std::size_t ib = class_index(b.cls);
      if (ia != ib) return ia < ib;
      if (a.target.value() != b.target.value()) return a.target.value() < b.target.value();
      return a.cls < b.cls;
    };
    std::stable_sort(feasible.begin(), feasible.end(), better);
    ev.decision = InterventionDecision{feasible.front(), {}};
    if (ev.decision) {
      ev.decision->rankingTrace.push_back(
          std::string("selected ") + to_string(feasible.front().cls) +
          " on workload " + std::to_string(feasible.front().target.value()));
    }
    ev.state = ConflictState::ACTIONABLE;
  } else {
    ev.state = ConflictState::UNRESOLVED;
    ev.status = Status::bad(StatusCode::NO_LEGAL_INTERVENTION, "no legal intervention");
  }

  return ev;
}

InterventionId next_intervention_id() {
  static std::uint64_t counter = 1;
  return InterventionId(counter++);
}

}  // namespace contention
