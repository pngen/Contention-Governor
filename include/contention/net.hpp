// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Framed, versioned, bounded, checksummed TCP loopback protocol used by the
// distributed multiprocess proof. Partial-read safe, partial-write safe,
// concurrent-write safe, malformed-frame safe, oversized-frame safe.

#ifndef CONTENTION_GOVERNOR_NET_HPP
#define CONTENTION_GOVERNOR_NET_HPP

#include "contention/clock.hpp"
#include "contention/status.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace contention {

constexpr std::uint32_t kProtocolMagic = 0x00C64E01u;   // "CGOV" little-endian
constexpr std::uint32_t kProtocolVersion = 1;
constexpr std::uint32_t kMaxFramePayload = 1u << 20;    // 1 MiB
constexpr std::size_t kFrameHeaderLen = 16;             // magic+version+type+len
constexpr std::size_t kFrameCrcLen = 4;

enum class MsgType : std::uint32_t {
  HELLO = 1,               // handshake
  WORKER_STATUS,           // worker boot/status
  EVIDENCE,                // interference evidence
  ACTION_AUTHORIZE,        // coordinator -> worker intent
  ACTION_ACK,              // worker -> coordinator acknowledgment
  ACTION_COMPLETE,         // worker completes / reports outcome
  POLICY_UPDATE,           // policy generation change
  SHUTDOWN,                // orderly shutdown
  RPC_REQUEST,             // generic versioned request
  RPC_RESPONSE             // generic versioned response
};

const char* to_string(MsgType v) noexcept;

struct Frame {
  MsgType type {MsgType::RPC_REQUEST};
  std::vector<std::uint8_t> payload;
};

// Encodes one frame into a contiguous byte buffer.
std::vector<std::uint8_t> encode_frame(MsgType type, const std::uint8_t* payload,
                                       std::size_t len);

// Decode zero or more complete frames from a contiguous buffer. Rejects
// malformed magic/version, oversized payloads, and checksum mismatches.
// On success `out` contains every complete frame in order.
Status decode_frames(const std::uint8_t* data, std::size_t len, std::vector<Frame>& out);

// Thread-safe framed connection over a socket.
class Connection {
 public:
  // Adopts an already-connected socket handle (SOCKET on Windows, fd on POSIX).
  explicit Connection(std::uintptr_t sock);
  ~Connection();
  Connection(const Connection&) = delete;
  Connection& operator=(const Connection&) = delete;

  // Thread-safe, partial-write-safe send.
  Status send(MsgType type, const std::uint8_t* payload, std::size_t len);

  // Partial-read-safe receive of exactly one frame. Returns
  // Status::bad(PROTOCOL_ERROR) on close-with-partial-frame or malformed input.
  Status receive(Frame& out, bool blocking);

  void close();

  bool valid() const noexcept;

 private:
  std::uintptr_t sock_;
  std::mutex write_mu_;
  std::vector<std::uint8_t> rbuf_;
};

// Cross-platform winsock/POSIX init helpers (Windows-first).
Status socket_init();
void socket_shutdown();

// RAII TCP listener.
class Listener {
 public:
  Listener() = default;
  ~Listener();
  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;

  Status bind_and_listen(std::uint16_t port);
  // Accept a connection with an optional timeout (ms). Returns a Connection*.
  std::unique_ptr<Connection> accept(TimeMs timeoutMs);
  std::uint16_t bound_port() const;

 private:
  std::uintptr_t sock_ {0};
  std::uint16_t port_ {0};
};

// Connect to a loopback endpoint.
std::unique_ptr<Connection> connect_to(const std::string& host, std::uint16_t port);

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_NET_HPP
