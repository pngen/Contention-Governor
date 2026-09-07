// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/persistence.hpp"

#include "contention/digest.hpp"

#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

#if defined(_WIN32)
#include <cstdio>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace contention {

namespace {

constexpr std::uint8_t kMagic[8] = {'C', 'G', 'O', 'V', 1, 0, 0, 0};

struct W {
  std::vector<std::uint8_t>& b;
  explicit W(std::vector<std::uint8_t>& bb) : b(bb) {}
  void u8(std::uint8_t v) { b.push_back(v); }
  void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); }
  void u64(std::uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); }
  void f64(double v) { std::uint64_t u = 0; std::memcpy(&u, &v, 8); u64(u); }
  void i64(std::int64_t v) { std::uint64_t u = 0; std::memcpy(&u, &v, 8); u64(u); }
  void str(std::string_view s) {
    u64(static_cast<std::uint64_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
  }
  void bytes(const void* p, std::size_t n) { const auto* q = static_cast<const std::uint8_t*>(p); b.insert(b.end(), q, q + n); }
};

template <typename Id>
void wid(W& w, Id id) { w.u64(id.value()); }

// ---- Reader (bounded, allocation-capped) ----
struct R {
  const std::uint8_t* d;
  std::size_t n;
  std::size_t p {0};
  bool ok {true};

  R(const std::uint8_t* data, std::size_t len) : d(data), n(len) {}

  bool u8(std::uint8_t& v) { if (!ok || p + 1 > n) { ok = false; return false; } v = d[p++]; return true; }
  bool u32(std::uint32_t& v) {
    if (!ok || p + 4 > n) { ok = false; return false; }
    v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(d[p + i]) << (8 * i);
    p += 4; return true;
  }
  bool u64(std::uint64_t& v) {
    if (!ok || p + 8 > n) { ok = false; return false; }
    v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(d[p + i]) << (8 * i);
    p += 8; return true;
  }
  bool f64(double& v) { std::uint64_t u; if (!u64(u)) return false; std::memcpy(&v, &u, 8); return true; }
  bool i64(std::int64_t& v) { std::uint64_t u; if (!u64(u)) return false; std::memcpy(&v, &u, 8); return true; }
  bool str(std::string& s) {
    std::uint64_t len; if (!u64(len)) return false;
    if (len > kMaxStringLen) { ok = false; return false; }
    if (p + len > n) { ok = false; return false; }
    s.assign(reinterpret_cast<const char*>(d + p), static_cast<std::size_t>(len));
    p += static_cast<std::size_t>(len); return true;
  }
  bool bytes(void* out, std::size_t len) {
    if (!ok || p + len > n) { ok = false; return false; }
    std::memcpy(out, d + p, len); p += len; return true;
  }
  template <typename Id>
  bool id(Id& v) { std::uint64_t u; if (!u64(u)) return false; v = Id(u); return true; }
  bool done() const { return ok && p == n; }
  bool good() const { return ok; }
};

template <typename T, typename E>
bool e_from_u8(std::uint8_t raw, T& out, std::size_t count) {
  if (raw >= count) return false;
  out = static_cast<T>(raw);
  return true;
}

#define EU8(raw, out, T, count) \
  do { std::uint8_t rv = 0; if (!rd.u8(rv)) return false; \
       if (rv >= (count)) return false; out = static_cast<T>(rv); } while (0)

}  // namespace

