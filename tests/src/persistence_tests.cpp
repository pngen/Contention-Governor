// Persistence / replay / digest integrity tests.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {
using namespace contention;

std::filesystem::path td() {
  return std::filesystem::temp_directory_path() / "cg_persist_test";
}

Governor build_state() {
  ManualClock* clock = new ManualClock(0);
  ctest::RefRegistry reg;
  Governor g(Governor::Options{clock, &reg, false, ""});
  g.set_authority(ctest::make_authority(WorkloadId{1}, WorkloadId{2}));
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  return g;
}
}  // namespace

TEST(persistence_roundtrip) {
  using namespace contention;
  GovernorState st;
  {
    Governor g = build_state();
    std::vector<std::uint8_t> bytes = /* need access */ save_to_bytes(*g.snapshot());
    Status ls = load_from_bytes(bytes.data(), bytes.size(), st);
    CHECK(ls.ok());
  }
  CHECK(st.engine.conflicts.size() == 1);
  CHECK(st.engine.evidence.size() == 1);
}

TEST(persistence_corruption_rejected) {
  using namespace contention;
  Governor g = build_state();
  std::vector<std::uint8_t> bytes = save_to_bytes(*g.snapshot());
  std::uint8_t* payload = bytes.data() + 20;  // payload start
  payload[5] ^= 0xFF;  // corrupt a byte
  GovernorState st;
  Status ls = load_from_bytes(bytes.data(), bytes.size(), st);
  CHECK(ls.code == StatusCode::PERSISTENCE_CORRUPT);
}

TEST(persistence_truncation_rejected) {
  using namespace contention;
  Governor g = build_state();
  std::vector<std::uint8_t> bytes = save_to_bytes(*g.snapshot());
  GovernorState st;
  Status ls = load_from_bytes(bytes.data(), bytes.size() - 3, st);
  CHECK(ls.code == StatusCode::PERSISTENCE_CORRUPT);
}

TEST(persistence_trailing_garbage_rejected) {
  using namespace contention;
  Governor g = build_state();
  std::vector<std::uint8_t> bytes = save_to_bytes(*g.snapshot());
  bytes.push_back(0xAB);
  GovernorState st;
  Status ls = load_from_bytes(bytes.data(), bytes.size(), st);
  CHECK(ls.code == StatusCode::PERSISTENCE_CORRUPT);
}

TEST(persistence_unknown_version_rejected) {
  using namespace contention;
  Governor g = build_state();
  std::vector<std::uint8_t> bytes = save_to_bytes(*g.snapshot());
  bytes[8] = 0xFF;  // corrupt version field
  GovernorState st;
  Status ls = load_from_bytes(bytes.data(), bytes.size(), st);
  CHECK(ls.code == StatusCode::PERSISTENCE_CORRUPT);
}

TEST(replay_is_deterministic) {
  using namespace contention;
  Governor g = build_state();
  GovernorState st = *g.snapshot();
  ReplayReport rp = replay(st);
  CHECK(rp.status.ok());
  CHECK(rp.conflictsEvaluated == 1);
}

TEST(persistence_file_roundtrip) {
  using namespace contention;
  auto dir = td();
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  std::string path = (dir / "state.bin").string();
  {
    Governor inner;
    auto* clock = new ManualClock(0);
    ctest::RefRegistry reg;
    // Build a governor whose persist path is set.
    Governor g(Governor::Options{clock, &reg, true, path});
    g.set_authority(ctest::make_authority(WorkloadId{1}, WorkloadId{2}));
    auto pol = ctest::make_policy();
    ctest::add_workload(pol, WorkloadId{1}, 100.0);
    ctest::add_workload(pol, WorkloadId{2}, 1.0);
    g.set_policy(pol);
    auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                  100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
    g.ingest_evidence(e);
    CHECK(g.save().ok());
  }
  // Reload in a fresh governor.
  {
    auto* clock = new ManualClock(0);
    ctest::RefRegistry reg;
    Governor g(Governor::Options{clock, &reg, true, path});
    Status st = g.load();
    CHECK(st.ok());
    CHECK(g.conflict_ids().size() == 1);
  }
  std::filesystem::remove_all(dir);
}
