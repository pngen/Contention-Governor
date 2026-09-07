// Contention Governor benchmark: measure completed operations at scale.
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/cg.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace contention;
using clk = std::chrono::steady_clock;

static double ms(clk::time_point a, clk::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

static InterferenceEvidence make_ev(std::uint64_t id, WorkloadId victim, WorkloadId nb) {
  InterferenceEvidence e;
  e.id = InterferenceEvidenceId{id};
  e.generation = InterferenceEvidenceGeneration{1};
  e.victim = victim;
  e.neighbors = {nb};
  e.domain = ResourceDomain::COMPUTE;
  e.metric = MetricDirection::HIGHER_IS_BETTER;
  e.isolatedBaseline = 100.0;
  e.coRunObservation = 60.0;
  e.degradation = compute_degradation(e.metric, 100.0, 60.0);
  e.attribution = AttributionStrength::CONTROLLED_COMPARISON;
  e.confidence = 0.9;
  e.sampleCount = 100;
  e.status = EvidenceStatus::CURRENT;
  e.maxAgeMs = 5000;
  return e;
}

int main(int argc, char** argv) {
  std::vector<std::size_t> scales = {100, 1000, 10000, 100000};
  if (argc > 1) scales = {static_cast<std::size_t>(std::atoi(argv[1]))};

  std::printf("scale,ingest_ms,evaluate_ms,save_ms,load_ms\n");
  for (std::size_t n : scales) {
    ManualClock clock(0);
    Governor g(Governor::Options{&clock, nullptr, false, ""});
    AuthorityContext auth;
    auth.coordinatorEpoch = CoordinatorEpoch{1};
    auth.policyGeneration = PolicyGeneration{1};
    g.set_authority(auth);
    ContentionPolicy pol;
    pol.id = PolicyId{1}; pol.generation = PolicyGeneration{1};
    WorkloadPolicy wa; wa.workload = WorkloadId{1}; wa.value = 100.0;
    WorkloadPolicy wb; wb.workload = WorkloadId{2}; wb.value = 1.0;
    pol.workloads[WorkloadId{1}] = wa;
    pol.workloads[WorkloadId{2}] = wb;
    g.set_policy(pol);

    auto t0 = clk::now();
    for (std::size_t i = 0; i < n; ++i) {
      WorkloadId victim{1 + (i % 2)};
      WorkloadId nb{2 + (i % 2)};
      g.ingest_evidence(make_ev(100 + i, victim, nb));
    }
    auto t1 = clk::now();

    auto ids = g.conflict_ids();
    double evms = 0.0;
    std::size_t evcount = 0;
    auto t2 = clk::now();
    for (std::size_t i = 0; i < ids.size(); ++i) {
      g.evaluate(ids[i]);
      ++evcount;
      if (evcount >= 10000) break;
    }
    auto t3 = clk::now();
    evms = ms(t2, t3);

    std::string path = "bench_state.bin";
    auto t4 = clk::now();
    Governor g2(Governor::Options{&clock, nullptr, true, path});
    (void)g.save();
    auto t5 = clk::now();
    Governor g3(Governor::Options{&clock, nullptr, true, path});
    auto t6 = clk::now();
    (void)g3.load();
    auto t7 = clk::now();

    std::printf("%zu,%.2f,%.2f,%.2f,%.2f\n", n, ms(t0, t1), evms, ms(t4, t5), ms(t6, t7));
  }
  return 0;
}
