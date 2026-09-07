// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/policy.hpp"

#include <algorithm>
#include <cmath>

namespace contention {

WorkloadPolicy workload_policy_for(const ContentionPolicy& p, WorkloadId w) {
  auto it = p.workloads.find(w);
  if (it != p.workloads.end()) return it->second;
  WorkloadPolicy d;
  d.workload = w;
  d.generation = p.generation;
  return d;
}

Status validate_policy(const ContentionPolicy& p) {
  if (p.actionabilityThreshold < 0.0 || p.actionabilityThreshold > 1.0)
    return Status::bad(StatusCode::INVALID_INPUT, "actionability threshold out of range");
  if (p.releaseThreshold < 0.0 || p.releaseThreshold > 1.0)
    return Status::bad(StatusCode::INVALID_INPUT, "release threshold out of range");
  if (p.releaseThreshold >= p.actionabilityThreshold)
    return Status::bad(StatusCode::INVALID_INPUT, "release threshold must be below actionability threshold");
  if (p.hysteresisEvidenceCount == 0)
    return Status::bad(StatusCode::INVALID_INPUT, "hysteresis evidence count must be > 0");
  if (!std::isfinite(p.minConfidenceForDisruptive) || p.minConfidenceForDisruptive < 0.0 ||
      p.minConfidenceForDisruptive > 1.0)
    return Status::bad(StatusCode::INVALID_INPUT, "min confidence out of range");
  if (p.fairnessFloor < 0.0 || p.fairnessFloor > 1.0)
    return Status::bad(StatusCode::INVALID_INPUT, "fairness floor out of range");
  if (p.maxConsecutiveInterventionsPerWorkload == 0)
    return Status::bad(StatusCode::INVALID_INPUT, "max consecutive interventions must be > 0");
  if (p.budget.maxPreemptionsPerWindow == 0 || p.budget.maxMigrationsPerWindow == 0 ||
      p.budget.maxThrottlesPerWindow == 0 || p.budget.maxAnyPerWindow == 0)
    return Status::bad(StatusCode::INVALID_INPUT, "action budget limits must be > 0");
  for (const auto& [w, wp] : p.workloads) {
    if (!std::isfinite(wp.value)) return Status::bad(StatusCode::INVALID_INPUT, "workload value non-finite");
    if (wp.hardSloFloor < 0.0 || wp.hardSloFloor > 1.0)
      return Status::bad(StatusCode::INVALID_INPUT, "hard SLO floor out of range");
    if (wp.minServiceFloor < 0.0 || wp.minServiceFloor > 1.0)
      return Status::bad(StatusCode::INVALID_INPUT, "min service floor out of range");
    if (wp.maxDegradationTolerated < 0.0 || wp.maxDegradationTolerated > 1.0)
      return Status::bad(StatusCode::INVALID_INPUT, "max degradation tolerated out of range");
  }
  return Status::good();
}

bool class_eligible(const ContentionPolicy& p, InterventionClass c) {
  switch (c) {
    case InterventionClass::NO_ACTION:
    case InterventionClass::OBSERVE_MORE:
    case InterventionClass::ESCALATE:
    case InterventionClass::MANUAL_INTERVENTION_REQUIRED:
      return true;
    case InterventionClass::THROTTLE:
    case InterventionClass::REDUCE_CONCURRENCY:
    case InterventionClass::REDUCE_BATCH:
    case InterventionClass::SERIALIZE:
    case InterventionClass::TIME_SLICE:
    case InterventionClass::REDUCE_MEMORY_PRESSURE:
      return p.throttleAllowed;
    case InterventionClass::DEFER_NEW_WORK:
      return p.admissionRestrictionAllowed;
    case InterventionClass::REQUEST_PREEMPTION:
      return p.preemptionAllowed;
    case InterventionClass::REQUEST_MIGRATION:
    case InterventionClass::REQUEST_REPLACEMENT:
      return p.migrationAllowed;
    case InterventionClass::CHANGE_PLACEMENT:
      return p.placementChangeAllowed;
    case InterventionClass::REQUEST_BANDWIDTH_REALLOCATION:
    case InterventionClass::REQUEST_COMMUNICATION_REPLAN:
    case InterventionClass::REQUEST_COLLECTIVE_RESCHEDULE:
      return p.bandwidthRequestAllowed || p.collectiveSchedulingAllowed;
    case InterventionClass::RELEASE_OPTIONAL_RESIDENCY:
      return true;
    case InterventionClass::SHED_LOW_VALUE_WORK:
    case InterventionClass::PROTECT_HIGH_VALUE_WORK:
      return true;
  }
  return false;
}

}  // namespace contention
