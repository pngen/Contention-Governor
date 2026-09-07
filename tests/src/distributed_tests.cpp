// Distributed multiprocess proof: real OS processes + real framed TCP loopback.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"
#include "protocol.hpp"

#include <atomic>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

using namespace contention;

// An in-process coordinator TCP throttle adapter (sends ACTION_AUTHORIZE and
// waits for the worker's ACK over real TCP).
struct TcpAdapter : contention::AdapterRegistry {
  std::mutex mu;
  std::map<WorkloadId, std::unique_ptr<Connection>>* conns {nullptr};

  struct Throttle : contention::IThrottleAdapter {
    TcpAdapter* owner {nullptr};
    std::string name() const override { return "TcpThrottle"; }
    contention::AdapterResult apply(contention::AttemptId attempt, contention::WorkloadId target,
                                    contention::InterventionClass cls, double intensity) override {
      Connection* conn = nullptr;
      { std::lock_guard<std::mutex> g(owner->mu); auto it = owner->conns->find(target); if (it != owner->conns->end()) conn = it->second.get(); }
      if (!conn) return {contention::AdapterOutcome::UNAVAILABLE, "no worker", attempt};
      dist::WireAction a; a.id = InterventionId{1}; a.cls = cls; a.target = target; a.attempt = attempt; a.intensity = intensity;
      auto p = dist::encode_action(a);
      if (!conn->send(contention::MsgType::ACTION_AUTHORIZE, p.data(), p.size()).ok()) return {contention::AdapterOutcome::UNKNOWN, "send", attempt};
      Frame f; Status st = conn->receive(f, true);
      if (!st.ok()) return {contention::AdapterOutcome::UNKNOWN, "no ack", attempt};
      dist::WireAck ack; if (!dist::decode_ack(f.payload.data(), f.payload.size(), ack)) return {contention::AdapterOutcome::UNKNOWN, "bad ack", attempt};
      return {ack.acknowledged ? contention::AdapterOutcome::ACCEPTED : contention::AdapterOutcome::REJECTED, ack.note, attempt};
    }
  } adapter;
  contention::IThrottleAdapter* throttle() override { adapter.owner = this; return &adapter; }
};

std::string exe_dir() {
#if defined(_WIN32)
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string p(buf);
  auto slash = p.find_last_of("\\/");
  return p.substr(0, slash);
#else
  return ".";
#endif
}

