// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Canonical hashing primitives: FNV-1a 64 (semantic digests) and CRC-32C
// (persistence integrity). Both are streaming-capable.

#ifndef CONTENTION_GOVERNOR_DIGEST_HPP
#define CONTENTION_GOVERNOR_DIGEST_HPP

#include <cstddef>
#include <cstdint>

namespace contention {

using Digest64 = std::uint64_t;

namespace detail {
inline constexpr Digest64 kFnvOffset = 14695981039346656037ULL;
inline constexpr Digest64 kFnvPrime  = 1099511628211ULL;
}  // namespace detail

inline Digest64 fnv1a_update(Digest64 h, const void* data, std::size_t len) noexcept {
  const auto* p = static_cast<const unsigned char*>(data);
  for (std::size_t i = 0; i < len; ++i) {
    h ^= p[i];
    h *= detail::kFnvPrime;
  }
  return h;
}

inline Digest64 fnv1a_bytes(const void* data, std::size_t len) noexcept {
  return fnv1a_update(detail::kFnvOffset, data, len);
}

// Streaming CRC-32C (Castagnoli). Used for persistence integrity.
class Crc32c {
 public:
  Crc32c() = default;
  void update(const void* data, std::size_t len) noexcept;
  std::uint32_t final() const noexcept { return ~crc_; }
  void reset() noexcept { crc_ = 0xFFFFFFFFu; }

 private:
  std::uint32_t crc_ {0xFFFFFFFFu};
};

std::uint32_t crc32c(const void* data, std::size_t len) noexcept;

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_DIGEST_HPP
