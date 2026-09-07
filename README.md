# Contention Governor

Contention Governor is an open-source, vendor-neutral C++20 runtime for actively
resolving cross-workload accelerator contention using interference evidence,
policy, SLOs, workload value, fairness, and generation-fenced enforcement across
shared compute, memory, bandwidth, PCIe, collective, NUMA, and storage resources.

Its core systems question is:

> Given authoritative evidence that workloads are interfering with one another,
> which workload should yield, by how much, through which legal intervention,
> under whose authority, and how do we prove that the intervention actually
> improved the binding objective without creating a worse secondary violation?

The thesis is:

> Observing contention is not resolving contention.

A workload may be causing a 30% throughput loss in a higher-value neighbor. Two
latency-sensitive services may both be legal individually but incompatible when
co-running. A memory-intensive batch job may destroy p99 for interactive
inference. A collective may monopolize a communication path needed by
deadline-sensitive traffic. A workload may be cheap to slow down but expensive to
migrate. A lower-priority job may be preemptible but expensive to resume. A
throughput-maximizing action may violate fairness. A latency-saving intervention
may exceed a hard cost budget. A workload may appear to be the aggressor while
the real cause is stale or confounded interference evidence. An intervention that
was correct when authorized may become invalid before dispatch.

Contention Governor turns interference evidence into bounded, deterministic,
policy-governed action. It does not merely recommend: it authorizes interventions
through explicit runtime boundaries, fences stale action, verifies execution
outcomes, and re-evaluates the conflict after intervention.

## Systems boundary

Contention Governor owns:

- contention-conflict representation;
- interference-evidence ingestion and validation;
- workload conflict sets;
- affected/victim and associated/aggressor identification;
- resource-domain identification;
- conflict severity;
- hard and soft policy constraints;
- SLO-, value-, priority-, fairness-, and cost-aware conflict resolution;
- intervention feasibility, candidate generation, deterministic ranking;
- intervention authority and generation fencing;
- pre-dispatch revalidation;
- typed enforcement adapters;
- intervention lifecycle and ambiguous-outcome handling;
- post-action observation and closed-loop verification;
- secondary-violation and shifted-contention detection;
- hysteresis, cooldown, anti-flapping, and action budgets;
- deterministic explanations and what-would-change analysis;
- persistence, restart, replay, and canonical digest.

Contention Governor does NOT own measuring interference, generic utilization
monitoring, SLO definition, monetary optimization, quota ownership, generic
admission control, scheduling or placement implementation, safe preemption
mechanics, bandwidth arbitration, communication path planning, collective
execution mechanics, memory compaction, fragmentation remediation, recovery
execution, device partitioning, NVLink/NVSwitch routing, or hardware power
control.

**Interference Observatory** is the previous layer. It owns OBSERVE, MEASURE,
COMPARE, ATTRIBUTE, and EXPLAIN cross-workload interference. Contention Governor
consumes authoritative Interference-Observatory-compatible evidence and decides
what corrective action is legal and preferable. It does not rebuild the
observatory.

Adjacent runtimes own the mechanisms. Contention Governor emits intents through
narrow typed adapters (`IThrottleAdapter`, `IAdmissionAdapter`,
`IPreemptionAdapter`, `IPlacementAdapter`, `IBandwidthAdapter`,
`ICommunicationAdapter`, `ICollectiveAdapter`). Reference adapters in this
repository operate against controlled test workers; they are NOT production
mechanisms.

## Core principles

- No interference evidence, no contention intervention.
- Correlation alone must not authorize destructive action.
- Hard constraints dominate optimization.
- Resolution must be deterministic for identical canonical state.
- Fairness must be explicit; workload value must be explicit.
- Priority is not automatically value; cost is not automatically value.
- SLO urgency is not automatically absolute priority unless policy says so.
- A legal intervention can still be a bad intervention: closed-loop verification
  is mandatory.
- ACKNOWLEDGED is not EFFECTIVE.
- Stale action must reject; UNKNOWN remains UNKNOWN.
- Do not fabricate a causal aggressor. Multiple workloads may jointly create the
  conflict. Contention may be asymmetric.
- Resolution must not flap.
- Intervention collateral damage must be measured.
- Current evidence after restart requires revalidation.

## Conflict model

Conflicts are explicit objects carrying a `ConflictId` and generation, the
affected (victim) workloads, the associated (interfering) workloads where
justified, the shared resource domain, the degradation magnitude, the binding
objective, severity, confidence, provenance, freshness, the active interval,
policy, current authority, unresolved confounders, candidate interventions, and
a state. States include DETECTED, VALIDATING, ACTIONABLE,
INTERVENTION_AUTHORIZED, INTERVENTION_IN_FLIGHT, VERIFYING, RESOLVED,
PARTIALLY_RESOLVED, UNRESOLVED, SUPPRESSED, REVALIDATION_REQUIRED, SUPERSEDED, and
UNKNOWN.

