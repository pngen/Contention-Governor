// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Injectable time. Tests use a manual clock; production uses a steady clock.
// Time is an unsigned millisecond count relative to an arbitrary epoch.

#ifndef CONTENTION_GOVERNOR_CLOCK_HPP
#define CONTENTION_GOVERNOR_CLOCK_HPP

#include <cstdint>

namespace contention {

using TimeMs = std::uint64_t;

// Abstract monotonic millisecond clock.
class Clock {
 public:
  virtual ~Clock() = default;
  virtual TimeMs now_ms() const = 0;
};

// Real monotonic clock (steady_clock).
class SystemClock final : public Clock {
 public:
  TimeMs now_ms() const override;
};

// Deterministic manual clock for tests.
class ManualClock final : public Clock {
 public:
  explicit ManualClock(TimeMs start = 0) : t_(start) {}
  TimeMs now_ms() const override { return t_; }
  void set(TimeMs t) { t_ = t; }
  void advance(TimeMs delta) { t_ += delta; }
 private:
  TimeMs t_ {0};
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_CLOCK_HPP
