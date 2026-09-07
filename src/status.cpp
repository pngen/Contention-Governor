// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/status.hpp"

namespace contention {

const char* to_string(StatusCode c) noexcept {
  switch (c) {
    case StatusCode::OK: return "OK";
    case StatusCode::INVALID_INPUT: return "INVALID_INPUT";
    case StatusCode::INSUFFICIENT_EVIDENCE: return "INSUFFICIENT_EVIDENCE";
    case StatusCode::CONFOUNDED: return "CONFOUNDED";
    case StatusCode::STALE_EVIDENCE: return "STALE_EVIDENCE";
    case StatusCode::STALE_AUTHORITY: return "STALE_AUTHORITY";
    case StatusCode::POLICY_SUPERSEDED: return "POLICY_SUPERSEDED";
    case StatusCode::CONFLICT_RESOLVED: return "CONFLICT_RESOLVED";
    case StatusCode::NO_LEGAL_INTERVENTION: return "NO_LEGAL_INTERVENTION";
    case StatusCode::HARD_CONSTRAINT_VIOLATION: return "HARD_CONSTRAINT_VIOLATION";
    case StatusCode::FAIRNESS_LIMIT: return "FAIRNESS_LIMIT";
    case StatusCode::ACTION_BUDGET_EXHAUSTED: return "ACTION_BUDGET_EXHAUSTED";
    case StatusCode::INTERVENTION_FAILED: return "INTERVENTION_FAILED";
    case StatusCode::OUTCOME_UNKNOWN: return "OUTCOME_UNKNOWN";
    case StatusCode::PERSISTENCE_CORRUPT: return "PERSISTENCE_CORRUPT";
    case StatusCode::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
    case StatusCode::ARITHMETIC_OVERFLOW: return "ARITHMETIC_OVERFLOW";
    case StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
    case StatusCode::CANCELLED: return "CANCELLED";
    case StatusCode::SHUTTING_DOWN: return "SHUTTING_DOWN";
  }
  return "OK";
}

}  // namespace contention
