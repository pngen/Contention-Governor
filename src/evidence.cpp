// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/evidence.hpp"

#include <algorithm>

namespace contention {

const char* to_string(MetricDirection v) noexcept {
  switch (v) {
    case MetricDirection::HIGHER_IS_BETTER: return "HIGHER_IS_BETTER";
    case MetricDirection::LOWER_IS_BETTER: return "LOWER_IS_BETTER";
  }
  return "UNKNOWN";
}

double compute_degradation(MetricDirection d, double baseline, double observation) noexcept {
  if (!(baseline > 0.0) || !std::isfinite(baseline) || !std::isfinite(observation)) return 0.0;
  if (d == MetricDirection::HIGHER_IS_BETTER) {
    if (observation >= baseline) return 0.0;
    return (baseline - observation) / baseline;
  }
  // lower is better
  if (observation <= baseline) return 0.0;
  return (observation - baseline) / baseline;
}

double compute_improvement(MetricDirection d, double baseline, double observation) noexcept {
  if (!(baseline > 0.0) || !std::isfinite(baseline) || !std::isfinite(observation)) return 0.0;
  if (d == MetricDirection::HIGHER_IS_BETTER) {
    if (observation <= baseline) return 0.0;
    return (observation - baseline) / baseline;
  }
  if (observation >= baseline) return 0.0;
  return (baseline - observation) / baseline;
}

bool structurally_valid(const InterferenceEvidence& e) noexcept {
  if (!std::isfinite(e.isolatedBaseline) || !std::isfinite(e.coRunObservation)) return false;
  if (e.isolatedBaseline <= 0.0) return false;
  if (!std::isfinite(e.confidence) || e.confidence < 0.0 || e.confidence > 1.0) return false;
  if (!std::isfinite(e.degradation) || e.degradation < 0.0) return false;
  if (e.sampleCount == 0) return false;
  // A victim with no identified neighbor is a valid-but-unattributed observation
  // (the observatory could not identify a justified aggressor). It is never
  // actionable, but it is structurally valid.
  return true;
}

Status validate_evidence(const InterferenceEvidence& e) {
  if (e.victim.value() == 0) return Status::bad(StatusCode::INVALID_INPUT, "evidence victim id is zero");
  if (!structurally_valid(e)) {
    return Status::bad(StatusCode::INVALID_INPUT, "evidence structurally invalid (NaN/Inf/range/count)");
  }
  return Status::good();
}

bool evidence_authorizes_control(const InterferenceEvidence& e) {
  if (e.status != EvidenceStatus::CURRENT) return false;
  if (!structurally_valid(e)) return false;
  // CONFOUNDED / INSUFFICIENT never reach here because status != CURRENT,
  // but guard the structural side independently.
  if (e.confidence < 0.0) return false;
  return true;
}

Severity classify_severity(double degradation) noexcept {
  if (!std::isfinite(degradation)) return Severity::UNKNOWN;
  if (degradation <= 0.0) return Severity::NONE;
  if (degradation < 0.10) return Severity::MINOR;
  if (degradation < 0.25) return Severity::MODERATE;
  if (degradation < 0.50) return Severity::SEVERE;
  return Severity::CRITICAL;
}

}  // namespace contention
