#include "eg.hpp"
#include <filesystem>
using namespace contention;
int main() {
  auto dir = std::filesystem::temp_directory_path() / "cg_example_restart";
  std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
  std::string path = (dir / "state.bin").string();
  {
    ManualClock clock(0); ex::RefReg reg;
    Governor g(Governor::Options{&clock, &reg, true, path});
    g.set_authority(ex::make_gov(clock, reg).snapshot()->engine.authority);
    auto e = ex::make_evidence(WorkloadId{1}, WorkloadId{2}, 100.0, 60.0);
    g.ingest_evidence(e);
    g.save();
  }
  {
    ManualClock clock(0); ex::RefReg reg;
    Governor g(Governor::Options{&clock, &reg, true, path});
    g.load();
    g.advance_epoch();
    bool ok = g.conflict_ids().size() == 1;
    ex::print_result("coordinator_restart_makes_revalidation", ok);
    std::filesystem::remove_all(dir);
    return ok ? 0 : 1;
  }
}