// ---------------------------------------------------------------------------
// Encode the entire durable state.
// ---------------------------------------------------------------------------
namespace {

// Forward declarations for cross-referencing serializers (W/R are defined above).
void serialize_workload(W& w, const WorkloadPolicy& wp);
bool deserialize_workload(R& rd, WorkloadPolicy& wp);

template <typename Id>
bool r_id(R& rd, Id& v) { return rd.id(v); }

void serialize_policy(W& w, const ContentionPolicy& p) {
  wid(w, p.id); wid(w, p.generation);
  w.f64(p.actionabilityThreshold); w.f64(p.releaseThreshold);
  w.u64(p.hysteresisEvidenceCount);
  w.u8(static_cast<std::uint8_t>(p.minAttributionForDisruptive));
  w.f64(p.minConfidenceForDisruptive);
  w.f64(p.fairnessFloor); w.f64(p.fairnessWeight);
  w.u32(p.maxConsecutiveInterventionsPerWorkload);
  w.f64(p.costCeiling);
  w.u64(p.cooldownMs);
  // budget
  wid(w, p.budget.generation);
  w.u64(p.budget.maxPreemptionsPerWindow); w.u64(p.budget.maxMigrationsPerWindow);
  w.u64(p.budget.maxPlacementChangesPerWindow); w.u64(p.budget.maxThrottlesPerWindow);
  w.u64(p.budget.maxCollectiveReschedulesPerWindow); w.u64(p.budget.maxAnyPerWindow);
  w.u64(p.budget.windowMs); w.u64(p.budget.windowStart);
  w.u64(p.budget.used.preemptions); w.u64(p.budget.used.migrations);
  w.u64(p.budget.used.placementChanges); w.u64(p.budget.used.throttles);
  w.u64(p.budget.used.collectiveReschedules); w.u64(p.budget.used.all);
  // workloads
  w.u64(static_cast<std::uint64_t>(p.workloads.size()));
  for (const auto& [id, wp] : p.workloads) { wid(w, id); serialize_workload(w, wp); }
  w.u8(p.preemptionAllowed ? 1 : 0); w.u8(p.migrationAllowed ? 1 : 0);
  w.u8(p.placementChangeAllowed ? 1 : 0); w.u8(p.bandwidthRequestAllowed ? 1 : 0);
  w.u8(p.collectiveSchedulingAllowed ? 1 : 0); w.u8(p.admissionRestrictionAllowed ? 1 : 0);
  w.u8(p.throttleAllowed ? 1 : 0);
}

void serialize_workload(W& w, const WorkloadPolicy& wp) {
  wid(w, wp.workload); wid(w, wp.generation);
  w.u8(wp.nonPreemptible ? 1 : 0); w.u8(wp.protectedWorkload ? 1 : 0);
  w.f64(wp.value); w.u32(wp.priority);
  w.f64(wp.hardSloFloor); w.u8(wp.softSloTarget.has_value() ? 1 : 0);
  if (wp.softSloTarget) w.f64(*wp.softSloTarget);
  w.f64(wp.minServiceFloor); w.f64(wp.costCeiling);
  w.f64(wp.restartCost); w.f64(wp.preemptionCost); w.f64(wp.recoveryCost);
  w.f64(wp.maxDegradationTolerated); w.u32(wp.maxInterventionFrequencyPerWindow);
  w.u8(wp.preemptionAllowed ? 1 : 0); w.u8(wp.throttlingAllowed ? 1 : 0);
  w.u8(wp.deferralAllowed ? 1 : 0); w.u8(wp.admissionRestrictionAllowed ? 1 : 0);
  w.u8(wp.placementChangeAllowed ? 1 : 0); w.u8(wp.bandwidthRequestAllowed ? 1 : 0);
  w.u8(wp.collectiveSchedulingAllowed ? 1 : 0); w.u8(wp.migrationAllowed ? 1 : 0);
}

void serialize_authority(W& w, const AuthorityContext& a) {
  wid(w, a.coordinatorEpoch); wid(w, a.governorGeneration);
  wid(w, a.policyGeneration); wid(w, a.sloGeneration);
  wid(w, a.valueGeneration); wid(w, a.fairnessGeneration);
  wid(w, a.resourceGeneration); wid(w, a.placementGeneration);
  wid(w, a.topologyGeneration);
  w.u64(static_cast<std::uint64_t>(a.workloadGenerations.size()));
  for (const auto& [id, g] : a.workloadGenerations) { wid(w, id); wid(w, g); }
  w.u64(static_cast<std::uint64_t>(a.workloadBoots.size()));
  for (const auto& [id, b] : a.workloadBoots) { wid(w, id); wid(w, b); }
}

void serialize_evidence(W& w, const InterferenceEvidence& e) {
  wid(w, e.id); wid(w, e.generation); wid(w, e.victim);
  w.u64(static_cast<std::uint64_t>(e.neighbors.size()));
  for (auto x : e.neighbors) wid(w, x);
  w.u8(static_cast<std::uint8_t>(e.domain));
  w.u8(static_cast<std::uint8_t>(e.metric));
  w.f64(e.isolatedBaseline); w.f64(e.coRunObservation);
  w.f64(e.degradation); w.f64(e.improvement);
  w.u8(static_cast<std::uint8_t>(e.attribution));
  w.f64(e.confidence); w.u64(e.sampleCount);
  w.u64(static_cast<std::uint64_t>(e.unresolvedConfounders.size()));
  for (auto& x : e.unresolvedConfounders) w.str(x);
  w.u8(static_cast<std::uint8_t>(e.status));
  w.u64(e.sampledAt); w.u64(e.maxAgeMs);
  w.u8(static_cast<std::uint8_t>(e.label));
  w.str(e.sourceId); w.str(e.sourceHealth);
  wid(w, e.workerBoot); wid(w, e.workloadGeneration); wid(w, e.deviceGeneration);
  wid(w, e.hostGeneration); wid(w, e.topologyGeneration);
  wid(w, e.resourceGeneration); wid(w, e.placementGeneration);
  w.u64(e.activeFrom); w.u64(e.activeUntil);
}

void serialize_fence(W& w, const Fence& f) {
  wid(w, f.coordinatorEpoch); wid(w, f.governorGeneration);
  wid(w, f.policyGeneration); wid(w, f.sloGeneration);
  wid(w, f.valueGeneration); wid(w, f.fairnessGeneration);
  wid(w, f.resourceGeneration); wid(w, f.placementGeneration);
  wid(w, f.topologyGeneration); wid(w, f.conflictGeneration);
  wid(w, f.evidenceGeneration); wid(w, f.workerBoot);
  wid(w, f.workloadGeneration); wid(w, f.interventionGeneration);
}

void serialize_intervention(W& w, const InterventionRecord& i) {
  wid(w, i.id); wid(w, i.generation);
  w.u8(static_cast<std::uint8_t>(i.cls)); wid(w, i.conflictId); wid(w, i.target);
  w.u8(static_cast<std::uint8_t>(i.state));
  serialize_fence(w, i.fence);
  w.str(i.adapter);
  w.u64(i.authorizedAt); w.u64(i.dispatchedAt); w.u64(i.acknowledgedAt); w.u64(i.completedAt);
  wid(w, i.dispatchId); wid(w, i.attemptId);
  w.str(i.ackNote);
  w.u8(static_cast<std::uint8_t>(i.interimStatus.code)); w.str(i.interimStatus.message);
  w.u8(static_cast<std::uint8_t>(i.result));
  w.u64(static_cast<std::uint64_t>(i.trace.size()));
  for (auto& t : i.trace) w.str(t);
  w.u8(i.verificationReported ? 1 : 0);
}

void serialize_conflict(W& w, const ContentionConflict& c) {
  wid(w, c.id); wid(w, c.generation);
  w.u64(c.affected.size()); for (auto x : c.affected) wid(w, x);
  w.u64(c.associated.size()); for (auto x : c.associated) wid(w, x);
  w.u8(static_cast<std::uint8_t>(c.domain));
  w.f64(c.degradation); w.f64(c.baseline); w.f64(c.confidence);
  w.u8(static_cast<std::uint8_t>(c.binding)); w.u8(static_cast<std::uint8_t>(c.severity));
  w.u64(c.evidenceIds.size()); for (auto x : c.evidenceIds) wid(w, x);
  w.u8(static_cast<std::uint8_t>(c.state));
  w.u64(c.firstDetectedAt); w.u64(c.lastUpdatedAt); w.u64(c.activeUntil);
  w.u64(c.unresolvedConfounders.size()); for (auto& x : c.unresolvedConfounders) w.str(x);
  w.u64(c.blockers.size()); for (auto& x : c.blockers) w.str(x);
  w.u64(c.candidateClasses.size()); for (auto x : c.candidateClasses) w.u8(static_cast<std::uint8_t>(x));
  w.u64(c.proposedIds.size()); for (auto x : c.proposedIds) wid(w, x);
  w.u64(c.selectedIds.size()); for (auto x : c.selectedIds) wid(w, x);
  wid(w, c.policyGeneration); wid(w, c.sloGeneration); wid(w, c.valueGeneration);
  wid(w, c.fairnessGeneration); wid(w, c.resourceGeneration); wid(w, c.placementGeneration);
  wid(w, c.coordinatorEpoch); wid(w, c.workerBoot);
  w.u8(c.groupEffectExplicit ? 1 : 0);
  w.u64(c.lastActionAt); w.u8(c.inActionableState ? 1 : 0);
}

void serialize_fairness(W& w, const FairnessRecord& f) {
  wid(w, f.workload); w.u64(f.interventionCount); w.u32(f.consecutiveInterventions);
  w.f64(f.deprivationDebt); w.u64(f.lastInterventionAt);
}

}  // namespace

