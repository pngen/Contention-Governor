// Contention Governor
// Copyright 2026 Summon Software Labs.
// SPDX-License-Identifier: Apache-2.0
//
// Narrow typed enforcement-adapter interfaces. Contention Governor emits
// intents; the adjacent runtime performs the mechanism. Reference adapters in
// this project operate against controlled test workers and are NOT production
// mechanisms.

#ifndef CONTENTION_GOVERNOR_ADAPTERS_HPP
#define CONTENTION_GOVERNOR_ADAPTERS_HPP

#include "contention/enums.hpp"
#include "contention/ids.hpp"
#include "contention/status.hpp"

#include <string>

namespace contention {

// The result of requesting an adapter to apply an intent.
enum class AdapterOutcome : std::uint8_t {
  ACCEPTED,       // mechanism acknowledged the intent (not proof of effect)
  REJECTED,       // mechanism refused
  UNAVAILABLE,    // no adapter present
  UNKNOWN         // acknowledgment lost / outcome unknown
};

const char* to_string(AdapterOutcome v) noexcept;

struct AdapterResult {
  AdapterOutcome outcome {AdapterOutcome::UNAVAILABLE};
  std::string note;
  AttemptId attemptId;
};

// A throttle intent (reference intensity control).
class IThrottleAdapter {
 public:
  virtual ~IThrottleAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls, double intensity) = 0;
};

// Admission control intent.
class IAdmissionAdapter {
 public:
  virtual ~IAdmissionAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// Preemption intent.
class IPreemptionAdapter {
 public:
  virtual ~IPreemptionAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// Placement intent.
class IPlacementAdapter {
 public:
  virtual ~IPlacementAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// Bandwidth intent.
class IBandwidthAdapter {
 public:
  virtual ~IBandwidthAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// Communication-plan intent.
class ICommunicationAdapter {
 public:
  virtual ~ICommunicationAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// Collective-scheduling intent.
class ICollectiveAdapter {
 public:
  virtual ~ICollectiveAdapter() = default;
  virtual std::string name() const = 0;
  virtual AdapterResult apply(AttemptId attemptId, WorkloadId target,
                              InterventionClass cls) = 0;
};

// A registry that binds an intervention class to the adapter that performs it.
class AdapterRegistry {
 public:
  virtual ~AdapterRegistry() = default;
  virtual IThrottleAdapter* throttle() { return nullptr; }
  virtual IAdmissionAdapter* admission() { return nullptr; }
  virtual IPreemptionAdapter* preemption() { return nullptr; }
  virtual IPlacementAdapter* placement() { return nullptr; }
  virtual IBandwidthAdapter* bandwidth() { return nullptr; }
  virtual ICommunicationAdapter* communication() { return nullptr; }
  virtual ICollectiveAdapter* collective() { return nullptr; }
};

}  // namespace contention

#endif  // CONTENTION_GOVERNOR_ADAPTERS_HPP
