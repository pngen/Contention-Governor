// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#ifndef CONTENTION_GOVERNOR_ENUMS_HPP
#define CONTENTION_GOVERNOR_ENUMS_HPP

#include <optional>
#include <string_view>
#include <cstddef>

namespace contention {

// ---------------------------------------------------------------------------
// Resource domains. Not every domain is supported on every platform.
// ---------------------------------------------------------------------------
enum class ResourceDomain : std::uint8_t {
  COMPUTE,
  CACHE,
  MEMORY_BANDWIDTH,
  MEMORY_CAPACITY,
  PCIE,
  TRANSFER,
  COLLECTIVE,
  NETWORK,
  NUMA,
  STORAGE,
  HOST_CPU,
  RESIDENCY,
  SHARED_ENGINE,
  UNKNOWN
};

// ---------------------------------------------------------------------------
// Conflict states. Only semantically justified states exist.
// ---------------------------------------------------------------------------
enum class ConflictState : std::uint8_t {
  DETECTED,
  VALIDATING,
  ACTIONABLE,
  INTERVENTION_AUTHORIZED,
  INTERVENTION_IN_FLIGHT,
  VERIFYING,
  RESOLVED,
  PARTIALLY_RESOLVED,
  UNRESOLVED,
  SUPPRESSED,
  REVALIDATION_REQUIRED,
  SUPERSEDED,
  UNKNOWN
};

// ---------------------------------------------------------------------------
// Evidence health status. Active control is never authorized from any status
// other than CURRENT.
// ---------------------------------------------------------------------------
enum class EvidenceStatus : std::uint8_t {
  CURRENT,
  STALE,
  EXPIRED,
  REVALIDATION_REQUIRED,
  UNSUPPORTED,
  UNKNOWN,
  CONFOUNDED,
  INSUFFICIENT_EVIDENCE
};

// ---------------------------------------------------------------------------
// Interference attribution strength. Policy gates disruptive actions by this.
// ---------------------------------------------------------------------------
enum class AttributionStrength : std::uint8_t {
  UNKNOWN,
  TEMPORALLY_ASSOCIATED,
  CORRELATED,
  CONTROLLED_COMPARISON,
  STRONG_COUNTERFACTUAL_EVIDENCE,
  DIRECT_RESOURCE_EVIDENCE
};

// ---------------------------------------------------------------------------
// Why an intervention is warranted.
// ---------------------------------------------------------------------------
enum class BindingObjective : std::uint8_t {
  LATENCY,
  TAIL_LATENCY,
  THROUGHPUT,
  DEADLINE,
  AVAILABILITY,
  RECOVERY,
  COST,
  FAIRNESS,
  RESOURCE_MINIMUM,
  VALUE_PROTECTION,
  MULTIPLE,
  UNKNOWN
};

// ---------------------------------------------------------------------------
// Typed intervention classes.
// ---------------------------------------------------------------------------
enum class InterventionClass : std::uint8_t {
  NO_ACTION,
  OBSERVE_MORE,
  THROTTLE,
  DEFER_NEW_WORK,
  REDUCE_CONCURRENCY,
  REDUCE_BATCH,
  SERIALIZE,
  TIME_SLICE,
  REQUEST_PREEMPTION,
  REQUEST_MIGRATION,
  REQUEST_REPLACEMENT,
  CHANGE_PLACEMENT,
  REQUEST_BANDWIDTH_REALLOCATION,
  REQUEST_COMMUNICATION_REPLAN,
  REQUEST_COLLECTIVE_RESCHEDULE,
  RELEASE_OPTIONAL_RESIDENCY,
  REDUCE_MEMORY_PRESSURE,
  SHED_LOW_VALUE_WORK,
  PROTECT_HIGH_VALUE_WORK,
  ESCALATE,
  MANUAL_INTERVENTION_REQUIRED
};

// ---------------------------------------------------------------------------
// Intervention lifecycle. ACKNOWLEDGED != EFFECTIVE. Terminals stay terminal.
// ---------------------------------------------------------------------------
enum class LifecycleState : std::uint8_t {
  PROPOSED,
  AUTHORIZED,
  DISPATCHED,
  ACKNOWLEDGED,
  EFFECTIVE,
  PARTIALLY_EFFECTIVE,
  INEFFECTIVE,
  FAILED,
  CANCELLED,
  SUPERSEDED,
  EXPIRED,
  OUTCOME_UNKNOWN
};

// ---------------------------------------------------------------------------
// Post-action verification outcome classification.
// ---------------------------------------------------------------------------
enum class VerificationOutcome : std::uint8_t {
  RESOLVED,
  PARTIALLY_RESOLVED,
  INEFFECTIVE,
  WORSENED,
  SHIFTED_CONTENTION,
  CREATED_SECONDARY_VIOLATION,
  CREATED_FAIRNESS_VIOLATION,
  INSUFFICIENT_POST_ACTION_EVIDENCE,
  OUTCOME_UNKNOWN
};

// ---------------------------------------------------------------------------
// Evidence classification label (REAL/CONTROLLED/DERIVED/POLICY/SYNTHETIC/
// UNSUPPORTED).
// ---------------------------------------------------------------------------
enum class EvidenceLabel : std::uint8_t {
  REAL,
  CONTROLLED,
  DERIVED,
  POLICY,
  SYNTHETIC,
  UNSUPPORTED
};

// ---------------------------------------------------------------------------
// Severity of a conflict.
// ---------------------------------------------------------------------------
enum class Severity : std::uint8_t {
  NONE,
  MINOR,
  MODERATE,
  SEVERE,
  CRITICAL,
  UNKNOWN
};

// ---------------------------------------------------------------------------
// Feasibility classification.
// ---------------------------------------------------------------------------
enum class FeasibilityStatus : std::uint8_t {
  FEASIBLE,
  INFEASIBLE
};

// ---------------------------------------------------------------------------
// Boolean outcome for lifecycle/verification transitions.
// ---------------------------------------------------------------------------
enum class TriState : std::uint8_t {
  YES,
  NO,
  UNKNOWN
};

// ---------------------------------------------------------------------------
// to_string / from_string for enums used in persistence and diagnostics.
// ---------------------------------------------------------------------------
#define CONTENTION_CASE(E) case E: return #E

inline const char* to_string(ResourceDomain v) noexcept {
  switch (v) {
    CONTENTION_CASE(ResourceDomain::COMPUTE);
    CONTENTION_CASE(ResourceDomain::CACHE);
    CONTENTION_CASE(ResourceDomain::MEMORY_BANDWIDTH);
    CONTENTION_CASE(ResourceDomain::MEMORY_CAPACITY);
    CONTENTION_CASE(ResourceDomain::PCIE);
    CONTENTION_CASE(ResourceDomain::TRANSFER);
    CONTENTION_CASE(ResourceDomain::COLLECTIVE);
    CONTENTION_CASE(ResourceDomain::NETWORK);
    CONTENTION_CASE(ResourceDomain::NUMA);
    CONTENTION_CASE(ResourceDomain::STORAGE);
    CONTENTION_CASE(ResourceDomain::HOST_CPU);
    CONTENTION_CASE(ResourceDomain::RESIDENCY);
    CONTENTION_CASE(ResourceDomain::SHARED_ENGINE);
    CONTENTION_CASE(ResourceDomain::UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(ConflictState v) noexcept {
  switch (v) {
    CONTENTION_CASE(ConflictState::DETECTED);
    CONTENTION_CASE(ConflictState::VALIDATING);
    CONTENTION_CASE(ConflictState::ACTIONABLE);
    CONTENTION_CASE(ConflictState::INTERVENTION_AUTHORIZED);
    CONTENTION_CASE(ConflictState::INTERVENTION_IN_FLIGHT);
    CONTENTION_CASE(ConflictState::VERIFYING);
    CONTENTION_CASE(ConflictState::RESOLVED);
    CONTENTION_CASE(ConflictState::PARTIALLY_RESOLVED);
    CONTENTION_CASE(ConflictState::UNRESOLVED);
    CONTENTION_CASE(ConflictState::SUPPRESSED);
    CONTENTION_CASE(ConflictState::REVALIDATION_REQUIRED);
    CONTENTION_CASE(ConflictState::SUPERSEDED);
    CONTENTION_CASE(ConflictState::UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(EvidenceStatus v) noexcept {
  switch (v) {
    CONTENTION_CASE(EvidenceStatus::CURRENT);
    CONTENTION_CASE(EvidenceStatus::STALE);
    CONTENTION_CASE(EvidenceStatus::EXPIRED);
    CONTENTION_CASE(EvidenceStatus::REVALIDATION_REQUIRED);
    CONTENTION_CASE(EvidenceStatus::UNSUPPORTED);
    CONTENTION_CASE(EvidenceStatus::UNKNOWN);
    CONTENTION_CASE(EvidenceStatus::CONFOUNDED);
    CONTENTION_CASE(EvidenceStatus::INSUFFICIENT_EVIDENCE);
  }
  return "UNKNOWN";
}

inline const char* to_string(AttributionStrength v) noexcept {
  switch (v) {
    CONTENTION_CASE(AttributionStrength::UNKNOWN);
    CONTENTION_CASE(AttributionStrength::TEMPORALLY_ASSOCIATED);
    CONTENTION_CASE(AttributionStrength::CORRELATED);
    CONTENTION_CASE(AttributionStrength::CONTROLLED_COMPARISON);
    CONTENTION_CASE(AttributionStrength::STRONG_COUNTERFACTUAL_EVIDENCE);
    CONTENTION_CASE(AttributionStrength::DIRECT_RESOURCE_EVIDENCE);
  }
  return "UNKNOWN";
}

inline const char* to_string(BindingObjective v) noexcept {
  switch (v) {
    CONTENTION_CASE(BindingObjective::LATENCY);
    CONTENTION_CASE(BindingObjective::TAIL_LATENCY);
    CONTENTION_CASE(BindingObjective::THROUGHPUT);
    CONTENTION_CASE(BindingObjective::DEADLINE);
    CONTENTION_CASE(BindingObjective::AVAILABILITY);
    CONTENTION_CASE(BindingObjective::RECOVERY);
    CONTENTION_CASE(BindingObjective::COST);
    CONTENTION_CASE(BindingObjective::FAIRNESS);
    CONTENTION_CASE(BindingObjective::RESOURCE_MINIMUM);
    CONTENTION_CASE(BindingObjective::VALUE_PROTECTION);
    CONTENTION_CASE(BindingObjective::MULTIPLE);
    CONTENTION_CASE(BindingObjective::UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(InterventionClass v) noexcept {
  switch (v) {
    CONTENTION_CASE(InterventionClass::NO_ACTION);
    CONTENTION_CASE(InterventionClass::OBSERVE_MORE);
    CONTENTION_CASE(InterventionClass::THROTTLE);
    CONTENTION_CASE(InterventionClass::DEFER_NEW_WORK);
    CONTENTION_CASE(InterventionClass::REDUCE_CONCURRENCY);
    CONTENTION_CASE(InterventionClass::REDUCE_BATCH);
    CONTENTION_CASE(InterventionClass::SERIALIZE);
    CONTENTION_CASE(InterventionClass::TIME_SLICE);
    CONTENTION_CASE(InterventionClass::REQUEST_PREEMPTION);
    CONTENTION_CASE(InterventionClass::REQUEST_MIGRATION);
    CONTENTION_CASE(InterventionClass::REQUEST_REPLACEMENT);
    CONTENTION_CASE(InterventionClass::CHANGE_PLACEMENT);
    CONTENTION_CASE(InterventionClass::REQUEST_BANDWIDTH_REALLOCATION);
    CONTENTION_CASE(InterventionClass::REQUEST_COMMUNICATION_REPLAN);
    CONTENTION_CASE(InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE);
    CONTENTION_CASE(InterventionClass::RELEASE_OPTIONAL_RESIDENCY);
    CONTENTION_CASE(InterventionClass::REDUCE_MEMORY_PRESSURE);
    CONTENTION_CASE(InterventionClass::SHED_LOW_VALUE_WORK);
    CONTENTION_CASE(InterventionClass::PROTECT_HIGH_VALUE_WORK);
    CONTENTION_CASE(InterventionClass::ESCALATE);
    CONTENTION_CASE(InterventionClass::MANUAL_INTERVENTION_REQUIRED);
  }
  return "UNKNOWN";
}

inline const char* to_string(LifecycleState v) noexcept {
  switch (v) {
    CONTENTION_CASE(LifecycleState::PROPOSED);
    CONTENTION_CASE(LifecycleState::AUTHORIZED);
    CONTENTION_CASE(LifecycleState::DISPATCHED);
    CONTENTION_CASE(LifecycleState::ACKNOWLEDGED);
    CONTENTION_CASE(LifecycleState::EFFECTIVE);
    CONTENTION_CASE(LifecycleState::PARTIALLY_EFFECTIVE);
    CONTENTION_CASE(LifecycleState::INEFFECTIVE);
    CONTENTION_CASE(LifecycleState::FAILED);
    CONTENTION_CASE(LifecycleState::CANCELLED);
    CONTENTION_CASE(LifecycleState::SUPERSEDED);
    CONTENTION_CASE(LifecycleState::EXPIRED);
    CONTENTION_CASE(LifecycleState::OUTCOME_UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(VerificationOutcome v) noexcept {
  switch (v) {
    CONTENTION_CASE(VerificationOutcome::RESOLVED);
    CONTENTION_CASE(VerificationOutcome::PARTIALLY_RESOLVED);
    CONTENTION_CASE(VerificationOutcome::INEFFECTIVE);
    CONTENTION_CASE(VerificationOutcome::WORSENED);
    CONTENTION_CASE(VerificationOutcome::SHIFTED_CONTENTION);
    CONTENTION_CASE(VerificationOutcome::CREATED_SECONDARY_VIOLATION);
    CONTENTION_CASE(VerificationOutcome::CREATED_FAIRNESS_VIOLATION);
    CONTENTION_CASE(VerificationOutcome::INSUFFICIENT_POST_ACTION_EVIDENCE);
    CONTENTION_CASE(VerificationOutcome::OUTCOME_UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(EvidenceLabel v) noexcept {
  switch (v) {
    CONTENTION_CASE(EvidenceLabel::REAL);
    CONTENTION_CASE(EvidenceLabel::CONTROLLED);
    CONTENTION_CASE(EvidenceLabel::DERIVED);
    CONTENTION_CASE(EvidenceLabel::POLICY);
    CONTENTION_CASE(EvidenceLabel::SYNTHETIC);
    CONTENTION_CASE(EvidenceLabel::UNSUPPORTED);
  }
  return "UNKNOWN";
}

inline const char* to_string(Severity v) noexcept {
  switch (v) {
    CONTENTION_CASE(Severity::NONE);
    CONTENTION_CASE(Severity::MINOR);
    CONTENTION_CASE(Severity::MODERATE);
    CONTENTION_CASE(Severity::SEVERE);
    CONTENTION_CASE(Severity::CRITICAL);
    CONTENTION_CASE(Severity::UNKNOWN);
  }
  return "UNKNOWN";
}

inline const char* to_string(FeasibilityStatus v) noexcept {
  switch (v) {
    CONTENTION_CASE(FeasibilityStatus::FEASIBLE);
    CONTENTION_CASE(FeasibilityStatus::INFEASIBLE);
  }
  return "UNKNOWN";
}

inline const char* to_string(TriState v) noexcept {
  switch (v) {
    CONTENTION_CASE(TriState::YES);
    CONTENTION_CASE(TriState::NO);
    CONTENTION_CASE(TriState::UNKNOWN);
  }
  return "UNKNOWN";
}

#undef CONTENTION_CASE

// ---------------------------------------------------------------------------
// from_string (case-sensitive) used by persistence/I-O.
// ---------------------------------------------------------------------------
#define CONTENTION_FROM(EnumName, ...) \
  inline std::optional<EnumName> from_string_##EnumName(std::string_view s) { \
    static constexpr std::pair<std::string_view, EnumName> k[] = { __VA_ARGS__ }; \
    for (auto& p : k) { if (p.first == s) return p.second; } \
    return std::nullopt; \
  }

CONTENTION_FROM(ResourceDomain,
  {"COMPUTE", ResourceDomain::COMPUTE},
  {"CACHE", ResourceDomain::CACHE},
  {"MEMORY_BANDWIDTH", ResourceDomain::MEMORY_BANDWIDTH},
  {"MEMORY_CAPACITY", ResourceDomain::MEMORY_CAPACITY},
  {"PCIE", ResourceDomain::PCIE},
  {"TRANSFER", ResourceDomain::TRANSFER},
  {"COLLECTIVE", ResourceDomain::COLLECTIVE},
  {"NETWORK", ResourceDomain::NETWORK},
  {"NUMA", ResourceDomain::NUMA},
  {"STORAGE", ResourceDomain::STORAGE},
  {"HOST_CPU", ResourceDomain::HOST_CPU},
  {"RESIDENCY", ResourceDomain::RESIDENCY},
  {"SHARED_ENGINE", ResourceDomain::SHARED_ENGINE},
  {"UNKNOWN", ResourceDomain::UNKNOWN})

CONTENTION_FROM(ConflictState,
  {"DETECTED", ConflictState::DETECTED},
  {"VALIDATING", ConflictState::VALIDATING},
  {"ACTIONABLE", ConflictState::ACTIONABLE},
  {"INTERVENTION_AUTHORIZED", ConflictState::INTERVENTION_AUTHORIZED},
  {"INTERVENTION_IN_FLIGHT", ConflictState::INTERVENTION_IN_FLIGHT},
  {"VERIFYING", ConflictState::VERIFYING},
  {"RESOLVED", ConflictState::RESOLVED},
  {"PARTIALLY_RESOLVED", ConflictState::PARTIALLY_RESOLVED},
  {"UNRESOLVED", ConflictState::UNRESOLVED},
  {"SUPPRESSED", ConflictState::SUPPRESSED},
  {"REVALIDATION_REQUIRED", ConflictState::REVALIDATION_REQUIRED},
  {"SUPERSEDED", ConflictState::SUPERSEDED},
  {"UNKNOWN", ConflictState::UNKNOWN})

CONTENTION_FROM(EvidenceStatus,
  {"CURRENT", EvidenceStatus::CURRENT},
  {"STALE", EvidenceStatus::STALE},
  {"EXPIRED", EvidenceStatus::EXPIRED},
  {"REVALIDATION_REQUIRED", EvidenceStatus::REVALIDATION_REQUIRED},
  {"UNSUPPORTED", EvidenceStatus::UNSUPPORTED},
  {"UNKNOWN", EvidenceStatus::UNKNOWN},
  {"CONFOUNDED", EvidenceStatus::CONFOUNDED},
  {"INSUFFICIENT_EVIDENCE", EvidenceStatus::INSUFFICIENT_EVIDENCE})

CONTENTION_FROM(AttributionStrength,
  {"UNKNOWN", AttributionStrength::UNKNOWN},
  {"TEMPORALLY_ASSOCIATED", AttributionStrength::TEMPORALLY_ASSOCIATED},
  {"CORRELATED", AttributionStrength::CORRELATED},
  {"CONTROLLED_COMPARISON", AttributionStrength::CONTROLLED_COMPARISON},
  {"STRONG_COUNTERFACTUAL_EVIDENCE", AttributionStrength::STRONG_COUNTERFACTUAL_EVIDENCE},
  {"DIRECT_RESOURCE_EVIDENCE", AttributionStrength::DIRECT_RESOURCE_EVIDENCE})

CONTENTION_FROM(BindingObjective,
  {"LATENCY", BindingObjective::LATENCY},
  {"TAIL_LATENCY", BindingObjective::TAIL_LATENCY},
  {"THROUGHPUT", BindingObjective::THROUGHPUT},
  {"DEADLINE", BindingObjective::DEADLINE},
  {"AVAILABILITY", BindingObjective::AVAILABILITY},
  {"RECOVERY", BindingObjective::RECOVERY},
  {"COST", BindingObjective::COST},
  {"FAIRNESS", BindingObjective::FAIRNESS},
  {"RESOURCE_MINIMUM", BindingObjective::RESOURCE_MINIMUM},
  {"VALUE_PROTECTION", BindingObjective::VALUE_PROTECTION},
  {"MULTIPLE", BindingObjective::MULTIPLE},
  {"UNKNOWN", BindingObjective::UNKNOWN})

CONTENTION_FROM(InterventionClass,
  {"NO_ACTION", InterventionClass::NO_ACTION},
  {"OBSERVE_MORE", InterventionClass::OBSERVE_MORE},
  {"THROTTLE", InterventionClass::THROTTLE},
  {"DEFER_NEW_WORK", InterventionClass::DEFER_NEW_WORK},
  {"REDUCE_CONCURRENCY", InterventionClass::REDUCE_CONCURRENCY},
  {"REDUCE_BATCH", InterventionClass::REDUCE_BATCH},
  {"SERIALIZE", InterventionClass::SERIALIZE},
  {"TIME_SLICE", InterventionClass::TIME_SLICE},
  {"REQUEST_PREEMPTION", InterventionClass::REQUEST_PREEMPTION},
  {"REQUEST_MIGRATION", InterventionClass::REQUEST_MIGRATION},
  {"REQUEST_REPLACEMENT", InterventionClass::REQUEST_REPLACEMENT},
  {"CHANGE_PLACEMENT", InterventionClass::CHANGE_PLACEMENT},
  {"REQUEST_BANDWIDTH_REALLOCATION", InterventionClass::REQUEST_BANDWIDTH_REALLOCATION},
  {"REQUEST_COMMUNICATION_REPLAN", InterventionClass::REQUEST_COMMUNICATION_REPLAN},
  {"REQUEST_COLLECTIVE_RESCHEDULE", InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE},
  {"RELEASE_OPTIONAL_RESIDENCY", InterventionClass::RELEASE_OPTIONAL_RESIDENCY},
  {"REDUCE_MEMORY_PRESSURE", InterventionClass::REDUCE_MEMORY_PRESSURE},
  {"SHED_LOW_VALUE_WORK", InterventionClass::SHED_LOW_VALUE_WORK},
  {"PROTECT_HIGH_VALUE_WORK", InterventionClass::PROTECT_HIGH_VALUE_WORK},
  {"ESCALATE", InterventionClass::ESCALATE},
  {"MANUAL_INTERVENTION_REQUIRED", InterventionClass::MANUAL_INTERVENTION_REQUIRED})

CONTENTION_FROM(LifecycleState,
  {"PROPOSED", LifecycleState::PROPOSED},
  {"AUTHORIZED", LifecycleState::AUTHORIZED},
  {"DISPATCHED", LifecycleState::DISPATCHED},
  {"ACKNOWLEDGED", LifecycleState::ACKNOWLEDGED},
  {"EFFECTIVE", LifecycleState::EFFECTIVE},
  {"PARTIALLY_EFFECTIVE", LifecycleState::PARTIALLY_EFFECTIVE},
  {"INEFFECTIVE", LifecycleState::INEFFECTIVE},
  {"FAILED", LifecycleState::FAILED},
  {"CANCELLED", LifecycleState::CANCELLED},
  {"SUPERSEDED", LifecycleState::SUPERSEDED},
  {"EXPIRED", LifecycleState::EXPIRED},
  {"OUTCOME_UNKNOWN", LifecycleState::OUTCOME_UNKNOWN})

CONTENTION_FROM(VerificationOutcome,
  {"RESOLVED", VerificationOutcome::RESOLVED},
  {"PARTIALLY_RESOLVED", VerificationOutcome::PARTIALLY_RESOLVED},
  {"INEFFECTIVE", VerificationOutcome::INEFFECTIVE},
  {"WORSENED", VerificationOutcome::WORSENED},
  {"SHIFTED_CONTENTION", VerificationOutcome::SHIFTED_CONTENTION},
  {"CREATED_SECONDARY_VIOLATION", VerificationOutcome::CREATED_SECONDARY_VIOLATION},
  {"CREATED_FAIRNESS_VIOLATION", VerificationOutcome::CREATED_FAIRNESS_VIOLATION},
  {"INSUFFICIENT_POST_ACTION_EVIDENCE", VerificationOutcome::INSUFFICIENT_POST_ACTION_EVIDENCE},
  {"OUTCOME_UNKNOWN", VerificationOutcome::OUTCOME_UNKNOWN})

CONTENTION_FROM(EvidenceLabel,
  {"REAL", EvidenceLabel::REAL},
  {"CONTROLLED", EvidenceLabel::CONTROLLED},
  {"DERIVED", EvidenceLabel::DERIVED},
  {"POLICY", EvidenceLabel::POLICY},
  {"SYNTHETIC", EvidenceLabel::SYNTHETIC},
  {"UNSUPPORTED", EvidenceLabel::UNSUPPORTED})

CONTENTION_FROM(Severity,
  {"NONE", Severity::NONE},
  {"MINOR", Severity::MINOR},
  {"MODERATE", Severity::MODERATE},
  {"SEVERE", Severity::SEVERE},
  {"CRITICAL", Severity::CRITICAL},
  {"UNKNOWN", Severity::UNKNOWN})

#undef CONTENTION_FROM

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_ENUMS_HPP
