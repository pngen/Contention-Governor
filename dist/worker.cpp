// Contention Governor -- distributed proof worker process.
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "protocol.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
  std::uint16_t port = 0;
  std::string role = "A";
  std::uint64_t boot = 1, workerId = 0, victim = 0, neighbor = 0;
  std::uint64_t sample = 0;
  double baseline = 100.0, corun = 60.0, postCorun = 98.0;
  bool dieAfterAck = false;

  for (int i = 1; i < argc - 1; i += 2) {
    std::string k = argv[i];
    std::string v = argv[i + 1];
    if (k == "--port") port = static_cast<std::uint16_t>(std::atoi(v.c_str()));
    else if (k == "--role") role = v;
    else if (k == "--boot") boot = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--workerid") workerId = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--victim") victim = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--neighbor") neighbor = std::strtoull(v.c_str(), nullptr, 10);
    else if (k == "--baseline") baseline = std::atof(v.c_str());
    else if (k == "--corun") corun = std::atof(v.c_str());
    else if (k == "--post-corun") postCorun = std::atof(v.c_str());
    else if (k == "--die-after-ack") dieAfterAck = true;
  }
  if (port == 0 || workerId == 0) { std::fprintf(stderr, "worker needs --port --workerid\n"); return 2; }

  contention::socket_init();
  auto conn = contention::connect_to("127.0.0.1", port);
  if (!conn) { std::fprintf(stderr, "worker connect failed\n"); return 1; }

  dist::WorkerHello hello;
  hello.worker = contention::WorkerId{workerId};
  hello.boot = contention::WorkerBootId{boot};
  hello.workloadGen = contention::WorkloadGeneration{1};
  hello.role = role;
  auto hp = dist::encode_hello(hello);
  if (!conn->send(contention::MsgType::WORKER_STATUS, hp.data(), hp.size()).ok()) return 1;

  // Report the controlled co-run interference measurement.
  contention::InterferenceEvidence e;
  e.id = contention::InterferenceEvidenceId(1000 + workerId);
  e.generation = contention::InterferenceEvidenceGeneration(1);
  e.victim = contention::WorkloadId{victim ? victim : workerId};
  e.neighbors = {contention::WorkloadId{neighbor ? neighbor : workerId}};
  e.domain = contention::ResourceDomain::COMPUTE;
  e.metric = contention::MetricDirection::HIGHER_IS_BETTER;
  e.isolatedBaseline = baseline;
  e.coRunObservation = corun;
  e.degradation = contention::compute_degradation(e.metric, baseline, corun);
  e.attribution = contention::AttributionStrength::CONTROLLED_COMPARISON;
  e.confidence = 0.9;
  e.sampleCount = 100 + sample;
  e.status = contention::EvidenceStatus::CURRENT;
  e.sampledAt = 1000;
  e.maxAgeMs = 5000;
  e.label = contention::EvidenceLabel::CONTROLLED;
  e.workerBoot = hello.boot;
  e.workloadGeneration = hello.workloadGen;
  auto ep = dist::encode_evidence(e);
  if (!conn->send(contention::MsgType::EVIDENCE, ep.data(), ep.size()).ok()) return 1;

  bool exited = false;
  while (!exited) {
    contention::Frame f;
    contention::Status st = conn->receive(f, true);
    if (!st.ok()) break;
    if (f.type == contention::MsgType::ACTION_AUTHORIZE) {
      dist::WireAction a;
      if (dist::decode_action(f.payload.data(), f.payload.size(), a)) {
        dist::WireAck ack;
        ack.id = a.id; ack.attempt = a.attempt; ack.acknowledged = true;
        // Simulate the reference software control taking effect; if requested,
        // report post-action improvement from the victim's perspective.
        if (role == "A") {
          contention::InterferenceEvidence pe;
          pe = e;
          pe.id = contention::InterferenceEvidenceId(2000 + workerId);
          pe.generation = contention::InterferenceEvidenceGeneration(2);
          pe.sampledAt = 3000;
          pe.coRunObservation = postCorun;
          pe.degradation = contention::compute_degradation(pe.metric, pe.isolatedBaseline, postCorun);
          auto pp = dist::encode_evidence(pe);
          conn->send(contention::MsgType::EVIDENCE, pp.data(), pp.size());
        }
        auto ap = dist::encode_ack(ack);
        conn->send(contention::MsgType::ACTION_ACK, ap.data(), ap.size());
        if (dieAfterAck) break;
      }
    } else if (f.type == contention::MsgType::SHUTDOWN) {
      break;
    } else if (f.type == contention::MsgType::RPC_REQUEST) {
      // Coordinator asks for a fresh measurement (post-action from the victim).
      contention::InterferenceEvidence pe;
      pe = e;
      pe.id = contention::InterferenceEvidenceId(2000 + workerId);
      pe.generation = contention::InterferenceEvidenceGeneration(2);
      pe.sampledAt = 3000;
      pe.coRunObservation = postCorun;
      pe.degradation = contention::compute_degradation(pe.metric, pe.isolatedBaseline, postCorun);
      auto pp = dist::encode_evidence(pe);
      conn->send(contention::MsgType::EVIDENCE, pp.data(), pp.size());
    }
  }
  conn->close();
  contention::socket_shutdown();
  return 0;
}
