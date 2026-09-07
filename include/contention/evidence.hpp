// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Interference-evidence input contract. Contention Governor consumes
// Interference-Observatory-compatible evidence; it never measures interference
// itself. Active control is only ever authorized from CURRENT evidence.

#ifndef CONTENTION_GOVERNOR_EVIDENCE_HPP
#define CONTENTION_GOVERNOR_EVIDENCE_HPP

#include "contention/enums.hpp"
#include "contention/ids.hpp"
#include "contention/clock.hpp"
#include "contention/status.hpp"

#include <cmath>
#include <string>
#include <vector>

namespace contention {

// Whether a metric is better when larger or smaller.
enum class MetricDirection : std::uint8_t {
  HIGHER_IS_BETTER,   // throughput, ops/s, availability
  LOWER_IS_BETTER     // latency, p99
};

const char* to_string(MetricDirection v) noexcept;

// A single interference observation supplied by the observatory.
struct InterferenceEvidence {
  InterferenceEvidenceId id;
  InterferenceEvidenceGeneration generation;
  WorkloadId victim;
  std::vector<WorkloadId> neighbors;
  ResourceDomain domain;
  MetricDirection metric {MetricDirection::HIGHER_IS_BETTER};

  double isolatedBaseline {0.0};
  double coRunObservation {0.0};
  double degradation {0.0};        // computed, >= 0 fraction (victim worsened)
  double improvement {0.0};        // computed, >= 0 fraction (post-action benefitted)

  AttributionStrength attribution {AttributionStrength::UNKNOWN};
  double confidence {0.0};         // [0,1]
  std::uint64_t sampleCount {0};
  std::vector<std::string> unresolvedConfounders;
  EvidenceStatus status {EvidenceStatus::UNKNOWN};

  // Freshness / provenance.
  TimeMs sampledAt {0};
  TimeMs maxAgeMs {0};
  EvidenceLabel label {EvidenceLabel::SYNTHETIC};
  std::string sourceId;
  std::string sourceHealth;

  // Authority generations that must be current for this evidence to be current.
  WorkerBootId workerBoot;
  WorkloadGeneration workloadGeneration;
  DeviceGeneration deviceGeneration;
  HostGeneration hostGeneration;
  TopologyGeneration topologyGeneration;
  ResourceGeneration resourceGeneration;
  PlacementGeneration placementGeneration;

  // Active interval.
  TimeMs activeFrom {0};
  TimeMs activeUntil {0};
};

// Compute signed degradation fraction given a direction.
// Returns a non-negative fraction (0 = no degradation).
double compute_degradation(MetricDirection d, double baseline, double observation) noexcept;

// Compute improvement fraction of observation relative to baseline (0 when no improvement).
double compute_improvement(MetricDirection d, double baseline, double observation) noexcept;

// Validate evidence. Returns an error Status if the evidence is structurally
// invalid (NaN/Inf, out-of-range confidence/count, impossible values) or if it
// is not CURRENT-enough for active control.
Status validate_evidence(const InterferenceEvidence& e);

// True when the evidence is structurally sane (finite, in-range).
bool structurally_valid(const InterferenceEvidence& e) noexcept;

// True when active control is permitted by this evidence's status/attribution.
bool evidence_authorizes_control(const InterferenceEvidence& e);

// Classify a conflict severity from a degradation fraction.
Severity classify_severity(double degradation) noexcept;

// A measurement seed used to derive numeric values in tests/examples.
struct MeasurementSeed {
  double value {1.0};
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_EVIDENCE_HPP
