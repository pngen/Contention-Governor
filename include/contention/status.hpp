// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#ifndef CONTENTION_GOVERNOR_STATUS_HPP
#define CONTENTION_GOVERNOR_STATUS_HPP

#include <cstdint>
#include <string>
#include <utility>

namespace contention {

// Typed statuses. A Status carries a code and a human-readable message.
enum class StatusCode : std::uint8_t {
  OK,
  INVALID_INPUT,
  INSUFFICIENT_EVIDENCE,
  CONFOUNDED,
  STALE_EVIDENCE,
  STALE_AUTHORITY,
  POLICY_SUPERSEDED,
  CONFLICT_RESOLVED,
  NO_LEGAL_INTERVENTION,
  HARD_CONSTRAINT_VIOLATION,
  FAIRNESS_LIMIT,
  ACTION_BUDGET_EXHAUSTED,
  INTERVENTION_FAILED,
  OUTCOME_UNKNOWN,
  PERSISTENCE_CORRUPT,
  PROTOCOL_ERROR,
  ARITHMETIC_OVERFLOW,
  RESOURCE_EXHAUSTED,
  CANCELLED,
  SHUTTING_DOWN
};

const char* to_string(StatusCode c) noexcept;

// A structured status result. Code OK means success.
struct Status {
  StatusCode code {StatusCode::OK};
  std::string message;

  Status() = default;
  Status(StatusCode c, std::string msg = {}) : code(c), message(std::move(msg)) {}

  bool ok() const noexcept { return code == StatusCode::OK; }
  explicit operator bool() const noexcept { return ok(); }

  static Status good() { return {}; }
  static Status bad(StatusCode c, std::string msg) { return Status(c, std::move(msg)); }

  friend bool operator==(const Status& a, const Status& b) { return a.code == b.code; }
  friend bool operator!=(const Status& a, const Status& b) { return !(a == b); }
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_STATUS_HPP
