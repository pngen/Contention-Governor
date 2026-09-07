// Concurrency stress: real multithreaded evidence/policy/action paths.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace {
using namespace contention;

Governor concurrency_governor(ManualClock& clock, ctest::RefRegistry& reg) {
  Governor g(ctest::make_governor(clock, reg));
  g.set_authority(ctest::make_authority(WorkloadId{1}, WorkloadId{2}));
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  ctest::add_workload(pol, WorkloadId{3}, 1.0);
  g.set_policy(pol);
  return g;
}
}  // namespace

TEST(concurrency_stress_no_deadlock) {
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = concurrency_governor(clock, reg);

  constexpr int kThreads = 8;
  constexpr int kOps = 500;
  std::vector<std::thread> threads;
  std::atomic<int> errors{0};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, t]() {
      for (int k = 0; k < kOps; ++k) {
        WorkloadId a{static_cast<std::uint64_t>(1 + (k % 3))};
        WorkloadId b{static_cast<std::uint64_t>(2 + ((k + t) % 2))};
        auto e = ctest::make_evidence(a, b, ResourceDomain::COMPUTE,
                                      100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
        if (!g.ingest_evidence(e).ok()) ++errors;
        auto ids = g.conflict_ids();
        if (!ids.empty()) {
          g.evaluate(ids[0]);
        }
        if (t == 0 || t == 1) {
          if (!ids.empty()) {
            auto st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
            if (st.code == StatusCode::ACTION_BUDGET_EXHAUSTED) {
              // fine
            }
          }
        }
        if (t == 7) {
          (void)g.digest();
          (void)g.action_budget();
        }
      }
    });
  }
  for (auto& th : threads) th.join();
  CHECK(g.conflict_ids().size() >= 1);
  CHECK(errors == 0);
}

TEST(concurrency_exact_accounting) {
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = concurrency_governor(clock, reg);
  // Single-threaded accounting must be exact: budget counts equal number of
  // successful dispatches.
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  int ok = 0;
  for (int i = 0; i < 30; ++i) {
    Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
    if (st.ok()) ++ok;
  }
  auto ab = g.action_budget();
  CHECK(ab.used.throttles == ok);
}
