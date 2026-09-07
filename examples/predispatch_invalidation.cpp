#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  auto interId = g.authorize(ids[0], InterventionClass::THROTTLE);
  if (!interId) return 1;
  // Worker B restarts -> new boot before dispatch.
  g.record_worker_boot(WorkloadId{2}, WorkerBootId{555});
  Status st = g.dispatch(*interId);
  bool ok = st.code == StatusCode::STALE_AUTHORITY;
  ex::print_result("predispatch_invalidation", ok);
  return ok ? 0 : 1;
}
