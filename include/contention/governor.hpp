// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Thread-safe public governor facade. Never exposes sockets, locks, or process
// handles. External calls (adapters, file I/O) are performed outside internal
// locks. RAII-managed.

#ifndef CONTENTION_GOVERNOR_GOVERNOR_HPP
#define CONTENTION_GOVERNOR_GOVERNOR_HPP

#include "contention/adapters.hpp"
#include "contention/digest.hpp"
#include "contention/engine.hpp"
#include "contention/evidence.hpp"
#include "contention/policy.hpp"
#include "contention/state.hpp"
#include "contention/status.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace contention {

class Governor {
 public:
  struct Options {
    Clock* clock {nullptr};                 // injectable test clock
    AdapterRegistry* adapters {nullptr};    // reference adapters (may be null)
    bool persistEnable {false};
    std::string persistPath;
  };

  Governor();
  explicit Governor(Options opts);
  ~Governor();

  Governor(const Governor&) = delete;
  Governor& operator=(const Governor&) = delete;
  Governor(Governor&&) noexcept;
  Governor& operator=(Governor&&) noexcept;

  // Load persisted state (if enabled) and arm the governor.
  Status open();

  // Authority / generations.
  Status set_authority(const AuthorityContext& a);
  Status set_policy(const ContentionPolicy& p);
  Status record_worker_boot(WorkloadId w, WorkerBootId boot);
  Status record_workload_generation(WorkloadId w, WorkloadGeneration g);
  // Coordinator restart: advance epoch, mark current dynamic evidence
  // REVALIDATION_REQUIRED, and reconcile in-flight actions.
  Status advance_epoch();

  // Evidence.
  Status ingest_evidence(const InterferenceEvidence& e);

  // Evaluation (pure, deterministic).
  ConflictEvaluation evaluate(ConflictId id);

  // Authorization + dispatch. Pre-dispatch revalidation runs inside.
  // Returns the created intervention id (so dispatch(id) re-validates before
  // the adapter is invoked), or nullopt on failure.
  std::optional<InterventionId> authorize(ConflictId id, InterventionClass cls);
  Status authorize_and_dispatch(ConflictId id, InterventionClass cls);
  Status dispatch(InterventionId id);

  // Lifecycle callbacks.
  Status on_acknowledge(InterventionId id, AttemptId attempt);
  Status on_action_failed(InterventionId id, AttemptId attempt, const std::string& note);
  Status submit_post_action_evidence(const InterferenceEvidence& e);

  // Queries.
  std::vector<ConflictId> conflict_ids() const;
  std::optional<ContentionConflict> conflict(ConflictId id) const;
  std::optional<InterventionRecord> intervention(InterventionId id) const;
  std::optional<FairnessRecord> fairness(WorkloadId w) const;
  ActionBudget action_budget() const;

  // Persistence / replay.
  Status save();
  Status load();
  Digest64 digest() const;
  std::size_t persistence_bytes() const;   // size of serialized snapshot

  // Durable state exposure for advanced tests (read-only).
  const GovernorState* snapshot() const;

  void set_adapters(AdapterRegistry* adapters) { adapters_ = adapters; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  AdapterRegistry* adapters_ {nullptr};
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_GOVERNOR_HPP
