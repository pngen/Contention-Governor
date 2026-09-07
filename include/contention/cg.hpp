// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Umbrella header. Includes the complete public surface.

#ifndef CONTENTION_GOVERNOR_CG_HPP
#define CONTENTION_GOVERNOR_CG_HPP

#include "contention/adapters.hpp"
#include "contention/checked.hpp"
#include "contention/clock.hpp"
#include "contention/conflict.hpp"
#include "contention/digest.hpp"
#include "contention/engine.hpp"
#include "contention/enums.hpp"
#include "contention/evidence.hpp"
#include "contention/governor.hpp"
#include "contention/ids.hpp"
#include "contention/net.hpp"
#include "contention/persistence.hpp"
#include "contention/policy.hpp"
#include "contention/replay.hpp"
#include "contention/state.hpp"
#include "contention/status.hpp"

namespace contention {
inline const char* version() noexcept { return "1.0.0"; }
}  // namespace contention

#endif  // CONTENTION_GOVERNOR_CG_HPP
