// Test utilities for constructing evidence/policy/authority.
#ifndef CONTENTION_TESTUTIL_HPP
#define CONTENTION_TESTUTIL_HPP

#include "contention/cg.hpp"

#include <cstdint>

namespace ctest {

inline contention::InterferenceEvidence make_evidence(
    contention::WorkloadId victim, contention::WorkloadId neighbor,
    contention::ResourceDomain domain, double baseline, double coRun,
    contention::AttributionStrength attr, double confidence,
    contention::EvidenceStatus status = contention::EvidenceStatus::CURRENT,
    bool overCommit = false) {
  (void)overCommit;
  static std::uint64_t counter = 1;
  contention::InterferenceEvidence e;
  e.id = contention::InterferenceEvidenceId(counter++);
  e.generation = contention::InterferenceEvidenceGeneration(e.id.value());
  e.victim = victim;
  e.neighbors = {neighbor};
  e.domain = domain;
  e.metric = contention::MetricDirection::HIGHER_IS_BETTER;
  e.isolatedBaseline = baseline;
  e.coRunObservation = coRun;
  e.degradation = contention::compute_degradation(e.metric, baseline, coRun);
  e.attribution = attr;
  e.confidence = confidence;
  e.sampleCount = 100;
  e.status = status;
  e.maxAgeMs = 5000;
  e.label = contention::EvidenceLabel::CONTROLLED;
  e.sampledAt = 1000;
  e.activeUntil = 6000;
  return e;
}

inline contention::ContentionPolicy make_policy(
    contention::PolicyGeneration gen = contention::PolicyGeneration{1}) {
  contention::ContentionPolicy p;
  p.id = contention::PolicyId{1};
  p.generation = gen;
  p.actionabilityThreshold = 0.20;
  p.releaseThreshold = 0.10;
  p.hysteresisEvidenceCount = 2;
  p.minAttributionForDisruptive = contention::AttributionStrength::CONTROLLED_COMPARISON;
  p.minConfidenceForDisruptive = 0.6;
  p.fairnessFloor = 0.10;
  p.maxConsecutiveInterventionsPerWorkload = 3;
  p.cooldownMs = 2000;
  p.throttleAllowed = true;
  p.preemptionAllowed = true;
  p.migrationAllowed = true;
  p.placementChangeAllowed = true;
  p.bandwidthRequestAllowed = true;
  p.collectiveSchedulingAllowed = true;
  p.admissionRestrictionAllowed = true;
  p.budget.maxPreemptionsPerWindow = 5;
  p.budget.maxMigrationsPerWindow = 5;
  p.budget.maxPlacementChangesPerWindow = 5;
  p.budget.maxThrottlesPerWindow = 50;
  p.budget.maxCollectiveReschedulesPerWindow = 5;
  p.budget.maxAnyPerWindow = 100;
  p.budget.windowMs = 60000;
  return p;
}

inline void add_workload(contention::ContentionPolicy& p, contention::WorkloadId id,
                         double value, bool preemptible = true,
                         double hardSloFloor = 0.0) {
  contention::WorkloadPolicy wp;
  wp.workload = id;
  wp.generation = p.generation;
  wp.value = value;
  wp.nonPreemptible = !preemptible;
  wp.hardSloFloor = hardSloFloor;
  wp.preemptionAllowed = preemptible;
  wp.throttlingAllowed = true;
  wp.migrationAllowed = true;
  p.workloads[id] = wp;
}

inline contention::AuthorityContext make_authority(contention::WorkloadId a, contention::WorkloadId b,
                                                   contention::CoordinatorEpoch epoch = contention::CoordinatorEpoch{1}) {
  contention::AuthorityContext ctx;
  ctx.coordinatorEpoch = epoch;
  ctx.governorGeneration = contention::GovernorGeneration{1};
  ctx.policyGeneration = contention::PolicyGeneration{1};
  ctx.sloGeneration = contention::SLOGeneration{1};
  ctx.valueGeneration = contention::ValueGeneration{1};
  ctx.fairnessGeneration = contention::FairnessGeneration{1};
  ctx.resourceGeneration = contention::ResourceGeneration{1};
  ctx.placementGeneration = contention::PlacementGeneration{1};
  ctx.topologyGeneration = contention::TopologyGeneration{1};
  ctx.workloadGenerations[a] = contention::WorkloadGeneration{1};
  ctx.workloadGenerations[b] = contention::WorkloadGeneration{1};
  ctx.workloadBoots[a] = contention::WorkerBootId{1};
  ctx.workloadBoots[b] = contention::WorkerBootId{1};
  return ctx;
}


// A reference throttle adapter that accepts intents against controlled workers.
struct RefThrottle : contention::IThrottleAdapter {
  std::string name() const override { return "RefThrottle"; }
  contention::AdapterResult apply(contention::AttemptId, contention::WorkloadId,
                                  contention::InterventionClass, double) override {
    return {contention::AdapterOutcome::ACCEPTED, "applied", contention::AttemptId(0)};
  }
};

struct RefRegistry : contention::AdapterRegistry {
  RefThrottle thr;
  contention::IThrottleAdapter* throttle() override { return &thr; }
};

inline contention::Governor make_governor(contention::ManualClock& clock, RefRegistry& reg) {
  return contention::Governor(contention::Governor::Options{&clock, &reg, false, ""});
}

}  // namespace ctest

#endif
