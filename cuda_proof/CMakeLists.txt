# Optional isolated CUDA contention proof. Requires CUDA toolkit + hardware.
if(NOT CONTENTION_BUILD_CUDA_PROOF)
  return()
endif()

find_package(CUDAToolkit QUIET)
if(CUDAToolkit_FOUND)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  add_executable(cuda_proof cuda_proof.cu)
  target_link_libraries(cuda_proof PRIVATE ContentionGovernor::ContentionGovernor CUDA::cudart)
  set_target_properties(cuda_proof PROPERTIES CUDA_ARCHITECTURES 120)
  if(MSVC)
    # nvcc host compile emits its own diagnostics; keep the global /WX from
    # tripping on CUDA-generated host code while retaining strictness elsewhere.
    target_compile_options(cuda_proof PRIVATE /WX-)
  endif()
  add_test(NAME cuda_proof COMMAND cuda_proof)
else()
  message(STATUS "CUDA toolkit not found; skipping CUDA proof.")
endif()
