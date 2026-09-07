#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg, 100.0, 1.0, false); // B non-preemptible
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::REQUEST_PREEMPTION);
  bool ok = !st.ok(); // hard constraint must reject preemption of non-preemptible
  ex::print_result("hard_slo_non_preemptible_rejected", ok);
  return ok ? 0 : 1;
}
