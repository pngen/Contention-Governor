#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0, EvidenceStatus::STALE);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  bool ok = !st.ok();
  ex::print_result("stale_evidence_no_destructive", ok);
  return ok ? 0 : 1;
}
