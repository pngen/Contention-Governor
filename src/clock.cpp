// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/clock.hpp"

#include <chrono>

namespace contention {

TimeMs SystemClock::now_ms() const {
  using namespace std::chrono;
  return static_cast<TimeMs>(duration_cast<milliseconds>(
      steady_clock::now().time_since_epoch()).count());
}

}  // namespace contention
