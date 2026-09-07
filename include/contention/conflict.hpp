// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Explicit contention-conflict representation. Directionality preserved.
// Multi-party conflicts are representable without fabricating a precise
// responsibility split.

#ifndef CONTENTION_GOVERNOR_CONFLICT_HPP
#define CONTENTION_GOVERNOR_CONFLICT_HPP

#include "contention/enums.hpp"
#include "contention/evidence.hpp"
#include "contention/ids.hpp"
#include "contention/status.hpp"

#include <optional>
#include <string>
#include <vector>

namespace contention {

struct ContentionConflict {
  ConflictId id;
  ConflictGeneration generation;

  std::vector<WorkloadId> affected;     // victims
  std::vector<WorkloadId> associated;   // interferers
  ResourceDomain domain {ResourceDomain::UNKNOWN};

  double degradation {0.0};             // magnitude of the binding objective
  double baseline {0.0};
  double confidence {0.0};
  BindingObjective binding {BindingObjective::UNKNOWN};
  Severity severity {Severity::UNKNOWN};

  std::vector<InterferenceEvidenceId> evidenceIds;
  ConflictState state {ConflictState::DETECTED};

  TimeMs firstDetectedAt {0};
  TimeMs lastUpdatedAt {0};
  TimeMs activeUntil {0};

  std::vector<std::string> unresolvedConfounders;
  std::vector<std::string> blockers;            // infeasibility reasons
  std::vector<InterventionClass> candidateClasses;
  std::vector<InterventionId> proposedIds;      // authorizable interventions
  std::vector<InterventionId> selectedIds;

  PolicyGeneration policyGeneration;
  SLOGeneration sloGeneration;
  ValueGeneration valueGeneration;
  FairnessGeneration fairnessGeneration;
  ResourceGeneration resourceGeneration;
  PlacementGeneration placementGeneration;
  CoordinatorEpoch coordinatorEpoch;
  WorkerBootId workerBoot;

  // Multi-party group effect: if true, the residual joint effect of the
  // associated group exceeds what pairwise attributions explain, and no exact
  // per-workload responsibility split is asserted.
  bool groupEffectExplicit {false};

  // Hysteresis bookkeeping.
  TimeMs lastActionAt {0};
  bool inActionableState {false};
};

// A structured rejected intervention (with deterministic reason).
struct RejectedIntervention {
  InterventionClass cls;
  std::string reason;   // e.g. "hard SLO floor", "non-preemptible", "stale"
};

// Aggregated effect model for a candidate.
struct InterventionCandidate {
  InterventionClass cls;
  WorkloadId target;
  double expectedConflictReduction {0.0};   // [0,1]
  double expectedTargetImprovement {0.0};   // [0,1]
  double expectedNeighborDegradation {0.0};
  double expectedCost {0.0};
  double expectedRecoveryCost {0.0};
  double confidence {0.0};
  double reversibility {0.5};               // [0,1]
  TimeMs timeToEffect {0};
  FeasibilityStatus feasible {FeasibilityStatus::INFEASIBLE};
  std::vector<std::string> infeasibilityReasons;
  std::string adapter;                       // typed adapter label
};

// Deterministic ranked decision.
struct InterventionDecision {
  InterventionCandidate chosen;
  std::vector<std::string> rankingTrace;
};

// The full structured evaluation of a conflict.
struct ConflictEvaluation {
  ConflictId conflictId;
  ConflictState state {ConflictState::UNKNOWN};
  BindingObjective binding {BindingObjective::UNKNOWN};
  double degradation {0.0};
  Severity severity {Severity::UNKNOWN};
  std::vector<InterventionCandidate> candidates;
  std::vector<RejectedIntervention> rejected;
  std::optional<InterventionDecision> decision;
  std::vector<std::string> reasons;
  Status status;
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_CONFLICT_HPP
