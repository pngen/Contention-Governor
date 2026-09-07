// Core unit tests: conflicts, authority, lifecycle, determinism, closed loop.
#include "framework.hpp"
#include "testutil.hpp"
#include "contention/cg.hpp"

#include <cmath>

namespace {

using namespace contention;

}  // namespace

TEST(evidence_validation_rejects_nan) {
  using namespace contention;
  InterferenceEvidence e;
  e.victim = WorkloadId{1};
  e.neighbors = {WorkloadId{2}};
  e.isolatedBaseline = 100.0;
  e.coRunObservation = std::numeric_limits<double>::quiet_NaN();
  e.confidence = 0.8;
  e.sampleCount = 5;
  Status st = validate_evidence(e);
  CHECK(st.code != StatusCode::OK);
}

TEST(evidence_validation_rejects_confidence_out_of_range) {
  using namespace contention;
  InterferenceEvidence e;
  e.victim = WorkloadId{1};
  e.neighbors = {WorkloadId{2}};
  e.isolatedBaseline = 100.0;
  e.coRunObservation = 70.0;
  e.confidence = 1.5;
  e.sampleCount = 5;
  Status st = validate_evidence(e);
  CHECK(st.code != StatusCode::OK);
}

TEST(degradation_is_directional) {
  using namespace contention;
  // higher-is-better: 30% loss.
  CHECK(compute_degradation(MetricDirection::HIGHER_IS_BETTER, 100.0, 70.0) > 0.299);
  CHECK(compute_degradation(MetricDirection::HIGHER_IS_BETTER, 100.0, 70.0) < 0.301);
  // lower-is-better: 30% more latency is degradation too.
  CHECK(compute_degradation(MetricDirection::LOWER_IS_BETTER, 100.0, 130.0) > 0.299);
  // improved throughput should not be degradation.
  CHECK(compute_degradation(MetricDirection::HIGHER_IS_BETTER, 100.0, 120.0) == 0.0);
}

TEST(severity_classification) {
  using namespace contention;
  CHECK(classify_severity(0.05) == Severity::MINOR);
  CHECK(classify_severity(0.15) == Severity::MODERATE);
  CHECK(classify_severity(0.30) == Severity::SEVERE);
  CHECK(classify_severity(0.70) == Severity::CRITICAL);
  CHECK(classify_severity(-0.1) == Severity::NONE);
}

TEST(policy_validation_rejects_release_at_or_above_actionable) {
  auto p = ctest::make_policy();
  p.releaseThreshold = 0.25;
  p.actionabilityThreshold = 0.20;
  Status st = validate_policy(p);
  CHECK(st.code != StatusCode::OK);
}

TEST(no_current_evidence_means_no_destructive_action) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  // Evidence is STALE -> not CURRENT -> must not authorize destructive action.
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON,
                                0.9, EvidenceStatus::STALE);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  CHECK(ids.size() == 1);
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.code == StatusCode::INSUFFICIENT_EVIDENCE || st.code == StatusCode::NO_LEGAL_INTERVENTION ||
        st.code == StatusCode::CONFOUNDED);
}

TEST(stale_authority_rejects_dispatch) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  CHECK(ids.size() == 1);
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.ok());
  // Advance coordinator epoch -> authority stale; the recorded intervention fence
  // should now reject as stale.
  g.advance_epoch();
  // A new authorization under the new epoch should also be revalidated.
  Status st2 = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st2.code == StatusCode::STALE_AUTHORITY || st2.code == StatusCode::INSUFFICIENT_EVIDENCE ||
        st2.code == StatusCode::NO_LEGAL_INTERVENTION);
}

TEST(non_preemptible_rejects_preemption) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0, /*preemptible=*/false);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  // Requesting preemption of non-preemptible B must be rejected / not selected.
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::REQUEST_PREEMPTION);
  CHECK(st.code == StatusCode::HARD_CONSTRAINT_VIOLATION || st.code == StatusCode::NO_LEGAL_INTERVENTION);
}

