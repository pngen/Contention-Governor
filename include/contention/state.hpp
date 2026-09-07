// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// The authoritative durable governable state. Persisted with versioning and
// integrity checks. Runtime-only handles (sockets, processes, adapter
// connections) are never part of this state.

#ifndef CONTENTION_GOVERNOR_STATE_HPP
#define CONTENTION_GOVERNOR_STATE_HPP

#include "contention/digest.hpp"
#include "contention/engine.hpp"

#include <cstdint>

namespace contention {

// Durable governance state. Everything the engine reasons over lives inside
// `engine`; the trailing counters and digest are bookkeeping.
struct GovernorState {
  EngineState engine;

  // Monotonic id generators.
  std::uint64_t nextConflictId {1};
  std::uint64_t nextEvidenceId {1};
  std::uint64_t nextInterventionId {1};
  std::uint64_t nextDispatchId {1};
  std::uint64_t nextAttemptId {1};
  std::uint64_t nextVerificationId {1};

  // Canonical digest of the last committed state (excludes nondeterministic
  // runtime-only fields).
  Digest64 lastDigest {0};
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_STATE_HPP