std::vector<std::uint8_t> save_to_bytes(const GovernorState& s) {
  std::vector<std::uint8_t> payload;
  W w(payload);
  serialize_policy(w, s.engine.policy);
  serialize_authority(w, s.engine.authority);
  w.u64(s.engine.conflicts.size());
  for (const auto& [id, c] : s.engine.conflicts) { wid(w, id); serialize_conflict(w, c); }
  w.u64(s.engine.evidence.size());
  for (const auto& [id, e] : s.engine.evidence) { wid(w, id); serialize_evidence(w, e); }
  w.u64(s.engine.interventions.size());
  for (const auto& [id, i] : s.engine.interventions) { wid(w, id); serialize_intervention(w, i); }
  w.u64(s.engine.fairness.size());
  for (const auto& [id, f] : s.engine.fairness) { wid(w, id); serialize_fairness(w, f); }
  w.u64(s.nextConflictId); w.u64(s.nextEvidenceId); w.u64(s.nextInterventionId);
  w.u64(s.nextDispatchId); w.u64(s.nextAttemptId); w.u64(s.nextVerificationId);
  w.u64(s.lastDigest);

  std::vector<std::uint8_t> out;
  out.insert(out.end(), kMagic, kMagic + 8);
  W h(out);
  h.u32(kPersistenceVersion);
  h.u64(static_cast<std::uint64_t>(payload.size()));
  out.insert(out.end(), payload.begin(), payload.end());
  std::uint32_t crc = crc32c(payload.data(), payload.size());
  W cw(out);
  cw.u32(crc);
  return out;
}

