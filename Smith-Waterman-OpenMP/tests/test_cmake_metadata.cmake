string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(fixture "${WORK_DIR}/unversioned-${suffix}")
file(MAKE_DIRECTORY "${fixture}/source")
file(COPY "${PROJECT_DIR}/CMakeLists.txt" "${PROJECT_DIR}/include" "${PROJECT_DIR}/src"
          "${PROJECT_DIR}/tests" DESTINATION "${fixture}/source")
# Prevent Git from accidentally finding the workspace's parent repository.
set(ENV{GIT_CEILING_DIRECTORIES} "${WORK_DIR}")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${fixture}/source" -B "${fixture}/build"
    -G Ninja "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT "${code}" STREQUAL "0")
    message(FATAL_ERROR "Unversioned source must configure successfully: ${output}\n${error}")
endif()
file(READ "${fixture}/build/compile_commands.json" commands)
if(NOT commands MATCHES "SW_BENCHMARK_REVISION[^\n]*unknown" OR NOT commands MATCHES "SW_BENCHMARK_DIRTY=true")
    message(FATAL_ERROR "Unversioned source must record revision=unknown, dirty=true; not an empty revision or false clean state.")
endif()
if(error MATCHES "fatal: not a git repository")
    message(FATAL_ERROR "Optional Git metadata should not print fatal errors for a source handoff: ${error}")
endif()
message(STATUS "PASS unversioned_source_metadata")
message(STATUS "Passed: 1 CMake metadata case")
