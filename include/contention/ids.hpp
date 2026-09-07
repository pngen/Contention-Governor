// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Strongly typed identity/generation domains. Each distinct semantic identity
// domain is a distinct C++ type so raw integers from different domains cannot be
// silently interchanged. Generations are monotonic, copyable, comparable values.

#ifndef CONTENTION_GOVERNOR_IDS_HPP
#define CONTENTION_GOVERNOR_IDS_HPP

#include <cstdint>

namespace contention {

// A strongly typed identity: a value in a single named domain. Two different
// domains are distinct types and cannot be compared or converted implicitly.
template <typename Tag>
struct Id {
  using Value = std::uint64_t;

  Value value_ {0};

  constexpr Id() noexcept = default;
  constexpr explicit Id(Value v) noexcept : value_(v) {}

  constexpr Value value() const noexcept { return value_; }
  constexpr explicit operator bool() const noexcept { return value_ != 0; }

  // Monotonic advancement. Saturates rather than wrapping.
  constexpr Id next() const noexcept {
    return Id(value_ == UINT64_MAX ? UINT64_MAX : value_ + 1);
  }
  constexpr Id& advance() noexcept {
    if (value_ != UINT64_MAX) ++value_;
    return *this;
  }

  friend constexpr bool operator==(Id a, Id b) noexcept { return a.value_ == b.value_; }
  friend constexpr bool operator!=(Id a, Id b) noexcept { return a.value_ != b.value_; }
  friend constexpr bool operator<(Id a, Id b) noexcept { return a.value_ < b.value_; }
  friend constexpr bool operator<=(Id a, Id b) noexcept { return a.value_ <= b.value_; }
  friend constexpr bool operator>(Id a, Id b) noexcept { return a.value_ > b.value_; }
  friend constexpr bool operator>=(Id a, Id b) noexcept { return a.value_ >= b.value_; }
};

#define CONTENTION_ID_DECL(name) \
  struct name##_Tag; \
  using name = Id<name##_Tag>;

// Identity domains.
CONTENTION_ID_DECL(CoordinatorEpoch)
CONTENTION_ID_DECL(GovernorId)
CONTENTION_ID_DECL(GovernorGeneration)
CONTENTION_ID_DECL(WorkerId)
CONTENTION_ID_DECL(WorkerBootId)
CONTENTION_ID_DECL(HostId)
CONTENTION_ID_DECL(HostGeneration)
CONTENTION_ID_DECL(DeviceId)
CONTENTION_ID_DECL(DeviceGeneration)
CONTENTION_ID_DECL(WorkloadId)
CONTENTION_ID_DECL(WorkloadGeneration)
CONTENTION_ID_DECL(ConflictId)
CONTENTION_ID_DECL(ConflictGeneration)
CONTENTION_ID_DECL(InterferenceEvidenceId)
CONTENTION_ID_DECL(InterferenceEvidenceGeneration)
CONTENTION_ID_DECL(PolicyId)
CONTENTION_ID_DECL(PolicyGeneration)
CONTENTION_ID_DECL(SLOGeneration)
CONTENTION_ID_DECL(ValueGeneration)
CONTENTION_ID_DECL(FairnessGeneration)
CONTENTION_ID_DECL(ActionBudgetGeneration)
CONTENTION_ID_DECL(ResourceGeneration)
CONTENTION_ID_DECL(PlacementGeneration)
CONTENTION_ID_DECL(ReservationGeneration)
CONTENTION_ID_DECL(TopologyGeneration)
CONTENTION_ID_DECL(InterventionId)
CONTENTION_ID_DECL(InterventionGeneration)
CONTENTION_ID_DECL(DispatchId)
CONTENTION_ID_DECL(AttemptId)
CONTENTION_ID_DECL(AttemptGeneration)
CONTENTION_ID_DECL(VerificationId)
CONTENTION_ID_DECL(VerificationGeneration)

// An experiment key identifying a specific distributed test/run epoch+replica.
struct ExperimentKey {
  std::uint64_t coordinator {0};
  std::uint64_t replica {0};

  friend constexpr bool operator==(ExperimentKey a, ExperimentKey b) noexcept {
    return a.coordinator == b.coordinator && a.replica == b.replica;
  }
  friend constexpr bool operator!=(ExperimentKey a, ExperimentKey b) noexcept {
    return !(a == b);
  }
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_IDS_HPP
