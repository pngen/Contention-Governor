// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0

#include "contention/replay.hpp"

#include "contention/engine.hpp"

namespace contention {

ReplayReport replay(const GovernorState& s) {
  ReplayReport report;
  for (const auto& [cid, c] : s.engine.conflicts) {
    ++report.conflictsEvaluated;
    ConflictEvaluation ev = evaluate_conflict(s.engine, cid, s.engine.authority);
    report.candidatesBuilt += ev.candidates.size();

    bool ok = true;
    if (c.selectedIds.empty()) {
      // Nothing was ever recorded as selected; replay has nothing to refute.
      ok = true;
    } else if (ev.decision) {
      // A recorded selection must match the replayed decision.
      const InterventionClass replayed = ev.decision->chosen.cls;
      const WorkloadId replayedTarget = ev.decision->chosen.target;
      bool matched = false;
      for (auto iid : c.selectedIds) {
        auto it = s.engine.interventions.find(iid);
        if (it != s.engine.interventions.end() && it->second.cls == replayed &&
            it->second.target == replayedTarget) {
          matched = true;
        }
      }
      ok = matched;
    } else {
      // A recorded selection exists but replay produces no decision: divergent.
      ok = false;
    }
    if (!ok) report.diverged.push_back(cid);
  }
  report.status = report.diverged.empty()
                      ? Status::good()
                      : Status::bad(StatusCode::INVALID_INPUT, "replay diverged");
  return report;
}

}  // namespace contention
