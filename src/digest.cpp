// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/digest.hpp"

namespace contention {

namespace {
constexpr std::uint32_t kPoly = 0x82F63B78u;
std::uint32_t table[256];
const bool table_init = ([]() {
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (kPoly ^ (c >> 1)) : (c >> 1);
    table[i] = c;
  }
  return true;
})();
}  // namespace

void Crc32c::update(const void* data, std::size_t len) noexcept {
  const auto* p = static_cast<const unsigned char*>(data);
  std::uint32_t c = crc_;
  for (std::size_t i = 0; i < len; ++i) {
    c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  }
  crc_ = c;
}

std::uint32_t crc32c(const void* data, std::size_t len) noexcept {
  Crc32c c;
  c.update(data, len);
  return c.final();
}

}  // namespace contention
