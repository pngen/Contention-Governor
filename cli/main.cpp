// Contention Governor CLI.
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/cg.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace contention;

int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf("Contention Governor CLI %s\n", version());
    std::printf("commands: conflicts | show-conflict <id> | evaluate <id> | candidates <id> | decision <id> | digest | history | fairness | action-budget | replay | validate-state\n");
    return 0;
  }
  std::string cmd = argv[1];
  std::string statePath;
  for (int i = 2; i < argc; ++i) {
    if (std::strcmp(argv[i], "--state") == 0 && i + 1 < argc) statePath = argv[++i];
  }

  Governor g(Governor::Options{nullptr, nullptr, !statePath.empty(), statePath});
  if (!statePath.empty()) {
    Status st = g.load();
    if (!st.ok()) { std::fprintf(stderr, "load failed: %s\n", to_string(st.code)); return 1; }
  }

  if (cmd == "conflicts") {
    for (auto id : g.conflict_ids())
      std::printf("conflict %llu\n", static_cast<unsigned long long>(id.value()));
    return 0;
  }
  if (cmd == "show-conflict") {
    auto id = argc > 2 ? ConflictId{std::strtoull(argv[2], nullptr, 10)} : ConflictId{};
    auto c = g.conflict(id);
    if (!c) { std::printf("unknown conflict\n"); return 1; }
    std::printf("id=%llu domain=%s degradation=%.4f severity=%s state=%s binding=%s\n",
                static_cast<unsigned long long>(c->id.value()), to_string(c->domain),
                c->degradation, to_string(c->severity), to_string(c->state), to_string(c->binding));
    return 0;
  }
  if (cmd == "evaluate") {
    auto id = argc > 2 ? ConflictId{std::strtoull(argv[2], nullptr, 10)} : ConflictId{};
    ConflictEvaluation ev = g.evaluate(id);
    std::printf("state=%s degradation=%.4f status=%s candidates=%zu\n",
                to_string(ev.state), ev.degradation, to_string(ev.status.code),
                ev.candidates.size());
    if (ev.decision)
      std::printf("decision=%s target=%llu\n", to_string(ev.decision->chosen.cls),
                  static_cast<unsigned long long>(ev.decision->chosen.target.value()));
    return 0;
  }
  if (cmd == "decision") {
    auto id = argc > 2 ? ConflictId{std::strtoull(argv[2], nullptr, 10)} : ConflictId{};
    ConflictEvaluation ev = g.evaluate(id);
    if (ev.decision) {
      std::printf("%s target=%llu\n", to_string(ev.decision->chosen.cls),
                  static_cast<unsigned long long>(ev.decision->chosen.target.value()));
      return 0;
    }
    std::printf("NO_LEGAL_INTERVENTION\n");
    return 1;
  }
  if (cmd == "digest") {
    std::printf("digest=%016llx\n", static_cast<unsigned long long>(g.digest()));
    return 0;
  }
  if (cmd == "history") {
    auto cids = g.conflict_ids();
    for (auto cid : cids) {
      auto c = g.conflict(cid);
      if (!c) continue;
      for (auto iid : c->selectedIds) {
        auto iv = g.intervention(iid);
        if (iv) std::printf("intervention %llu conflict=%llu state=%s cls=%s result=%s\n",
                            static_cast<unsigned long long>(iv->id.value()),
                            static_cast<unsigned long long>(iv->conflictId.value()),
                            to_string(iv->state), to_string(iv->cls), to_string(iv->result));
      }
    }
    return 0;
  }
  if (cmd == "fairness") {
    auto cids = g.conflict_ids();
    for (auto cid : cids) {
      auto c = g.conflict(cid);
      if (!c) continue;
      for (auto w : c->affected) {
        auto fr = g.fairness(w);
        if (fr) std::printf("workload=%llu interventions=%llu consecutive=%u\n",
                            static_cast<unsigned long long>(fr->workload.value()),
                            static_cast<unsigned long long>(fr->interventionCount),
                            fr->consecutiveInterventions);
      }
    }
    return 0;
  }
  if (cmd == "action-budget") {
    auto ab = g.action_budget();
    std::printf("throttles=%llu/%llu preemptions=%llu/%llu all=%llu/%llu\n",
                static_cast<unsigned long long>(ab.used.throttles),
                static_cast<unsigned long long>(ab.maxThrottlesPerWindow),
                static_cast<unsigned long long>(ab.used.preemptions),
                static_cast<unsigned long long>(ab.maxPreemptionsPerWindow),
                static_cast<unsigned long long>(ab.used.all),
                static_cast<unsigned long long>(ab.maxAnyPerWindow));
    return 0;
  }
  if (cmd == "replay") {
    ReplayReport rp = replay(*g.snapshot());
    std::printf("conflicts=%zu diverged=%zu\n", rp.conflictsEvaluated, rp.diverged.size());
    return rp.status.ok() ? 0 : 1;
  }
  if (cmd == "validate-state") {
    Status v = validate_policy(g.snapshot()->engine.policy);
    if (!v.ok()) { std::printf("policy invalid: %s\n", to_string(v.code)); return 1; }
    std::printf("state valid; conflicts=%zu evidence=%zu interventions=%zu\n",
                g.snapshot()->engine.conflicts.size(), g.snapshot()->engine.evidence.size(),
                g.snapshot()->engine.interventions.size());
    return 0;
  }
  std::printf("unknown command '%s'\n", cmd.c_str());
  return 1;
}
