set(CONTENTION_EXAMPLES
  basic_pairwise
  hard_slo
  fairness_floor
  value_based
  stale_evidence
  confounded
  predispatch_invalidation
  ineffective_action
  shifted_contention
  multiparty
  worker_restart
  coordinator_restart
)

foreach(e ${CONTENTION_EXAMPLES})
  add_executable(${e} ${e}.cpp)
  target_link_libraries(${e} PRIVATE ContentionGovernor::ContentionGovernor)
endforeach()
