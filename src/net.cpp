// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/net.hpp"

#include "contention/digest.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using sock_t = SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
#endif

#include <cstring>
#include <memory>

namespace contention {

namespace {
constexpr sock_t kInvalidSock = static_cast<sock_t>(-1);

#if defined(_WIN32)
bool g_winsock_init = false;
#endif

bool socket_valid(sock_t s) { return s != kInvalidSock; }
void close_sock(sock_t s) {
#if defined(_WIN32)
  closesocket(s);
#else
  ::close(s);
#endif
}
}  // namespace

const char* to_string(MsgType v) noexcept {
  switch (v) {
    case MsgType::HELLO: return "HELLO";
    case MsgType::WORKER_STATUS: return "WORKER_STATUS";
    case MsgType::EVIDENCE: return "EVIDENCE";
    case MsgType::ACTION_AUTHORIZE: return "ACTION_AUTHORIZE";
    case MsgType::ACTION_ACK: return "ACTION_ACK";
    case MsgType::ACTION_COMPLETE: return "ACTION_COMPLETE";
    case MsgType::POLICY_UPDATE: return "POLICY_UPDATE";
    case MsgType::SHUTDOWN: return "SHUTDOWN";
    case MsgType::RPC_REQUEST: return "RPC_REQUEST";
    case MsgType::RPC_RESPONSE: return "RPC_RESPONSE";
  }
  return "UNKNOWN";
}

Status socket_init() {
#if defined(_WIN32)
  if (g_winsock_init) return Status::good();
  WSADATA wsa;
  int err = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (err != 0) return Status::bad(StatusCode::PROTOCOL_ERROR, "WSAStartup failed");
  g_winsock_init = true;
#endif
  return Status::good();
}

void socket_shutdown() {
#if defined(_WIN32)
  if (g_winsock_init) {
    WSACleanup();
    g_winsock_init = false;
  }
#endif
}

std::vector<std::uint8_t> encode_frame(MsgType type, const std::uint8_t* payload,
                                       std::size_t len) {
  std::vector<std::uint8_t> out;
  out.reserve(kFrameHeaderLen + len + kFrameCrcLen);
  auto pu32 = [&](std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  };
  pu32(kProtocolMagic);
  pu32(kProtocolVersion);
  pu32(static_cast<std::uint32_t>(type));
  pu32(static_cast<std::uint32_t>(len));
  out.insert(out.end(), payload, payload + len);
  std::uint32_t crc = crc32c(out.data() + 8, kFrameHeaderLen - 8 + len);
  pu32(crc);
  return out;
}

namespace {
bool write_all(sock_t s, const std::uint8_t* data, std::size_t len) {
  std::size_t off = 0;
  while (off < len) {
    int sent = static_cast<int>(::send(s, reinterpret_cast<const char*>(data + off),
                                       static_cast<int>(len - off), 0));
    if (sent <= 0) return false;
    off += static_cast<std::size_t>(sent);
  }
  return true;
}
}  // namespace

Connection::Connection(std::uintptr_t sock) : sock_(static_cast<sock_t>(sock)) {}

Connection::~Connection() { close(); }

bool Connection::valid() const noexcept { return socket_valid(static_cast<sock_t>(sock_)); }

void Connection::close() {
  if (socket_valid(static_cast<sock_t>(sock_))) {
    close_sock(static_cast<sock_t>(sock_));
    sock_ = static_cast<std::uintptr_t>(kInvalidSock);
  }
}

Status Connection::send(MsgType type, const std::uint8_t* payload, std::size_t len) {
  if (len > kMaxFramePayload)
    return Status::bad(StatusCode::PROTOCOL_ERROR, "frame too large");
  if (!valid()) return Status::bad(StatusCode::PROTOCOL_ERROR, "connection closed");
  std::lock_guard<std::mutex> lock(write_mu_);
  std::vector<std::uint8_t> frame = encode_frame(type, payload, len);
  if (!write_all(static_cast<sock_t>(sock_), frame.data(), frame.size())) {
    return Status::bad(StatusCode::PROTOCOL_ERROR, "write failed");
  }
  return Status::good();
}

namespace {
bool parse_header(const std::uint8_t* h, std::uint32_t& version, std::uint32_t& type,
                  std::uint32_t& len) {
  std::uint32_t magic = 0;
  for (int i = 0; i < 4; ++i) magic |= static_cast<std::uint32_t>(h[i]) << (8 * i);
  if (magic != kProtocolMagic) return false;
  version = 0;
  for (int i = 0; i < 4; ++i) version |= static_cast<std::uint32_t>(h[4 + i]) << (8 * i);
  type = 0;
  for (int i = 0; i < 4; ++i) type |= static_cast<std::uint32_t>(h[8 + i]) << (8 * i);
  len = 0;
  for (int i = 0; i < 4; ++i) len |= static_cast<std::uint32_t>(h[12 + i]) << (8 * i);
  return true;
}
}  // namespace

Status Connection::receive(Frame& out, bool blocking) {
  const auto read_some = [&](std::vector<std::uint8_t>& buf) -> int {
    std::uint8_t tmp[4096];
    int n = static_cast<int>(::recv(static_cast<sock_t>(sock_),
                                    reinterpret_cast<char*>(tmp),
                                    static_cast<int>(sizeof(tmp)), 0));
    if (n > 0) {
      buf.insert(buf.end(), tmp, tmp + n);
      return 1;
    }
    if (n == 0) return 0;
    return -1;
  };

  for (;;) {
    if (rbuf_.size() >= kFrameHeaderLen) {
      std::uint32_t version = 0, type = 0, len = 0;
      if (!parse_header(rbuf_.data(), version, type, len))
        return Status::bad(StatusCode::PROTOCOL_ERROR, "malformed frame magic/version");
      if (version != kProtocolVersion)
        return Status::bad(StatusCode::PROTOCOL_ERROR, "unsupported protocol version");
      if (len > kMaxFramePayload)
        return Status::bad(StatusCode::PROTOCOL_ERROR, "oversized frame");
      const std::size_t need = kFrameHeaderLen + static_cast<std::size_t>(len) + kFrameCrcLen;
      if (rbuf_.size() >= need) {
        std::uint32_t crc = 0;
        for (int i = 0; i < 4; ++i)
          crc |= static_cast<std::uint32_t>(rbuf_[kFrameHeaderLen + len + i]) << (8 * i);
        std::uint32_t expect =
            crc32c(rbuf_.data() + 8, kFrameHeaderLen - 8 + static_cast<std::size_t>(len));
        if (crc != expect)
          return Status::bad(StatusCode::PROTOCOL_ERROR, "frame checksum mismatch");
        out.type = static_cast<MsgType>(type);
        out.payload.assign(rbuf_.begin() + kFrameHeaderLen,
                           rbuf_.begin() + kFrameHeaderLen + len);
        rbuf_.erase(rbuf_.begin(), rbuf_.begin() + need);
        return Status::good();
      }
    }

    int r = read_some(rbuf_);
    if (r == 0) {
      if (rbuf_.empty()) return Status::bad(StatusCode::PROTOCOL_ERROR, "peer closed");
      return Status::bad(StatusCode::PROTOCOL_ERROR, "peer closed with partial frame");
    }
    if (r < 0) {
      return Status::bad(StatusCode::PROTOCOL_ERROR, "recv failed");
    }
    if (rbuf_.size() > kMaxFramePayload + kFrameHeaderLen + kFrameCrcLen) {
      return Status::bad(StatusCode::PROTOCOL_ERROR, "oversized receive buffer");
    }
    if (!blocking) {
      return Status::bad(StatusCode::PROTOCOL_ERROR, "no complete frame (nonblocking)");
    }
  }
}

Listener::~Listener() {
  if (socket_valid(static_cast<sock_t>(sock_))) close_sock(static_cast<sock_t>(sock_));
}

Status Listener::bind_and_listen(std::uint16_t port) {
  socket_init();
  sock_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (!socket_valid(s)) return Status::bad(StatusCode::PROTOCOL_ERROR, "socket failed");
  int reuse = 1;
#if defined(_WIN32)
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_sock(s);
    return Status::bad(StatusCode::PROTOCOL_ERROR, "bind failed");
  }
  socklen_t slen = sizeof(addr);
  if (getsockname(s, reinterpret_cast<sockaddr*>(&addr), &slen) == 0) {
    port_ = ntohs(addr.sin_port);
  } else {
    close_sock(s);
    return Status::bad(StatusCode::PROTOCOL_ERROR, "getsockname failed");
  }
  if (::listen(s, 16) != 0) {
    close_sock(s);
    return Status::bad(StatusCode::PROTOCOL_ERROR, "listen failed");
  }
  sock_ = static_cast<std::uintptr_t>(s);
  return Status::good();
}

