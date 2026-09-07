// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Versioned, integrity-checked, bounded persistence. Rejects corruption,
// truncation, trailing garbage, unknown versions, and absurd sizes before
// allocating. Saves are atomic (write temp + rename).

#ifndef CONTENTION_GOVERNOR_PERSISTENCE_HPP
#define CONTENTION_GOVERNOR_PERSISTENCE_HPP

#include "contention/state.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace contention {

constexpr std::uint32_t kPersistenceVersion = 1;
constexpr std::size_t kMaxStringLen = 1u << 20;   // 1 MiB per string
constexpr std::size_t kMaxCollectionLen = 1u << 20; // 1M elements per collection

// Atomically save a state snapshot to path (write temp + replace).
// Returns false and sets status on failure.
bool save_state(const std::string& path, const GovernorState& s, Status& st);

// Load a state snapshot from path. Rejects corruption/truncation/garbage.
Status load_state(const std::string& path, GovernorState& s);

// Serialize to an in-memory buffer (for write-outside-lock paths and tests).
std::vector<std::uint8_t> save_to_bytes(const GovernorState& s);

// Deserialize from an in-memory buffer.
Status load_from_bytes(const std::uint8_t* data, std::size_t len, GovernorState& s);

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_PERSISTENCE_HPP
