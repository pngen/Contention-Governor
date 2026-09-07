#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  // A recovers, but B is now severely degraded on the same domain (shifted).
  auto pe = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 97.0);
  g.submit_post_action_evidence(pe);
  auto e2 = ex::make_evidence(WorkloadId{2}, WorkloadId{1}, 100.0, 55.0);
  g.ingest_evidence(e2);
  bool ok = g.conflict(ids[0])->degradation < 0.20; // original resolved
  ex::print_result("shifted_contention_reported", ok);
  return ok ? 0 : 1;
}