std::uint16_t Listener::bound_port() const { return port_; }

std::unique_ptr<Connection> Listener::accept(TimeMs timeoutMs) {
  if (!socket_valid(static_cast<sock_t>(sock_))) return nullptr;
  sock_t s = static_cast<sock_t>(sock_);
  if (timeoutMs != 0) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    timeval tv;
    tv.tv_sec = static_cast<long>(timeoutMs / 1000);
    tv.tv_usec = static_cast<long>((timeoutMs % 1000) * 1000);
    int rc = ::select(static_cast<int>(s) + 1, &rfds, nullptr, nullptr, &tv);
    if (rc <= 0) return nullptr;
  }
  sockaddr_in caddr {};
  socklen_t clen = sizeof(caddr);
  sock_t cs = ::accept(s, reinterpret_cast<sockaddr*>(&caddr), &clen);
  if (!socket_valid(cs)) return nullptr;
  return std::make_unique<Connection>(static_cast<std::uintptr_t>(cs));
}

Status decode_frames(const std::uint8_t* data, std::size_t len, std::vector<Frame>& out) {
  std::size_t pos = 0;
  while (pos < len) {
    if (len - pos < kFrameHeaderLen)
      return Status::bad(StatusCode::PROTOCOL_ERROR, "truncated frame header");
    std::uint32_t version = 0, type = 0, flen = 0;
    if (!parse_header(data + pos, version, type, flen))
      return Status::bad(StatusCode::PROTOCOL_ERROR, "malformed frame magic/version");
    if (version != kProtocolVersion)
      return Status::bad(StatusCode::PROTOCOL_ERROR, "unsupported protocol version");
    if (flen > kMaxFramePayload)
      return Status::bad(StatusCode::PROTOCOL_ERROR, "oversized frame");
    const std::size_t need = kFrameHeaderLen + static_cast<std::size_t>(flen) + kFrameCrcLen;
    if (len - pos < need)
      return Status::bad(StatusCode::PROTOCOL_ERROR, "truncated frame");
    std::uint32_t crc = 0;
    for (int i = 0; i < 4; ++i)
      crc |= static_cast<std::uint32_t>(data[pos + kFrameHeaderLen + flen + i]) << (8 * i);
    std::uint32_t expect =
        crc32c(data + pos + 8, kFrameHeaderLen - 8 + static_cast<std::size_t>(flen));
    if (crc != expect)
      return Status::bad(StatusCode::PROTOCOL_ERROR, "frame checksum mismatch");
    Frame f;
    f.type = static_cast<MsgType>(type);
    f.payload.assign(data + pos + kFrameHeaderLen, data + pos + kFrameHeaderLen + flen);
    out.push_back(std::move(f));
    pos += need;
  }
  return Status::good();
}

std::unique_ptr<Connection> connect_to(const std::string& host, std::uint16_t port) {
  socket_init();
  sock_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (!socket_valid(s)) return nullptr;
  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
      close_sock(s);
      return nullptr;
    }
    std::memcpy(&addr.sin_addr, &reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr,
                sizeof(addr.sin_addr));
    freeaddrinfo(res);
  }
  if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_sock(s);
    return nullptr;
  }
  return std::make_unique<Connection>(static_cast<std::uintptr_t>(s));
}

}  // namespace contention