Directionality is preserved: "A harms B by 30%, B harms A by 3%" is never
averaged. Multi-party conflicts are representable; when the residual joint
effect of a group exceeds pairwise attribution, it is marked
`groupEffectExplicit` and no exact per-workload responsibility split is asserted.

## Evidence input contract

Evidence carries the victim, neighbors, interference domain, isolated baseline,
co-run observation, degradation magnitude, attribution strength, confidence,
sample count, unresolved confounders, freshness, source health, and the worker
boot, workload, device, host, topology, resource, and placement generations.
Active control is only ever authorized from CURRENT evidence.
STALE/EXPIRED/REVALIDATION_REQUIRED/UNSUPPORTED/UNKNOWN evidence cannot authorize
active control. Low-confidence evidence yields NO_ACTION / OBSERVE_MORE /
REQUEST_REMEASUREMENT / MANUAL_INTERVENTION_REQUIRED, never destructive
enforcement.

## Policy, value, and fairness model

Policy is a generation-bound `ContentionPolicy` with an `ActionBudget`, and a
`WorkloadPolicy` per workload. Hard constraints include hard SLO floors,
non-preemptibility, protected classes, minimum guaranteed share, hard quota, hard
reservation, hard cost ceiling, safety/compatibility constraints, and resource
unavailability. Soft constraints include preferred fairness, cost preference,
energy, throughput/latency preference, and utilization targets. Hard constraints
are applied first; a cheaper or faster action that violates a hard constraint is
infeasible.

Value is explicit and typed (priority, service class, SLO criticality, deadline
urgency, configured importance). It is never fabricated as a hidden monetary
scalar. Fairness is explicit: minimum service floor, weighted fairness, bounded
deprivation, and max-consecutive-interventions protection. Cumulative
intervention burden (`interventionCount`, `consecutiveInterventions`,
`deprivationDebt`) is tracked per workload and the fair floor is never violated:
a lower-priority workload cannot be suppressed forever unless explicit policy
legally permits it.

## Intervention classes

Typed classes include NO_ACTION, OBSERVE_MORE, THROTTLE, DEFER_NEW_WORK,
REDUCE_CONCURRENCY, REDUCE_BATCH, SERIALIZE, TIME_SLICE, REQUEST_PREEMPTION,
REQUEST_MIGRATION, REQUEST_REPLACEMENT, CHANGE_PLACEMENT,
REQUEST_BANDWIDTH_REALLOCATION, REQUEST_COMMUNICATION_REPLAN,
REQUEST_COLLECTIVE_RESCHEDULE, RELEASE_OPTIONAL_RESIDENCY,
REDUCE_MEMORY_PRESSURE, SHED_LOW_VALUE_WORK, PROTECT_HIGH_VALUE_WORK, ESCALATE,
and MANUAL_INTERVENTION_REQUIRED. Only adapters with clean semantics are
implemented; unsupported classes surface as UNAVAILABLE.

## Feasibility and deterministic ranking

Before ranking, each candidate is validated for feasibility: a non-preemptible
workload cannot be preempted; a workload cannot be throttled below its hard
service floor; an action cannot exceed a hard cost budget; stale evidence or a
stale generation cannot authorize an action. Each candidate exposes an explicit
effect model (expected conflict reduction, target improvement, neighbor
degradation, cost, recovery cost, confidence, reversibility, time to effect).

Ranking is deterministic: validate evidence, identify the binding objective,
build legal interventions, remove hard-constraint violations, evaluate service
floors, evaluate fairness, evaluate expected conflict reduction, evaluate
collateral damage, evaluate value, evaluate cost, evaluate reversibility, then
break ties deterministically (a fixed class order, then ids). Identical canonical
state always yields the identical decision. Candidate enumeration order never
changes the result.

## Authority and pre-dispatch revalidation

Every intervention carries a generation fence covering CoordinatorEpoch,
GovernorGeneration, ConflictGeneration, InterferenceEvidenceGeneration,
PolicyGeneration, SLOGeneration, ValueGeneration, FairnessGeneration,
ResourceGeneration, PlacementGeneration, TopologyGeneration, WorkerBootId,
WorkloadGeneration, and InterventionGeneration. Stale action rejects without
mutation. Before dispatch the governor revalidates that the fence is still
current, the conflict is still active, evidence is still current, the target
still exists, the action is still feasible, and no stronger constraint has
appeared. If anything changed, the action is rejected and the conflict is
re-evaluated.

## Intervention lifecycle and closed-loop verification

