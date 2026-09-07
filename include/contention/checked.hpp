// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Checked arithmetic. No silent wrap in counts, durations, budgets, debt, etc.

#ifndef CONTENTION_GOVERNOR_CHECKED_HPP
#define CONTENTION_GOVERNOR_CHECKED_HPP

#include <cstdint>
#include <limits>

namespace contention {

namespace detail {
template <typename T>
constexpr T max_v = std::numeric_limits<T>::max();
}

// Returns false on overflow; out is set only on success.
inline bool checked_add(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
  if (b > detail::max_v<std::uint64_t> - a) return false;
  out = a + b;
  return true;
}

inline bool checked_mul(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
  if (a != 0 && b > detail::max_v<std::uint64_t> / a) return false;
  out = a * b;
  return true;
}

inline bool checked_add(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
  const std::int64_t lo = std::numeric_limits<std::int64_t>::min();
  const std::int64_t hi = std::numeric_limits<std::int64_t>::max();
  if ((b > 0 && a > hi - b) || (b < 0 && a < lo - b)) return false;
  out = a + b;
  return true;
}

// Saturating add for counters used in accounting where saturation is safe and
// cheaper than propagation; callers must still guard the action-budget bounds.
inline std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) noexcept {
  std::uint64_t out = 0;
  return checked_add(a, b, out) ? out : detail::max_v<std::uint64_t>;
}

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_CHECKED_HPP