void spawn_proc(const std::string& exe, const std::string& args) {
#if defined(_WIN32)
  std::string cmdline = "\"" + exe + "\" " + args;
  std::vector<char> cmd(cmdline.begin(), cmdline.end());
  cmd.push_back('\0');
  STARTUPINFOA si {};
  PROCESS_INFORMATION pi {};
  si.cb = sizeof(si);
  if (CreateProcessA(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                     &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
#else
  std::thread([cmdline = ("\"" + exe + "\" " + args + " &")]() { std::system(cmdline.c_str()); }).detach();
#endif
}

bool file_exists(const std::string& p) {
  std::FILE* f = std::fopen(p.c_str(), "rb");
  if (f) { std::fclose(f); return true; }
  return false;
}

std::string worker_path() {
  std::string dir = exe_dir();
  std::vector<std::string> cands = {
      dir + "\\cg_dist_worker.exe",
      dir + "\\..\\..\\dist\\Release\\cg_dist_worker.exe",
      dir + "\\..\\dist\\cg_dist_worker.exe" };
  for (auto& c : cands) if (file_exists(c)) return c;
  return cands[0];
}

void spawn_worker(std::uint16_t port, const std::string& role, std::uint64_t workerId,
                  std::uint64_t boot, double baseline, double corun, double postCorun,
                  bool dieAfterAck, const std::string& extra = "") {
  std::string args = "--port " + std::to_string(port) +
                    " --role " + role + " --workerid " + std::to_string(workerId) +
                    " --boot " + std::to_string(boot) + " --victim 1 --neighbor 2" +
                    " --baseline " + std::to_string(baseline) + " --corun " + std::to_string(corun) +
                    " --post-corun " + std::to_string(postCorun) +
                    (dieAfterAck ? " --die-after-ack" : "") + " " + extra;
  spawn_proc(worker_path(), args);
}

Governor make_coordinator(ManualClock& clock, TcpAdapter& ad, std::map<WorkloadId, std::unique_ptr<Connection>>& conns) {
  Governor g(Governor::Options{&clock, &ad, false, ""});
  ad.conns = &conns;
  g.set_authority(ctest::make_authority(WorkloadId{1}, WorkloadId{2}));
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  return g;
}

}  // namespace

TEST(dist_scenario_A_basic_resolution) {
  using namespace contention;
  socket_init();
  Listener listener;
  CHECK(listener.bind_and_listen(0).ok());
  auto port = listener.bound_port();
  std::map<WorkloadId, std::unique_ptr<Connection>> conns;
  ManualClock clock(0);
  TcpAdapter adapter;
  Governor g = make_coordinator(clock, adapter, conns);

  spawn_worker(port, "A", 1, 1, 100.0, 60.0, 98.0, false);
  spawn_worker(port, "B", 2, 1, 100.0, 60.0, 98.0, true);

  // Accept two workers and feed their subscriptions.
  for (int i = 0; i < 2; ++i) {
    auto c = listener.accept(30000);
    CHECK(c != nullptr);
    Frame f; CHECK(c->receive(f, true).ok() && f.type == MsgType::WORKER_STATUS);
    dist::WorkerHello hello; CHECK(dist::decode_hello(f.payload.data(), f.payload.size(), hello));
    WorkloadId wl{hello.worker.value()};
    conns[wl] = std::move(c);
    g.record_worker_boot(wl, hello.boot);
    g.record_workload_generation(wl, hello.workloadGen);
    Frame ev; CHECK(conns[wl]->receive(ev, true).ok() && ev.type == MsgType::EVIDENCE);
    InterferenceEvidence e; CHECK(dist::decode_evidence(ev.payload.data(), ev.payload.size(), e));
    CHECK(g.ingest_evidence(e).ok());
  }
  auto ids = g.conflict_ids();
  CHECK(ids.size() == 1);
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.ok());
  // Ask worker A to re-measure.
  auto vit = conns.find(WorkloadId{1});
  CHECK(vit != conns.end());
  std::uint8_t cmd = 1;
  vit->second->send(MsgType::RPC_REQUEST, &cmd, 1);
  Frame pe; CHECK(vit->second->receive(pe, true).ok() && pe.type == MsgType::EVIDENCE);
  InterferenceEvidence post; CHECK(dist::decode_evidence(pe.payload.data(), pe.payload.size(), post));
  CHECK(g.submit_post_action_evidence(post).ok());
  auto c = g.conflict(ids[0]);
  CHECK(c.has_value() && c->degradation < ctest::make_policy().actionabilityThreshold);
  socket_shutdown();
}

TEST(dist_scenario_D_stale_evidence_rejects) {
  using namespace contention;
  socket_init();
  Listener listener;
  CHECK(listener.bind_and_listen(0).ok());
  auto port = listener.bound_port();
  std::map<WorkloadId, std::unique_ptr<Connection>> conns;
  ManualClock clock(0);
  TcpAdapter adapter;
  Governor g = make_coordinator(clock, adapter, conns);
  // Spawn a worker, then advance its boot before dispatch.
  spawn_worker(port, "B", 2, 1, 100.0, 60.0, 98.0, true);
  auto c = listener.accept(30000);
  CHECK(c != nullptr);
  Frame f; CHECK(c->receive(f, true).ok());
  dist::WorkerHello hello; CHECK(dist::decode_hello(f.payload.data(), f.payload.size(), hello));
  WorkloadId wl{hello.worker.value()};
  conns[wl] = std::move(c);
  g.record_worker_boot(wl, hello.boot);
  Frame ev; CHECK(conns[wl]->receive(ev, true).ok());
  InterferenceEvidence e; CHECK(dist::decode_evidence(ev.payload.data(), ev.payload.size(), e));
  CHECK(g.ingest_evidence(e).ok());
  auto ids = g.conflict_ids();
  CHECK(ids.size() == 1);
  // Authorize while the worker's boot is current.
  auto interId = g.authorize(ids[0], InterventionClass::THROTTLE);
  CHECK(interId.has_value());
  // THEN the worker dies/restarts with a fresh boot identity before dispatch.
  g.record_worker_boot(WorkloadId{2}, WorkerBootId{77});
  Status st = g.dispatch(*interId);
  CHECK(st.code == StatusCode::STALE_AUTHORITY);
  socket_shutdown();
}