namespace {
bool deserialize_policy(R& rd, ContentionPolicy& p) {
  if (!r_id(rd, p.id) || !r_id(rd, p.generation)) return false;
  if (!rd.f64(p.actionabilityThreshold) || !rd.f64(p.releaseThreshold)) return false;
  std::uint64_t hc = 0; if (!rd.u64(hc)) return false; p.hysteresisEvidenceCount = hc;
  std::uint8_t raw = 0; if (!rd.u8(raw) || raw >= 7) return false;
  p.minAttributionForDisruptive = static_cast<AttributionStrength>(raw);
  if (!rd.f64(p.minConfidenceForDisruptive)) return false;
  if (!rd.f64(p.fairnessFloor) || !rd.f64(p.fairnessWeight)) return false;
  std::uint32_t mc = 0; if (!rd.u32(mc)) return false; p.maxConsecutiveInterventionsPerWorkload = mc;
  if (!rd.f64(p.costCeiling)) return false;
  if (!rd.u64(p.cooldownMs)) return false;
  if (!r_id(rd, p.budget.generation)) return false;
  if (!rd.u64(p.budget.maxPreemptionsPerWindow) || !rd.u64(p.budget.maxMigrationsPerWindow) ||
      !rd.u64(p.budget.maxPlacementChangesPerWindow) || !rd.u64(p.budget.maxThrottlesPerWindow) ||
      !rd.u64(p.budget.maxCollectiveReschedulesPerWindow) || !rd.u64(p.budget.maxAnyPerWindow)) return false;
  if (!rd.u64(p.budget.windowMs) || !rd.u64(p.budget.windowStart)) return false;
  if (!rd.u64(p.budget.used.preemptions) || !rd.u64(p.budget.used.migrations) ||
      !rd.u64(p.budget.used.placementChanges) || !rd.u64(p.budget.used.throttles) ||
      !rd.u64(p.budget.used.collectiveReschedules) || !rd.u64(p.budget.used.all)) return false;
  std::uint64_t nw = 0; if (!rd.u64(nw) || nw > kMaxCollectionLen) return false;
  p.workloads.clear();
  for (std::uint64_t i = 0; i < nw; ++i) {
    WorkloadId id; if (!r_id(rd, id)) return false;
    WorkloadPolicy wp; if (!deserialize_workload(rd, wp)) return false;
    p.workloads[id] = wp;
  }
  std::uint8_t f = 0;
  if (!rd.u8(f)) return false; p.preemptionAllowed = f != 0;
  if (!rd.u8(f)) return false; p.migrationAllowed = f != 0;
  if (!rd.u8(f)) return false; p.placementChangeAllowed = f != 0;
  if (!rd.u8(f)) return false; p.bandwidthRequestAllowed = f != 0;
  if (!rd.u8(f)) return false; p.collectiveSchedulingAllowed = f != 0;
  if (!rd.u8(f)) return false; p.admissionRestrictionAllowed = f != 0;
  if (!rd.u8(f)) return false; p.throttleAllowed = f != 0;
  return true;
}

bool deserialize_workload(R& rd, WorkloadPolicy& wp) {
  if (!r_id(rd, wp.workload) || !r_id(rd, wp.generation)) return false;
  std::uint8_t b = 0;
  if (!rd.u8(b)) return false; wp.nonPreemptible = b != 0;
  if (!rd.u8(b)) return false; wp.protectedWorkload = b != 0;
  if (!rd.f64(wp.value)) return false;
  if (!rd.u32(wp.priority)) return false;
  if (!rd.f64(wp.hardSloFloor)) return false;
  if (!rd.u8(b)) return false;
  if (b != 0) { double d; if (!rd.f64(d)) return false; wp.softSloTarget = d; }
  if (!rd.f64(wp.minServiceFloor) || !rd.f64(wp.costCeiling) ||
      !rd.f64(wp.restartCost) || !rd.f64(wp.preemptionCost) || !rd.f64(wp.recoveryCost)) return false;
  if (!rd.f64(wp.maxDegradationTolerated)) return false;
  if (!rd.u32(wp.maxInterventionFrequencyPerWindow)) return false;
  if (!rd.u8(b)) return false; wp.preemptionAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.throttlingAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.deferralAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.admissionRestrictionAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.placementChangeAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.bandwidthRequestAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.collectiveSchedulingAllowed = b != 0;
  if (!rd.u8(b)) return false; wp.migrationAllowed = b != 0;
  return true;
}

}  // namespace

