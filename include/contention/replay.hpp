// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Deterministic replay. Given identical canonical history and policy inputs,
// the same conflict states, candidate sets, rejected reasons, selected action,
// fairness accounting, verification outcomes, and digest must reconstruct.

#ifndef CONTENTION_GOVERNOR_REPLAY_HPP
#define CONTENTION_GOVERNOR_REPLAY_HPP

#include "contention/state.hpp"
#include "contention/status.hpp"

#include <cstddef>
#include <vector>

namespace contention {

struct ReplayReport {
  std::size_t conflictsEvaluated {0};
  std::size_t candidatesBuilt {0};
  std::vector<ConflictId> diverged;
  Status status;
};

// Re-evaluate every conflict from a persisted state and compare each decision
// to the recorded selected intervention. Reports structural divergence.
ReplayReport replay(const GovernorState& s);

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_REPLAY_HPP
