#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg, 100.0, 1.0);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  ConflictEvaluation ev = g.evaluate(ids[0]);
  bool ok = ev.decision.has_value() && ev.decision->chosen.target == WorkloadId{2};
  ex::print_result("value_based_targets_lower_value", ok);
  return ok ? 0 : 1;
}
