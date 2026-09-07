// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "protocol.hpp"

namespace dist {

std::vector<std::uint8_t> encode_hello(const WorkerHello& h) {
  std::vector<std::uint8_t> b;
  Writer w(b);
  wid(w, h.worker); wid(w, h.boot); wid(w, h.workloadGen);
  w.str(h.role);
  return b;
}

bool decode_hello(const std::uint8_t* d, std::size_t n, WorkerHello& h) {
  Reader r(d, n);
  if (!rid(r, h.worker) || !rid(r, h.boot) || !rid(r, h.workloadGen)) return false;
  return r.str(h.role) && r.ok;
}

std::vector<std::uint8_t> encode_evidence(const contention::InterferenceEvidence& e) {
  std::vector<std::uint8_t> b;
  Writer w(b);
  wid(w, e.id); wid(w, e.generation); wid(w, e.victim);
  w.u64(e.neighbors.size()); for (auto x : e.neighbors) wid(w, x);
  w.u8(static_cast<std::uint8_t>(e.domain));
  w.u8(static_cast<std::uint8_t>(e.metric));
  w.f64(e.isolatedBaseline); w.f64(e.coRunObservation);
  w.u8(static_cast<std::uint8_t>(e.attribution));
  w.f64(e.confidence); w.u64(e.sampleCount);
  w.u8(static_cast<std::uint8_t>(e.status));
  w.u64(e.sampledAt); w.u64(e.maxAgeMs);
  w.u8(static_cast<std::uint8_t>(e.label));
  w.str(e.sourceId); w.str(e.sourceHealth);
  wid(w, e.workerBoot); wid(w, e.workloadGeneration); wid(w, e.deviceGeneration);
  wid(w, e.hostGeneration); wid(w, e.topologyGeneration);
  wid(w, e.resourceGeneration); wid(w, e.placementGeneration);
  w.u64(e.activeFrom); w.u64(e.activeUntil);
  w.u64(e.unresolvedConfounders.size());
  for (auto& c : e.unresolvedConfounders) w.str(c);
  return b;
}

bool decode_evidence(const std::uint8_t* d, std::size_t n, contention::InterferenceEvidence& e) {
  Reader r(d, n);
  if (!rid(r, e.id) || !rid(r, e.generation) || !rid(r, e.victim)) return false;
  std::uint64_t nn = 0;
  if (!r.u64(nn) || nn > 1024) return false;
  e.neighbors.clear();
  for (std::uint64_t i = 0; i < nn; ++i) { contention::WorkloadId x; if (!rid(r, x)) return false; e.neighbors.push_back(x); }
  std::uint8_t raw = 0;
  if (!r.u8(raw) || raw >= 14) return false; e.domain = static_cast<contention::ResourceDomain>(raw);
  if (!r.u8(raw) || raw >= 2) return false; e.metric = static_cast<contention::MetricDirection>(raw);
  if (!r.f64(e.isolatedBaseline) || !r.f64(e.coRunObservation)) return false;
  if (!r.u8(raw) || raw >= 6) return false; e.attribution = static_cast<contention::AttributionStrength>(raw);
  if (!r.f64(e.confidence)) return false;
  if (!r.u64(e.sampleCount)) return false;
  if (!r.u8(raw) || raw >= 8) return false; e.status = static_cast<contention::EvidenceStatus>(raw);
  if (!r.u64(e.sampledAt) || !r.u64(e.maxAgeMs)) return false;
  if (!r.u8(raw) || raw >= 6) return false; e.label = static_cast<contention::EvidenceLabel>(raw);
  if (!r.str(e.sourceId) || !r.str(e.sourceHealth)) return false;
  if (!rid(r, e.workerBoot) || !rid(r, e.workloadGeneration) || !rid(r, e.deviceGeneration)) return false;
  if (!rid(r, e.hostGeneration) || !rid(r, e.topologyGeneration)) return false;
  if (!rid(r, e.resourceGeneration) || !rid(r, e.placementGeneration)) return false;
  if (!r.u64(e.activeFrom) || !r.u64(e.activeUntil)) return false;
  std::uint64_t nc = 0;
  if (!r.u64(nc) || nc > 1024) return false;
  e.unresolvedConfounders.clear();
  for (std::uint64_t i = 0; i < nc; ++i) { std::string s; if (!r.str(s)) return false; e.unresolvedConfounders.push_back(s); }
  return r.ok;
}

std::vector<std::uint8_t> encode_action(const WireAction& a) {
  std::vector<std::uint8_t> b;
  Writer w(b);
  wid(w, a.id); wid(w, a.target); wid(w, a.attempt);
  w.u8(static_cast<std::uint8_t>(a.cls));
  w.f64(a.intensity);
  return b;
}

bool decode_action(const std::uint8_t* d, std::size_t n, WireAction& a) {
  Reader r(d, n);
  if (!rid(r, a.id) || !rid(r, a.target) || !rid(r, a.attempt)) return false;
  std::uint8_t raw = 0;
  if (!r.u8(raw) || raw >= 21) return false; a.cls = static_cast<contention::InterventionClass>(raw);
  return r.f64(a.intensity) && r.ok;
}

std::vector<std::uint8_t> encode_ack(const WireAck& a) {
  std::vector<std::uint8_t> b;
  Writer w(b);
  wid(w, a.id); wid(w, a.attempt);
  w.u8(a.acknowledged ? 1 : 0);
  w.str(a.note);
  return b;
}

bool decode_ack(const std::uint8_t* d, std::size_t n, WireAck& a) {
  Reader r(d, n);
  if (!rid(r, a.id) || !rid(r, a.attempt)) return false;
  std::uint8_t raw = 0;
  if (!r.u8(raw)) return false; a.acknowledged = raw != 0;
  return r.str(a.note) && r.ok;
}

}  // namespace dist
