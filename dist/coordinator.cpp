// Contention Governor -- distributed proof coordinator process.
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "protocol.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>

using namespace contention;

namespace {

struct TcpAdapters : contention::AdapterRegistry {
  std::mutex mu;
  std::map<WorkloadId, std::unique_ptr<Connection>>* conns {nullptr};

  struct Throttle : contention::IThrottleAdapter {
    TcpAdapters* owner {nullptr};
    std::string name() const override { return "TcpThrottle"; }
    contention::AdapterResult apply(contention::AttemptId attempt, contention::WorkloadId target,
                                    contention::InterventionClass cls, double intensity) override {
      Connection* conn = nullptr;
      {
        std::lock_guard<std::mutex> lock(owner->mu);
        auto it = owner->conns->find(target);
        if (it != owner->conns->end()) conn = it->second.get();
      }
      if (!conn) return {contention::AdapterOutcome::UNAVAILABLE, "no worker for target", attempt};
      dist::WireAction a;
      a.id = InterventionId{1};
      a.cls = cls; a.target = target; a.attempt = attempt; a.intensity = intensity;
      auto p = dist::encode_action(a);
      if (!conn->send(contention::MsgType::ACTION_AUTHORIZE, p.data(), p.size()).ok())
        return {contention::AdapterOutcome::UNKNOWN, "send failed", attempt};
      Frame f;
      Status st = conn->receive(f, true);
      if (!st.ok()) return {contention::AdapterOutcome::UNKNOWN, "no ack", attempt};
      dist::WireAck ack;
      if (!dist::decode_ack(f.payload.data(), f.payload.size(), ack))
        return {contention::AdapterOutcome::UNKNOWN, "bad ack", attempt};
      return {ack.acknowledged ? contention::AdapterOutcome::ACCEPTED
                               : contention::AdapterOutcome::REJECTED,
              ack.note, attempt};
    }
  } adapter;

  contention::IThrottleAdapter* throttle() override { adapter.owner = this; return &adapter; }
};

}  // namespace

int main(int argc, char** argv) {
  std::string mode = "basic";
  std::string persistPath;
  std::uint16_t port = 0;
  std::string readyFile;
  for (int i = 1; i < argc - 1; i += 2) {
    std::string k = argv[i], v = argv[i + 1];
    if (k == "--mode") mode = v;
    else if (k == "--persist") persistPath = v;
    else if (k == "--port") port = static_cast<std::uint16_t>(std::atoi(v.c_str()));
    else if (k == "--ready") readyFile = v;
  }

  Listener listener;
  Status st = listener.bind_and_listen(port);
  if (!st.ok()) { std::fprintf(stderr, "coordinator bind failed\n"); return 2; }
  std::uint16_t actual = listener.bound_port();
  if (!readyFile.empty()) {
    std::FILE* f = std::fopen(readyFile.c_str(), "w");
    if (f) { std::fprintf(f, "%u\n", actual); std::fclose(f); }
  }

  socket_init();
  std::map<WorkloadId, std::unique_ptr<Connection>> conns;
  TcpAdapters adapters;
  adapters.conns = &conns;

  Governor g(Governor::Options{nullptr, &adapters, !persistPath.empty(), persistPath});
  if (mode == "resume") {
    Status ls = g.load();
    if (!ls.ok()) { std::fprintf(stderr, "coordinator load failed\n"); return 3; }
    g.advance_epoch();
  }

  for (int i = 0; i < 2; ++i) {
    auto c = listener.accept(30000);
    if (!c) { std::fprintf(stderr, "coordinator accept timeout\n"); return 4; }
    Frame f;
    Status rs = c->receive(f, true);
    if (!rs.ok() || f.type != MsgType::WORKER_STATUS) return 5;
    dist::WorkerHello hello;
    if (!dist::decode_hello(f.payload.data(), f.payload.size(), hello)) return 6;
    WorkloadId wl{hello.worker.value()};
    conns[wl] = std::move(c);
    g.record_worker_boot(wl, hello.boot);
    g.record_workload_generation(wl, hello.workloadGen);
    Frame ev;
    Status es = conns[wl]->receive(ev, true);
    if (!es.ok() || ev.type != MsgType::EVIDENCE) return 7;
    InterferenceEvidence e;
    if (!dist::decode_evidence(ev.payload.data(), ev.payload.size(), e)) return 8;
    g.ingest_evidence(e);
  }

  auto ids = g.conflict_ids();
  if (ids.empty()) { std::fprintf(stderr, "no conflicts\n"); return 9; }

  auto interId = g.authorize(ids[0], InterventionClass::THROTTLE);
  if (!interId) {
    std::fprintf(stdout, "NO_LEGAL_INTERVENTION\n"); std::fflush(stdout);
    return 0;
  }
  Status ds = g.dispatch(*interId);
  if (ds.code != StatusCode::OK) {
    std::fprintf(stdout, "DISPATCH_REJECTED %s\n", to_string(ds.code)); std::fflush(stdout);
    return 0;
  }

  if (mode == "persist") {
    g.save();
    std::fprintf(stdout, "PERSISTED %llu\n", static_cast<unsigned long long>(interId->value()));
    std::fflush(stdout);
    while (true) { Frame f; if (conns.empty() || !conns.begin()->second->receive(f, false).ok()) break; }
    return 0;
  }

  if (mode == "resume") {
    Status stale = g.dispatch(*interId);
    std::fprintf(stdout, "RESUME_DISPATCH %s\n", to_string(stale.code));
    std::fflush(stdout);
    return 0;
  }

  WorkloadId victim = g.conflict(ids[0])->affected.back();
  auto vit = conns.find(victim);
  if (vit != conns.end()) {
    std::uint8_t cmd = 1;
    vit->second->send(MsgType::RPC_REQUEST, &cmd, 1);
    Frame pe;
    Status ps = vit->second->receive(pe, true);
    if (ps.ok() && pe.type == MsgType::EVIDENCE) {
      InterferenceEvidence post;
      if (dist::decode_evidence(pe.payload.data(), pe.payload.size(), post)) {
        g.submit_post_action_evidence(post);
      }
    }
  }
  auto conf = g.conflict(ids[0]);
  std::fprintf(stdout, "OK %s\n", conf ? to_string(conf->state) : "missing");
  std::fflush(stdout);
  return 0;
}
