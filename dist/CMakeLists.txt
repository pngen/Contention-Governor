# Distributed multiprocess proof helper executables.
find_package(Threads REQUIRED)

add_executable(cg_dist_worker worker.cpp protocol.cpp)
target_link_libraries(cg_dist_worker PRIVATE ContentionGovernor::ContentionGovernor Threads::Threads)
target_include_directories(cg_dist_worker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(cg_dist_coordinator coordinator.cpp protocol.cpp)
target_link_libraries(cg_dist_coordinator PRIVATE ContentionGovernor::ContentionGovernor Threads::Threads)
target_include_directories(cg_dist_coordinator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
