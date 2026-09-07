// Shared helpers for the runnable examples.
#ifndef CONTENTION_EXAMPLE_HELPERS_HPP
#define CONTENTION_EXAMPLE_HELPERS_HPP

#include "contention/cg.hpp"
#include <cstdio>
#include <string>

namespace ex {

struct RefThrottle : contention::IThrottleAdapter {
  std::string name() const override { return "RefThrottle"; }
  contention::AdapterResult apply(contention::AttemptId, contention::WorkloadId,
                                  contention::InterventionClass, double) override {
    return {contention::AdapterOutcome::ACCEPTED, "applied", contention::AttemptId(0)};
  }
};
struct RefReg : contention::AdapterRegistry {
  RefThrottle thr;
  contention::IThrottleAdapter* throttle() override { return &thr; }
};

inline contention::InterferenceEvidence make_evidence(contention::WorkloadId victim,
                                                       contention::WorkloadId neighbor,
                                                       double baseline, double coRun,
                                                       contention::EvidenceStatus status =
                                                           contention::EvidenceStatus::CURRENT) {
  static std::uint64_t c = 1;
  contention::InterferenceEvidence e;
  e.id = contention::InterferenceEvidenceId(c++);
  e.generation = contention::InterferenceEvidenceGeneration{e.id.value()};
  e.victim = victim;
  e.neighbors = {neighbor};
  e.domain = contention::ResourceDomain::COMPUTE;
  e.metric = contention::MetricDirection::HIGHER_IS_BETTER;
  e.isolatedBaseline = baseline;
  e.coRunObservation = coRun;
  e.degradation = contention::compute_degradation(e.metric, baseline, coRun);
  e.attribution = contention::AttributionStrength::CONTROLLED_COMPARISON;
  e.confidence = 0.9;
  e.sampleCount = 100;
  e.status = status;
  e.maxAgeMs = 5000;
  e.label = contention::EvidenceLabel::CONTROLLED;
  e.sampledAt = 1000;
  e.activeUntil = 6000;
  return e;
}

inline void print_result(const std::string& label, int ok) {
  std::printf("%-32s %s\n", label.c_str(), ok ? "PASS" : "FAIL");
}

inline contention::Governor make_gov(contention::ManualClock& clock, RefReg& reg,
                                     double valueA = 100.0, double valueB = 1.0,
                                     bool bPreemptible = true) {
  contention::Governor g(contention::Governor::Options{&clock, &reg, false, ""});
  contention::AuthorityContext auth;
  auth.coordinatorEpoch = contention::CoordinatorEpoch{1};
  auth.policyGeneration = contention::PolicyGeneration{1};
  auth.workloadGenerations[contention::WorkloadId{1}] = contention::WorkloadGeneration{1};
  auth.workloadGenerations[contention::WorkloadId{2}] = contention::WorkloadGeneration{1};
  auth.workloadBoots[contention::WorkloadId{1}] = contention::WorkerBootId{1};
  auth.workloadBoots[contention::WorkloadId{2}] = contention::WorkerBootId{1};
  g.set_authority(auth);
  contention::ContentionPolicy pol;
  pol.id = contention::PolicyId{1}; pol.generation = contention::PolicyGeneration{1};
  contention::WorkloadPolicy wa; wa.workload = contention::WorkloadId{1}; wa.value = valueA;
  contention::WorkloadPolicy wb; wb.workload = contention::WorkloadId{2}; wb.value = valueB;
  wb.nonPreemptible = !bPreemptible; wb.preemptionAllowed = bPreemptible;
  pol.workloads[contention::WorkloadId{1}] = wa;
  pol.workloads[contention::WorkloadId{2}] = wb;
  g.set_policy(pol);
  return g;
}

}  // namespace ex

#endif
