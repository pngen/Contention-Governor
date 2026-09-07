set(CONTENTION_TESTS
  core_tests
  property_tests
  adversarial_tests
  concurrency_tests
  persistence_tests
  distributed_tests
)

find_package(Threads REQUIRED)

foreach(t ${CONTENTION_TESTS})
  set(extra "")
  if(t STREQUAL "distributed_tests")
    set(extra "${CMAKE_CURRENT_SOURCE_DIR}/../dist/protocol.cpp")
  endif()
  add_executable(${t} src/main.cpp src/${t}.cpp ${extra})
  target_link_libraries(${t} PRIVATE ContentionGovernor::ContentionGovernor Threads::Threads)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../dist)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# Network protocol unit tests (loopback, no external deps).
add_executable(net_tests src/main.cpp src/net_tests.cpp)
target_link_libraries(net_tests PRIVATE ContentionGovernor::ContentionGovernor Threads::Threads)
target_include_directories(net_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME net_tests COMMAND net_tests)