TEST(deterministic_evaluation_identical_state) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  ConflictEvaluation a = g.evaluate(ids[0]);
  ConflictEvaluation b = g.evaluate(ids[0]);
  CHECK(a.decision.has_value());
  CHECK(b.decision.has_value());
  CHECK(a.decision->chosen.cls == b.decision->chosen.cls);
  CHECK(a.decision->chosen.target == b.decision->chosen.target);
}

TEST(acknowledged_is_not_effective) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.ok());
  // The reference adapter ACCEPTS, so the intervention is ACKNOWLEDGED -- NOT EFFECTIVE.
  auto c = g.conflict(ids[0]);
  CHECK(c.has_value() && !c->selectedIds.empty());
  auto iv = g.intervention(c->selectedIds[0]);
  CHECK(iv.has_value());
  CHECK(iv->state != LifecycleState::EFFECTIVE);
  CHECK(iv->state == LifecycleState::ACKNOWLEDGED || iv->state == LifecycleState::DISPATCHED ||
        iv->state == LifecycleState::OUTCOME_UNKNOWN);
  // Now supply fresh post-action evidence showing recovery.
  auto pe = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                 100.0, 98.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.submit_post_action_evidence(pe);
  iv = g.intervention(c->selectedIds[0]);
  CHECK(iv.has_value());
  CHECK(iv->state == LifecycleState::EFFECTIVE);
}

TEST(closed_loop_resolves) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  auto ids = g.conflict_ids();
  Status st = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
  CHECK(st.ok());
  // Fresh post-action evidence: A recovers (improved throughput).
  auto pe = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                 100.0, 98.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.submit_post_action_evidence(pe);
  // The conflict degradation should now be below the actionability threshold.
  auto c = g.conflict(ids[0]);
  CHECK(c.has_value());
  CHECK(c->degradation < pol.actionabilityThreshold);
}

TEST(fairness_blocks_repeated_target) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  pol.maxConsecutiveInterventionsPerWorkload = 2;
  g.set_policy(pol);
  // Ingest and dispatch repeatedly; after hitting the cap the target should be
  // blocked by fairness.
  for (int i = 0; i < 5; ++i) {
    auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                  100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
    g.ingest_evidence(e);
  }
  auto ids = g.conflict_ids();
  int okCount = 0;
  Status last;
  for (int i = 0; i < 5; ++i) {
    last = g.authorize_and_dispatch(ids[0], InterventionClass::THROTTLE);
    if (last.ok()) ++okCount;
  }
  // maxConsecutiveInterventionsPerWorkload == 2 so exactly 2 should succeed.
  CHECK(okCount == 2);
  CHECK(last.code == StatusCode::FAIRNESS_LIMIT || last.code == StatusCode::ACTION_BUDGET_EXHAUSTED ||
        last.code == StatusCode::NO_LEGAL_INTERVENTION);
  auto fr = g.fairness(WorkloadId{2});
  CHECK(fr.has_value());
  CHECK(fr->interventionCount >= 1);
}

TEST(digest_is_deterministic) {
  using namespace contention;
  ManualClock clock(0);
  ctest::RefRegistry reg;
  Governor g = ctest::make_governor(clock, reg);
  auto auth = ctest::make_authority(WorkloadId{1}, WorkloadId{2});
  g.set_authority(auth);
  auto pol = ctest::make_policy();
  ctest::add_workload(pol, WorkloadId{1}, 100.0);
  ctest::add_workload(pol, WorkloadId{2}, 1.0);
  g.set_policy(pol);
  auto e = ctest::make_evidence(WorkloadId{1}, WorkloadId{2}, ResourceDomain::COMPUTE,
                                100.0, 60.0, AttributionStrength::CONTROLLED_COMPARISON, 0.9);
  g.ingest_evidence(e);
  Digest64 d1 = g.digest();
  Digest64 d2 = g.digest();
  CHECK(d1 == d2);
}