Lifecycle states are PROPOSED, AUTHORIZED, DISPATCHED, ACKNOWLEDGED, EFFECTIVE,
PARTIALLY_EFFECTIVE, INEFFECTIVE, FAILED, CANCELLED, SUPERSEDED, EXPIRED, and
OUTCOME_UNKNOWN. ACKNOWLEDGED is distinct from EFFECTIVE. Terminal states remain
terminal.

Closed-loop verification requires fresh post-action evidence. The outcome is
classified as RESOLVED, PARTIALLY_RESOLVED, INEFFECTIVE, WORSENED,
SHIFTED_CONTENTION, CREATED_SECONDARY_VIOLATION, CREATED_FAIRNESS_VIOLATION,
INSUFFICIENT_POST_ACTION_EVIDENCE, or OUTCOME_UNKNOWN. An intervention is never
called successful merely because it executed.

## Shifted contention

A critical proof obligation is that an action may solve one conflict by moving
pressure elsewhere (compute to PCIe, latency to throughput). The governor
detects secondary hard violations and shifted contention and never declares
RESOLVED if the system simply moved the violation.

## Hysteresis, cooldown, and action budgets

Policy-defined hysteresis determines when a conflict becomes actionable
(default 20% degradation) and when it may return to unconstrained (default 10%
for sufficient evidence). Disruptive actions respect a cooldown. Action budgets
place exact, generation-aware caps on preemptions, migrations, placement changes,
throttles, collective reschedules, and total actions per window; accounting never
underflows or overflows.

## Real multiprocess proof

The distributed proof uses independent OS processes (a coordinator and worker A
and worker B) over real TCP loopback. The protocol is framed, versioned, bounded,
checksummed, partial-read safe, partial-write safe, concurrent-write safe,
malformed-frame safe, and oversized-frame safe. Scenarios cover basic
resolution, hard constraints, fairness, stale evidence, worker death/restart with
a fresh `WorkerBootId`, shifted contention, confounded evidence, real coordinator
restart with a new `CoordinatorEpoch`, surviving worker reconnect and
republish, old-epoch rejection, ambiguous action outcomes, and multi-party
conflicts.

## REAL / CONTROLLED / DERIVED / POLICY / SYNTHETIC / UNSUPPORTED

Evidence is labelled explicitly. The core is CUDA-free. No hardware-control
fabrication is made: GPU SM partitioning, memory-bandwidth QoS, NVLink throttling,
PCIe hardware arbitration, MIG repartitioning, hardware collective scheduling, and
provider-specific isolation are all UNSUPPORTED. A cooperative worker throttle is
REAL software control, not hardware bandwidth enforcement.

## Persistence and replay

Durable state (policy, conflicts, evidence, intervention history, fairness debt,
action budgets, stable identities, digests) is persisted in a versioned,
CRC-32C-checked, bounded format with atomic save/replace. Corruption, truncation,
trailing garbage, unknown versions, and absurd sizes are rejected before
allocation. Same canonical history and policy inputs reconstruct the same
conflict states, candidates, rejected reasons, selected action, fairness
accounting, verification outcomes, and canonical digest.

## Build, install, and examples

Requires CMake 3.20+ and a C++20 compiler. Windows with MSVC is first-class;
Linux/GCC/Clang is portable where practical.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Install and consume with `find_package`:

```sh
cmake --install build --prefix <prefix>
```

```cmake
find_package(ContentionGovernor CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE ContentionGovernor::ContentionGovernor)
```

The CLI (`contention_cli`) provides `conflicts`, `show-conflict`, `evaluate`,
`decision`, `digest`, `history`, `fairness`, `action-budget`, `replay`, and
`validate-state`. Runnable examples cover basic pairwise resolution, hard SLO,
fairness floor, value-based resolution, stale evidence, confounded evidence,
pre-dispatch invalidation, ineffective action, shifted contention, multi-party
conflict, worker restart, and coordinator restart.

## Limitations

- The reference throttle adapter is a cooperative software control against
  controlled test workers; it is not a production hardware mechanism.
- Hardware bandwidth, NVLink, PCIe, MIG, and vendor-scheduler enforcement are
  not implemented.
- Real CUDA contention control is isolated behind a separate proof (`cuda_proof/cuda_proof.cu`) and is only built where a CUDA-capable device is available. It has been exercised on an NVIDIA RTX 5090 / Blackwell (sm_120, CUDA 13) and reported a real measured co-run throughput loss with honest memory closure.
- The CUDA proof builds with the Ninja generator + `nvcc`; the CMake options are gated by `CONTENTION_BUILD_CUDA_PROOF`.
- Closed-loop outcome classification uses the deterministic post-action evidence
  that the runtime supplies; where hardware noise dominates, an honest
  NO_INTERFERENCE_DETECTED result is reported rather than a fabricated one.

## License

Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.