// (Forward declarations for the rest of the decode.)
namespace {
bool deserialize_authority(R& rd, AuthorityContext& a);
bool deserialize_evidence(R& rd, InterferenceEvidence& e);
bool deserialize_fence(R& rd, Fence& f);
bool deserialize_intervention(R& rd, InterventionRecord& i);
bool deserialize_conflict(R& rd, ContentionConflict& c);
bool deserialize_fairness(R& rd, FairnessRecord& f);
}

namespace {

bool deserialize_authority(R& rd, AuthorityContext& a) {
  if (!r_id(rd, a.coordinatorEpoch) || !r_id(rd, a.governorGeneration)) return false;
  if (!r_id(rd, a.policyGeneration) || !r_id(rd, a.sloGeneration)) return false;
  if (!r_id(rd, a.valueGeneration) || !r_id(rd, a.fairnessGeneration)) return false;
  if (!r_id(rd, a.resourceGeneration) || !r_id(rd, a.placementGeneration)) return false;
  if (!r_id(rd, a.topologyGeneration)) return false;
  std::uint64_t nwg = 0; if (!rd.u64(nwg) || nwg > kMaxCollectionLen) return false;
  a.workloadGenerations.clear();
  for (std::uint64_t i = 0; i < nwg; ++i) { WorkloadId id; WorkloadGeneration g; if (!r_id(rd, id) || !r_id(rd, g)) return false; a.workloadGenerations[id] = g; }
  std::uint64_t nwb = 0; if (!rd.u64(nwb) || nwb > kMaxCollectionLen) return false;
  a.workloadBoots.clear();
  for (std::uint64_t i = 0; i < nwb; ++i) { WorkloadId id; WorkerBootId b; if (!r_id(rd, id) || !r_id(rd, b)) return false; a.workloadBoots[id] = b; }
  return true;
}

bool deserialize_evidence(R& rd, InterferenceEvidence& e) {
  if (!r_id(rd, e.id) || !r_id(rd, e.generation) || !r_id(rd, e.victim)) return false;
  std::uint64_t nn = 0; if (!rd.u64(nn) || nn > kMaxCollectionLen) return false;
  e.neighbors.clear();
  for (std::uint64_t i = 0; i < nn; ++i) { WorkloadId x; if (!r_id(rd, x)) return false; e.neighbors.push_back(x); }
  std::uint8_t raw = 0;
  if (!rd.u8(raw) || raw >= 14) return false; e.domain = static_cast<ResourceDomain>(raw);
  if (!rd.u8(raw) || raw >= 2) return false; e.metric = static_cast<MetricDirection>(raw);
  if (!rd.f64(e.isolatedBaseline) || !rd.f64(e.coRunObservation)) return false;
  if (!rd.f64(e.degradation) || !rd.f64(e.improvement)) return false;
  if (!rd.u8(raw) || raw >= 6) return false; e.attribution = static_cast<AttributionStrength>(raw);
  if (!rd.f64(e.confidence)) return false;
  if (!rd.u64(e.sampleCount)) return false;
  std::uint64_t nc = 0; if (!rd.u64(nc) || nc > kMaxCollectionLen) return false;
  e.unresolvedConfounders.clear();
  for (std::uint64_t i = 0; i < nc; ++i) { std::string s; if (!rd.str(s)) return false; e.unresolvedConfounders.push_back(std::move(s)); }
  if (!rd.u8(raw) || raw >= 8) return false; e.status = static_cast<EvidenceStatus>(raw);
  if (!rd.u64(e.sampledAt) || !rd.u64(e.maxAgeMs)) return false;
  if (!rd.u8(raw) || raw >= 6) return false; e.label = static_cast<EvidenceLabel>(raw);
  if (!rd.str(e.sourceId) || !rd.str(e.sourceHealth)) return false;
  if (!r_id(rd, e.workerBoot) || !r_id(rd, e.workloadGeneration) || !r_id(rd, e.deviceGeneration)) return false;
  if (!r_id(rd, e.hostGeneration) || !r_id(rd, e.topologyGeneration)) return false;
  if (!r_id(rd, e.resourceGeneration) || !r_id(rd, e.placementGeneration)) return false;
  if (!rd.u64(e.activeFrom) || !rd.u64(e.activeUntil)) return false;
  return true;
}

bool deserialize_fence(R& rd, Fence& f) {
  if (!r_id(rd, f.coordinatorEpoch) || !r_id(rd, f.governorGeneration)) return false;
  if (!r_id(rd, f.policyGeneration) || !r_id(rd, f.sloGeneration)) return false;
  if (!r_id(rd, f.valueGeneration) || !r_id(rd, f.fairnessGeneration)) return false;
  if (!r_id(rd, f.resourceGeneration) || !r_id(rd, f.placementGeneration)) return false;
  if (!r_id(rd, f.topologyGeneration) || !r_id(rd, f.conflictGeneration)) return false;
  if (!r_id(rd, f.evidenceGeneration) || !r_id(rd, f.workerBoot)) return false;
  if (!r_id(rd, f.workloadGeneration) || !r_id(rd, f.interventionGeneration)) return false;
  return true;
}

bool deserialize_intervention(R& rd, InterventionRecord& i) {
  if (!r_id(rd, i.id) || !r_id(rd, i.generation)) return false;
  std::uint8_t raw = 0;
  if (!rd.u8(raw) || raw >= 21) return false; i.cls = static_cast<InterventionClass>(raw);
  if (!r_id(rd, i.conflictId) || !r_id(rd, i.target)) return false;
  if (!rd.u8(raw) || raw >= 12) return false; i.state = static_cast<LifecycleState>(raw);
  if (!deserialize_fence(rd, i.fence)) return false;
  if (!rd.str(i.adapter)) return false;
  if (!rd.u64(i.authorizedAt) || !rd.u64(i.dispatchedAt) || !rd.u64(i.acknowledgedAt) || !rd.u64(i.completedAt)) return false;
  if (!r_id(rd, i.dispatchId) || !r_id(rd, i.attemptId)) return false;
  if (!rd.str(i.ackNote)) return false;
  if (!rd.u8(raw) || raw >= 21) return false; i.interimStatus.code = static_cast<StatusCode>(raw);
  if (!rd.str(i.interimStatus.message)) return false;
  if (!rd.u8(raw) || raw >= 9) return false; i.result = static_cast<VerificationOutcome>(raw);
  std::uint64_t nt = 0; if (!rd.u64(nt) || nt > kMaxCollectionLen) return false;
  i.trace.clear();
  for (std::uint64_t k = 0; k < nt; ++k) { std::string s; if (!rd.str(s)) return false; i.trace.push_back(std::move(s)); }
  if (!rd.u8(raw)) return false; i.verificationReported = raw != 0;
  return true;
}

bool deserialize_conflict(R& rd, ContentionConflict& c) {
  if (!r_id(rd, c.id) || !r_id(rd, c.generation)) return false;
  std::uint64_t n1 = 0; if (!rd.u64(n1) || n1 > kMaxCollectionLen) return false;
  c.affected.clear(); for (std::uint64_t i = 0; i < n1; ++i) { WorkloadId x; if (!r_id(rd, x)) return false; c.affected.push_back(x); }
  std::uint64_t n2 = 0; if (!rd.u64(n2) || n2 > kMaxCollectionLen) return false;
  c.associated.clear(); for (std::uint64_t i = 0; i < n2; ++i) { WorkloadId x; if (!r_id(rd, x)) return false; c.associated.push_back(x); }
  std::uint8_t raw = 0;
  if (!rd.u8(raw) || raw >= 14) return false; c.domain = static_cast<ResourceDomain>(raw);
  if (!rd.f64(c.degradation) || !rd.f64(c.baseline) || !rd.f64(c.confidence)) return false;
  if (!rd.u8(raw) || raw >= 12) return false; c.binding = static_cast<BindingObjective>(raw);
  if (!rd.u8(raw) || raw >= 6) return false; c.severity = static_cast<Severity>(raw);
  std::uint64_t n3 = 0; if (!rd.u64(n3) || n3 > kMaxCollectionLen) return false;
  c.evidenceIds.clear(); for (std::uint64_t i = 0; i < n3; ++i) { InterferenceEvidenceId x; if (!r_id(rd, x)) return false; c.evidenceIds.push_back(x); }
  if (!rd.u8(raw) || raw >= 13) return false; c.state = static_cast<ConflictState>(raw);
  if (!rd.u64(c.firstDetectedAt) || !rd.u64(c.lastUpdatedAt) || !rd.u64(c.activeUntil)) return false;
  std::uint64_t n4 = 0; if (!rd.u64(n4) || n4 > kMaxCollectionLen) return false;
  c.unresolvedConfounders.clear(); for (std::uint64_t i = 0; i < n4; ++i) { std::string s; if (!rd.str(s)) return false; c.unresolvedConfounders.push_back(std::move(s)); }
  std::uint64_t n5 = 0; if (!rd.u64(n5) || n5 > kMaxCollectionLen) return false;
  c.blockers.clear(); for (std::uint64_t i = 0; i < n5; ++i) { std::string s; if (!rd.str(s)) return false; c.blockers.push_back(std::move(s)); }
  std::uint64_t n6 = 0; if (!rd.u64(n6) || n6 > kMaxCollectionLen) return false;
  c.candidateClasses.clear(); for (std::uint64_t i = 0; i < n6; ++i) { if (!rd.u8(raw) || raw >= 21) return false; c.candidateClasses.push_back(static_cast<InterventionClass>(raw)); }
  std::uint64_t n7 = 0; if (!rd.u64(n7) || n7 > kMaxCollectionLen) return false;
  c.proposedIds.clear(); for (std::uint64_t i = 0; i < n7; ++i) { InterventionId x; if (!r_id(rd, x)) return false; c.proposedIds.push_back(x); }
  std::uint64_t n8 = 0; if (!rd.u64(n8) || n8 > kMaxCollectionLen) return false;
  c.selectedIds.clear(); for (std::uint64_t i = 0; i < n8; ++i) { InterventionId x; if (!r_id(rd, x)) return false; c.selectedIds.push_back(x); }
  if (!r_id(rd, c.policyGeneration) || !r_id(rd, c.sloGeneration) || !r_id(rd, c.valueGeneration)) return false;
  if (!r_id(rd, c.fairnessGeneration) || !r_id(rd, c.resourceGeneration) || !r_id(rd, c.placementGeneration)) return false;
  if (!r_id(rd, c.coordinatorEpoch) || !r_id(rd, c.workerBoot)) return false;
  if (!rd.u8(raw)) return false; c.groupEffectExplicit = raw != 0;
  if (!rd.u64(c.lastActionAt)) return false;
  if (!rd.u8(raw)) return false; c.inActionableState = raw != 0;
  return true;
}

bool deserialize_fairness(R& rd, FairnessRecord& f) {
  if (!r_id(rd, f.workload)) return false;
  if (!rd.u64(f.interventionCount)) return false;
  if (!rd.u32(f.consecutiveInterventions)) return false;
  if (!rd.f64(f.deprivationDebt)) return false;
  if (!rd.u64(f.lastInterventionAt)) return false;
  return true;
}

}  // namespace

