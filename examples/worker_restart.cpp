#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  // Authorize while the worker's boot is current, THEN the worker restarts.
  auto interId = g.authorize(ids[0], InterventionClass::THROTTLE);
  if (!interId) return 1;
  g.record_worker_boot(WorkloadId{2}, WorkerBootId{777});
  Status st = g.dispatch(*interId);
  bool ok = st.code == StatusCode::STALE_AUTHORITY;
  ex::print_result("worker_restart_fences_stale", ok);
  return ok ? 0 : 1;
}
