#include "contention/cg.hpp"
#include <cstdio>
int main() {
  using namespace contention;
  std::printf("consumer version=%s\n", version());
  ManualClock clock(0);
  Governor g(Governor::Options{&clock, nullptr, false, ""});
  AuthorityContext auth;
  auth.coordinatorEpoch = CoordinatorEpoch{1};
  auth.policyGeneration = PolicyGeneration{1};
  auth.workloadGenerations[WorkloadId{1}] = WorkloadGeneration{1};
  auth.workloadGenerations[WorkloadId{2}] = WorkloadGeneration{1};
  auth.workloadBoots[WorkloadId{1}] = WorkerBootId{1};
  auth.workloadBoots[WorkloadId{2}] = WorkerBootId{1};
  g.set_authority(auth);
  ContentionPolicy pol;
  pol.id = PolicyId{1}; pol.generation = PolicyGeneration{1};
  WorkloadPolicy wa; wa.workload = WorkloadId{1}; wa.value = 100.0;
  WorkloadPolicy wb; wb.workload = WorkloadId{2}; wb.value = 1.0;
  pol.workloads[WorkloadId{1}] = wa;
  pol.workloads[WorkloadId{2}] = wb;
  g.set_policy(pol);
  InterferenceEvidence e;
  e.id = InterferenceEvidenceId{1}; e.generation = InterferenceEvidenceGeneration{1};
  e.victim = WorkloadId{1}; e.neighbors = {WorkloadId{2}};
  e.domain = ResourceDomain::COMPUTE; e.metric = MetricDirection::HIGHER_IS_BETTER;
  e.isolatedBaseline = 100.0; e.coRunObservation = 60.0;
  e.degradation = compute_degradation(e.metric, 100.0, 60.0);
  e.attribution = AttributionStrength::CONTROLLED_COMPARISON;
  e.confidence = 0.9; e.sampleCount = 100; e.status = EvidenceStatus::CURRENT; e.maxAgeMs = 5000;
  e.workerBoot = WorkerBootId{1}; e.workloadGeneration = WorkloadGeneration{1};
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  bool ok = !ids.empty();
  std::printf("consumer conflict_ids=%zu decision_present=%d\n", ids.size(),
              ok ? (int)g.evaluate(ids[0]).decision.has_value() : 0);
  return ok ? 0 : 1;
}