Status load_from_bytes(const std::uint8_t* data, std::size_t len, GovernorState& s) {
  constexpr std::size_t kMagicLen = 8;
  constexpr std::size_t kVarHeaderLen = 4 + 8;  // version + payload length
  constexpr std::size_t kCrcLen = 4;
  const std::size_t payloadOff = kMagicLen + kVarHeaderLen;  // 20
  if (data == nullptr || len < payloadOff + kCrcLen)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "truncated persistence header");
  if (std::memcmp(data, kMagic, 8) != 0)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad magic");
  R h(data + kMagicLen, len - kMagicLen);
  std::uint32_t version = 0;
  std::uint64_t payloadLen = 0;
  if (!h.u32(version)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad version");
  if (version != kPersistenceVersion)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "unknown persistence version");
  if (!h.u64(payloadLen)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad payload length");

  const std::uint64_t total = payloadOff + payloadLen + kCrcLen;
  if (total > len)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "truncated payload");
  if (total != len)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "trailing garbage");

  std::uint32_t crc = 0;
  std::memcpy(&crc, data + len - kCrcLen, kCrcLen);  // little-endian
  std::uint32_t expected = crc32c(data + payloadOff, static_cast<std::size_t>(payloadLen));
  if (crc != expected) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "payload checksum mismatch");

  R rd(data + payloadOff, static_cast<std::size_t>(payloadLen));
  GovernorState out;
  if (!deserialize_policy(rd, out.engine.policy)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad policy");
  if (!deserialize_authority(rd, out.engine.authority)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad authority");
  std::uint64_t n = 0;
  if (!rd.u64(n) || n > kMaxCollectionLen) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad conflict count");
  for (std::uint64_t i = 0; i < n; ++i) { ConflictId id; ContentionConflict c; if (!r_id(rd, id)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad conflict id"); if (!deserialize_conflict(rd, c)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad conflict"); out.engine.conflicts[id] = c; }
  if (!rd.u64(n) || n > kMaxCollectionLen) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad evidence count");
  for (std::uint64_t i = 0; i < n; ++i) { InterferenceEvidenceId id; InterferenceEvidence e; if (!r_id(rd, id)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad evidence id"); if (!deserialize_evidence(rd, e)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad evidence"); out.engine.evidence[id] = e; }
  if (!rd.u64(n) || n > kMaxCollectionLen) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad intervention count");
  for (std::uint64_t i = 0; i < n; ++i) { InterventionId id; InterventionRecord ir; if (!r_id(rd, id)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad intervention id"); if (!deserialize_intervention(rd, ir)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad intervention"); out.engine.interventions[id] = ir; }
  if (!rd.u64(n) || n > kMaxCollectionLen) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad fairness count");
  for (std::uint64_t i = 0; i < n; ++i) { WorkloadId id; FairnessRecord f; if (!r_id(rd, id)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad fairness id"); if (!deserialize_fairness(rd, f)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad fairness"); out.engine.fairness[id] = f; }
  if (!rd.u64(out.nextConflictId) || !rd.u64(out.nextEvidenceId) || !rd.u64(out.nextInterventionId) ||
      !rd.u64(out.nextDispatchId) || !rd.u64(out.nextAttemptId) || !rd.u64(out.nextVerificationId)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad counters");
  if (!rd.u64(out.lastDigest)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "bad digest");
  if (!rd.done()) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "unconsumed payload bytes");
  if (validate_policy(out.engine.policy).code != StatusCode::OK)
    return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "invalid policy in persistence");
  s = std::move(out);
  return Status::good();
}

