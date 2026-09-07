#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  // Post-action evidence shows NO improvement (still degraded).
  auto pe = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.submit_post_action_evidence(pe);
  auto c = g.conflict(ids[0]);
  bool ok = c && c->degradation >= 0.199;
  ex::print_result("ineffective_action_detected", ok);
  return ok ? 0 : 1;
}
