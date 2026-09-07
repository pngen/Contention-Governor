#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0);
  ex::RefReg reg;
  Governor g(Governor::Options{&clock, &reg, false, ""});
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
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  ConflictEvaluation ev = g.evaluate(ids[0]);
  bool ok = ev.decision.has_value();
  ex::print_result("basic_pairwise_resolution", ok ? 1 : 0);
  return ok ? 0 : 1;
}
