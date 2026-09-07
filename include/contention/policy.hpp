// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Explicit policy generation-bound model: hard/soft SLO, value, priority,
// fairness floor, feasibility gates, cooldown, hysteresis, action budgets.

#ifndef CONTENTION_GOVERNOR_POLICY_HPP
#define CONTENTION_GOVERNOR_POLICY_HPP

#include "contention/enums.hpp"
#include "contention/evidence.hpp"
#include "contention/ids.hpp"
#include "contention/status.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace contention {

// Per-workload policy. Generation-bound.
struct WorkloadPolicy {
  WorkloadId workload;
  PolicyGeneration generation;

  bool nonPreemptible {false};
  bool protectedWorkload {false};

  double value {0.0};          // configured policy value (not monetary unless so declared)
  std::uint32_t priority {0};  // higher = more urgent scheduling priority
  double hardSloFloor {0.0};   // fraction of baseline service that must remain (hard)
  std::optional<double> softSloTarget;
  double minServiceFloor {0.0}; // fairness floor fraction
  double costCeiling {0.0};     // 0 => unbounded
  double restartCost {0.0};
  double preemptionCost {0.0};
  double recoveryCost {0.0};

  double maxDegradationTolerated {1.0};  // 0..1; hard cap on imposed degradation
  std::uint32_t maxInterventionFrequencyPerWindow {0}; // 0 => policy default

  bool preemptionAllowed {true};
  bool throttlingAllowed {true};
  bool deferralAllowed {true};
  bool admissionRestrictionAllowed {true};
  bool placementChangeAllowed {true};
  bool bandwidthRequestAllowed {true};
  bool collectiveSchedulingAllowed {true};
  bool migrationAllowed {true};
};

// Exact action-budget accounting. Generation-aware.
struct ActionBudgetStats {
  std::uint64_t preemptions {0};
  std::uint64_t migrations {0};
  std::uint64_t placementChanges {0};
  std::uint64_t throttles {0};
  std::uint64_t collectiveReschedules {0};
  std::uint64_t all {0};

  friend bool operator==(const ActionBudgetStats& a, const ActionBudgetStats& b) {
    return a.preemptions == b.preemptions && a.migrations == b.migrations &&
           a.placementChanges == b.placementChanges && a.throttles == b.throttles &&
           a.collectiveReschedules == b.collectiveReschedules && a.all == b.all;
  }
};

struct ActionBudget {
  ActionBudgetGeneration generation;
  std::uint64_t maxPreemptionsPerWindow {3};
  std::uint64_t maxMigrationsPerWindow {2};
  std::uint64_t maxPlacementChangesPerWindow {2};
  std::uint64_t maxThrottlesPerWindow {8};
  std::uint64_t maxCollectiveReschedulesPerWindow {2};
  std::uint64_t maxAnyPerWindow {32};
  TimeMs windowMs {60000};
  TimeMs windowStart {0};
  ActionBudgetStats used;
};

// Global contention policy.
struct ContentionPolicy {
  PolicyId id;
  PolicyGeneration generation;

  // Hysteresis.
  double actionabilityThreshold {0.20};   // degrade >= this => actionable
  double releaseThreshold {0.10};         // degrade <= this for enough samples => release
  std::uint64_t hysteresisEvidenceCount {2};

  // Gating disruptive actions on attribution/confidence.
  AttributionStrength minAttributionForDisruptive {AttributionStrength::CONTROLLED_COMPARISON};
  double minConfidenceForDisruptive {0.6};

  // Fairness.
  double fairnessFloor {0.10};                 // minimum service floor fraction
  std::uint32_t maxConsecutiveInterventionsPerWorkload {3};
  double fairnessWeight {1.0};

  // Cost awareness (consumed evidence).
  double costCeiling {0.0};

  // Cooldown (ms) for disruptive classes.
  TimeMs cooldownMs {5000};

  ActionBudget budget;
  std::map<WorkloadId, WorkloadPolicy> workloads;

  // Global toggles -- the governor does NOT own these mechanisms, only whether
  // it may emit the corresponding intent.
  bool preemptionAllowed {true};
  bool migrationAllowed {true};
  bool placementChangeAllowed {true};
  bool bandwidthRequestAllowed {true};
  bool collectiveSchedulingAllowed {true};
  bool admissionRestrictionAllowed {true};
  bool throttleAllowed {true};
};

// Access a workload policy, falling back to defaults for unknown workloads.
WorkloadPolicy workload_policy_for(const ContentionPolicy& p, WorkloadId w);

// Validation. Returns INVALID_INPUT with a reason on a structural defect.
Status validate_policy(const ContentionPolicy& p);

// Whether a given class is eligible for consideration at all (conservative
// default eligibility by class).
bool class_eligible(const ContentionPolicy& p, InterventionClass c);

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_POLICY_HPP
