cmake_minimum_required(VERSION 3.20)
project(ContentionGovernor VERSION 1.0.0 LANGUAGES CXX)

option(CONTENTION_BUILD_TESTS "Build the full test suite" ON)
option(CONTENTION_BUILD_EXAMPLES "Build example programs" ON)
option(CONTENTION_BUILD_CLI "Build the command-line tool" ON)
option(CONTENTION_BUILD_BENCHMARKS "Build benchmarks" ON)
option(CONTENTION_BUILD_CUDA_PROOF "Build the optional isolated CUDA contention proof (needs CUDA)" OFF)
option(CONTENTION_STRICT_WARNINGS "Treat warnings as errors with strict flags" ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Platform compiler flags. Windows-first MSVC; portable GCC/Clang.
if(MSVC)
  add_compile_options(/W4)
  if(CONTENTION_STRICT_WARNINGS)
    add_compile_options(/WX)
    add_compile_options(/permissive-)
  endif()
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CONTENTION_STRICT_WARNINGS)
    add_compile_options(-Werror)
  endif()
endif()

function(contention_target tgt)
  target_include_directories(${tgt} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_features(${tgt} PUBLIC cxx_std_20)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4)
    if(CONTENTION_STRICT_WARNINGS)
      target_compile_options(${tgt} PRIVATE /WX /permissive-)
    endif()
  endif()
endfunction()

add_library(ContentionGovernor
  src/status.cpp
  src/clock.cpp
  src/conflict.cpp
  src/evidence.cpp
  src/policy.cpp
  src/governor.cpp
  src/governor_engine.cpp
  src/persistence.cpp
  src/digest.cpp
  src/net.cpp
  src/replay.cpp
)
add_library(ContentionGovernor::ContentionGovernor ALIAS ContentionGovernor)
contention_target(ContentionGovernor)
set_target_properties(ContentionGovernor PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

find_package(Threads REQUIRED)
target_link_libraries(ContentionGovernor PUBLIC Threads::Threads)
if(WIN32)
  target_link_libraries(ContentionGovernor PUBLIC ws2_32)
endif()

# Place all executables in a single binary directory so the multiprocess proof
# can locate its helper processes next to the test executable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_BINARY_DIR}/bin)
endforeach()
target_compile_definitions(ContentionGovernor PUBLIC CONTENTION_VERSION_STRING="${PROJECT_VERSION}")

# ---- Installation / package export ----
install(TARGETS ContentionGovernor
  EXPORT ContentionGovernorTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/contention DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ContentionGovernorTargets
  FILE ContentionGovernorTargets.cmake
  NAMESPACE ContentionGovernor::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContentionGovernor)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ContentionGovernorConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ContentionGovernorConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContentionGovernor)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ContentionGovernorConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ContentionGovernorConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ContentionGovernorConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ContentionGovernor)

if(CONTENTION_BUILD_TESTS)
  enable_testing()
  add_subdirectory(dist)
  add_subdirectory(tests)
endif()
if(CONTENTION_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
if(CONTENTION_BUILD_CLI)
  add_subdirectory(cli)
endif()
if(CONTENTION_BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()
if(CONTENTION_BUILD_CUDA_PROOF)
  add_subdirectory(cuda_proof)
endif()
