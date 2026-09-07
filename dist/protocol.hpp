// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Wire protocol shared by the coordinator and worker processes in the real
// multiprocess TCP proof. Compose messages into framed payloads using the
// bounded byte helpers; transport uses contention::Connection framing.

#ifndef CONTENTION_DIST_PROTOCOL_HPP
#define CONTENTION_DIST_PROTOCOL_HPP

#include "contention/cg.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace dist {

class Writer {
 public:
  std::vector<std::uint8_t>& b;
  explicit Writer(std::vector<std::uint8_t>& bb) : b(bb) {}
  void u8(std::uint8_t v) { b.push_back(v); }
  void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); }
  void u64(std::uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF)); }
  void f64(double v) { std::uint64_t u = 0; std::memcpy(&u, &v, 8); u64(u); }
  void str(const std::string& s) { u64(s.size()); b.insert(b.end(), s.begin(), s.end()); }
};

class Reader {
 public:
  const std::uint8_t* d;
  std::size_t n;
  std::size_t p {0};
  bool ok {true};
  Reader(const std::uint8_t* data, std::size_t len) : d(data), n(len) {}
  bool u8(std::uint8_t& v) { if (p + 1 > n) { ok = false; return false; } v = d[p++]; return true; }
  bool u32(std::uint32_t& v) { if (p + 4 > n) { ok = false; return false; } v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(d[p + i]) << (8 * i); p += 4; return true; }
  bool u64(std::uint64_t& v) { if (p + 8 > n) { ok = false; return false; } v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(d[p + i]) << (8 * i); p += 8; return true; }
  bool f64(double& v) { std::uint64_t u; if (!u64(u)) return false; std::memcpy(&v, &u, 8); return true; }
  bool str(std::string& s) { std::uint64_t len; if (!u64(len)) return false; if (len > (1u << 20) || p + len > n) { ok = false; return false; } s.assign(reinterpret_cast<const char*>(d + p), static_cast<std::size_t>(len)); p += static_cast<std::size_t>(len); return true; }
};

template <typename Id>
void wid(Writer& w, Id id) { w.u64(id.value()); }
template <typename Id>
bool rid(Reader& r, Id& v) { std::uint64_t u; if (!r.u64(u)) return false; v = Id(u); return true; }

struct WorkerHello {
  contention::WorkerId worker;
  contention::WorkerBootId boot;
  contention::WorkloadGeneration workloadGen;
  std::string role;
};

struct WireEvidence {
  contention::InterferenceEvidence ev;
};

struct WireAction {
  contention::InterventionId id;
  contention::InterventionClass cls;
  contention::WorkloadId target;
  contention::AttemptId attempt;
  double intensity {1.0};
};

struct WireAck {
  contention::InterventionId id;
  contention::AttemptId attempt;
  bool acknowledged {false};
  std::string note;
};

std::vector<std::uint8_t> encode_hello(const WorkerHello& h);
bool decode_hello(const std::uint8_t* d, std::size_t n, WorkerHello& h);
std::vector<std::uint8_t> encode_evidence(const contention::InterferenceEvidence& e);
bool decode_evidence(const std::uint8_t* d, std::size_t n, contention::InterferenceEvidence& e);
std::vector<std::uint8_t> encode_action(const WireAction& a);
bool decode_action(const std::uint8_t* d, std::size_t n, WireAction& a);
std::vector<std::uint8_t> encode_ack(const WireAck& a);
bool decode_ack(const std::uint8_t* d, std::size_t n, WireAck& a);

}  // namespace dist

#endif
