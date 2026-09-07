// Property / invariant tests.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"

#include <random>

namespace {

using namespace contention;

bool run_randomized_case(std::mt19937& rng, ManualClock& clock, ctest::RefRegistry& reg) {
  std::uniform_real_distribution<double> conf(0.0, 1.0);
  std::uniform_int_distribution<int> degLen(20, 80);
  int loss = degLen(rng);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  Governor g(Governor::Options{&clock, &reg, false, ""});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  double c = conf(rng);
  double co = 100.0 * (100.0 - loss) / 100.0;
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, co, AttributionStrength::CONTROLLED_COMPARISON, c);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  if (ids.empty()) return true;
  ConflictEvaluation ev = g.evaluate(ids[0]);
  if (c < pol.minConfidenceForDisruptive) {
    if (ev.decision) return false;
  }
  return true;
}

}  // namespace

TEST(property_no_action_for_low_confidence) {
  std::mt19937 rng(12345);
  ManualClock clock(0);
  ctest::RefRegistry reg;
  for (int i = 0; i < 200; ++i) {
    if (!run_randomized_case(rng, clock, reg)) {
      CHECK(false);
      return;
    }
  }
  CHECK(true);
}

TEST(property_budget_never_underflows) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g(ctest::make_governor(clock, reg));
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  for (int i = 0; i < 200; ++i) {
    Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
    if (st.code == StatusCode::ACTION_BUDGET_EXHAUSTED) break;
  }
  auto ab = g.action_budget();
  CHECK(ab.used.all <= ab.maxAnyPerWindow);
}

TEST(property_terminal_lifecycle_stays_terminal) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g(ctest::make_governor(clock, reg));
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.ok());
  auto c = g.conflict(ids[0]);
  CHECK(c.has_value() && !c->selectedIds.empty());
  auto iv = g.intervention(c->selectedIds[0]);
  g.on_action_failed(c->selectedIds[0], iv->attemptId, "test");
  iv = g.intervention(c->selectedIds[0]);
  CHECK(iv->state == LifecycleState::FAILED);
  Status ack = g.on_acknowledge(c->selectedIds[0], iv->attemptId);
  CHECK(ack.code != StatusCode::OK);
  iv = g.intervention(c->selectedIds[0]);
  CHECK(iv->state == LifecycleState::FAILED);
}

TEST(property_identical_state_same_digest) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g(ctest::make_governor(clock, reg));
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  Digest64 d1 = g.digest();
  Digest64 d2 = g.digest();
  CHECK(d1 == d2);
}

TEST(property_unknown_does_not_become_false_certainty) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g(ctest::make_governor(clock, reg));
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::TEMPORALLY_ASSOCIATED, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  ConflictEvaluation ev = g.evaluate(ids[0]);
  CHECK(!ev.decision.has_value());
}
