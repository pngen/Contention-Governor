// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Deterministic decision engine. Pure functions over immutable engine state;
// no external calls, no clock effects other than explicit TimeMs inputs, and no
// dependence on unordered-container iteration. Identical canonical state =>
// identical decision.

#ifndef CONTENTION_GOVERNOR_ENGINE_HPP
#define CONTENTION_GOVERNOR_ENGINE_HPP

#include "contention/conflict.hpp"
#include "contention/evidence.hpp"
#include "contention/ids.hpp"
#include "contention/policy.hpp"
#include "contention/status.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace contention {

// The generation fence that an intervention carries. Every one of these must
// still be current at pre-dispatch revalidation and at completion.
struct Fence {
  CoordinatorEpoch coordinatorEpoch;
  GovernorGeneration governorGeneration;
  PolicyGeneration policyGeneration;
  SLOGeneration sloGeneration;
  ValueGeneration valueGeneration;
  FairnessGeneration fairnessGeneration;
  ResourceGeneration resourceGeneration;
  PlacementGeneration placementGeneration;
  TopologyGeneration topologyGeneration;
  ConflictGeneration conflictGeneration;
  InterferenceEvidenceGeneration evidenceGeneration;
  WorkerBootId workerBoot;
  WorkloadGeneration workloadGeneration;
  InterventionGeneration interventionGeneration;

  friend bool operator==(const Fence& a, const Fence& b) {
    return a.coordinatorEpoch == b.coordinatorEpoch &&
           a.governorGeneration == b.governorGeneration &&
           a.policyGeneration == b.policyGeneration &&
           a.sloGeneration == b.sloGeneration &&
           a.valueGeneration == b.valueGeneration &&
           a.fairnessGeneration == b.fairnessGeneration &&
           a.resourceGeneration == b.resourceGeneration &&
           a.placementGeneration == b.placementGeneration &&
           a.topologyGeneration == b.topologyGeneration &&
           a.conflictGeneration == b.conflictGeneration &&
           a.evidenceGeneration == b.evidenceGeneration &&
           a.workerBoot == b.workerBoot &&
           a.workloadGeneration == b.workloadGeneration &&
           a.interventionGeneration == b.interventionGeneration;
  }
  friend bool operator!=(const Fence& a, const Fence& b) { return !(a == b); }
};

// Current authority generations as known by the coordinator.
struct AuthorityContext {
  CoordinatorEpoch coordinatorEpoch;
  GovernorGeneration governorGeneration;
  PolicyGeneration policyGeneration;
  SLOGeneration sloGeneration;
  ValueGeneration valueGeneration;
  FairnessGeneration fairnessGeneration;
  ResourceGeneration resourceGeneration;
  PlacementGeneration placementGeneration;
  TopologyGeneration topologyGeneration;

  // Per-workload generations and boot ids.
  std::map<WorkloadId, WorkloadGeneration> workloadGenerations;
  std::map<WorkloadId, WorkerBootId> workloadBoots;
};

// The recorded in-flight / historical intervention.
struct InterventionRecord {
  InterventionId id;
  InterventionGeneration generation;
  InterventionClass cls;
  ConflictId conflictId;
  WorkloadId target;
  LifecycleState state {LifecycleState::PROPOSED};
  Fence fence;
  std::string adapter;
  TimeMs authorizedAt {0};
  TimeMs dispatchedAt {0};
  TimeMs acknowledgedAt {0};
  TimeMs completedAt {0};
  DispatchId dispatchId;
  AttemptId attemptId;
  std::string ackNote;
  Status interimStatus;
  VerificationOutcome result {VerificationOutcome::OUTCOME_UNKNOWN};
  std::vector<std::string> trace;
  bool verificationReported {false};
};

// Fairness state per workload.
struct FairnessRecord {
  WorkloadId workload;
  std::uint64_t interventionCount {0};
  std::uint32_t consecutiveInterventions {0};
  double deprivationDebt {0.0};
  TimeMs lastInterventionAt {0};
};

// Complete immutable engine state snapshot used by the deterministic pipeline.
struct EngineState {
  ContentionPolicy policy;
  std::map<ConflictId, ContentionConflict> conflicts;
  std::map<InterferenceEvidenceId, InterferenceEvidence> evidence;
  std::map<InterventionId, InterventionRecord> interventions;
  std::map<WorkloadId, FairnessRecord> fairness;
  AuthorityContext authority;
};

// ---------------------------------------------------------------------------
// Feasibility
// ---------------------------------------------------------------------------
struct FeasibilityResult {
  FeasibilityStatus status {FeasibilityStatus::INFEASIBLE};
  std::vector<std::string> reasons;
};

FeasibilityResult check_feasibility(const EngineState& s, WorkloadId target,
                                    InterventionClass cls, const Fence& f);

// ---------------------------------------------------------------------------
// Deterministic evaluation pipeline.
//   validate evidence -> identify binding objective -> build legal
//   interventions -> remove hard-constraint violations -> evaluate floors ->
//   fairness -> expected reduction -> collateral -> value -> cost ->
//   reversibility -> deterministic tie-break.
// ---------------------------------------------------------------------------
ConflictEvaluation evaluate_conflict(const EngineState& s, ConflictId conflictId,
                                     const AuthorityContext& ctx);

// Build the generation fence recorded at authorization time.
Fence make_fence(const AuthorityContext& ctx, ConflictGeneration conflictGeneration,
                 InterferenceEvidenceGeneration evidenceGeneration,
                 WorkloadId target, InterventionGeneration interventionGeneration);

// Revalidate a fence against the current authority. Returns STALE_AUTHORITY on
// any mismatch, OK on success.
Status revalidate(const EngineState& s, const Fence& f, WorkloadId target);

// Generate a unique intervention id (deterministic only if ids are preallocated
// by the caller; here it just creates a fresh value from the counter).
InterventionId next_intervention_id();

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_ENGINE_HPP
