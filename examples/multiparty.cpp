#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  e.neighbors = {WorkloadId{2}, WorkloadId{3}};
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  ConflictEvaluation ev = g.evaluate(ids[0]);
  bool ok = !ev.candidates.empty();
  ex::print_result("multiparty_representable", ok);
  return ok ? 0 : 1;
}