bool save_state(const std::string& path, const GovernorState& s, Status& st) {
  std::vector<std::uint8_t> bytes = save_to_bytes(s);
  std::string tmp = path + ".tmp";
  std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
  if (!out) {
    st = Status::bad(StatusCode::PERSISTENCE_CORRUPT, "cannot open temp file");
    return false;
  }
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  out.flush();
  if (!out.good()) {
    st = Status::bad(StatusCode::PERSISTENCE_CORRUPT, "write failed");
    return false;
  }
  out.close();

#if defined(_WIN32)
  if (std::remove(path.c_str()) != 0) {
    // Must not treat missing old file as failure.
  }
#endif
  std::error_code ec;
#if defined(_WIN32)
  std::rename(tmp.c_str(), path.c_str());
#else
  std::rename(tmp.c_str(), path.c_str());
#endif
  (void)ec;
  st = Status::good();
  return true;
}

Status load_state(const std::string& path, GovernorState& s) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "cannot open persistence file");
  in.seekg(0, std::ios::end);
  std::size_t size = static_cast<std::size_t>(in.tellg());
  in.seekg(0, std::ios::beg);
  if (size > (1u << 31)) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "persistence file too large");
  std::vector<std::uint8_t> buf(size);
  if (size > 0) in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
  if (!in.good()) return Status::bad(StatusCode::PERSISTENCE_CORRUPT, "read failed");
  return load_from_bytes(buf.data(), buf.size(), s);
}

}  // namespace contention
