#include "eg.hpp"
using namespace contention;
int main() {
  ManualClock clock(0); ex::RefReg reg;
  Governor g = ex::make_gov(clock, reg);
  auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  bool ok = false;
  for (int i = 0; i < 30; ++i) {
    Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
    if (st.code == StatusCode::FAIRNESS_LIMIT || st.code == StatusCode::ACTION_BUDGET_EXHAUSTED ||
        st.code == StatusCode::NO_LEGAL_INTERVENTION) { ok = true; break; }
  }
  ex::print_result("fairness_limits_repeated_sacrifice", ok);
  return ok ? 0 : 1;
}
