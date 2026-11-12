cmake_minimum_required(VERSION 3.16)
project(SmithWatermanOpenMP VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Let VS Code use the same include paths and per-file flags as the real build.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(sw_core
    src/smith_waterman_serial.cpp
    src/fasta_reader.cpp
    src/dna_generator.cpp
)
target_include_directories(sw_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

function(sw_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
sw_warnings(sw_core)

add_executable(sw_cli src/main.cpp)
sw_warnings(sw_cli)

# Keep the serial library independent of OpenMP; batch execution is a separate
# library that reuses the serial kernel and exposes the OpenMP link dependency.
find_package(OpenMP REQUIRED COMPONENTS CXX)
add_library(sw_parallel src/smith_waterman_parallel.cpp)
target_link_libraries(sw_parallel PUBLIC sw_core OpenMP::OpenMP_CXX)
sw_warnings(sw_parallel)
target_link_libraries(sw_cli PRIVATE sw_parallel)
add_executable(test_openmp tests/test_openmp.cpp)
target_link_libraries(test_openmp PRIVATE OpenMP::OpenMP_CXX)
sw_warnings(test_openmp)

add_library(sw_benchmark_support src/benchmark_support.cpp)
target_link_libraries(sw_benchmark_support PUBLIC sw_parallel)
sw_warnings(sw_benchmark_support)
add_executable(sw_benchmark src/benchmark.cpp)
target_link_libraries(sw_benchmark PRIVATE sw_benchmark_support)
sw_warnings(sw_benchmark)
set(SW_BENCHMARK_REVISION "unknown")
set(SW_BENCHMARK_DIRTY "true")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE SW_GIT_REVISION_STATUS OUTPUT_VARIABLE SW_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # A source ZIP may have Git installed but no repository. Keep the explicit
    # unknown/dirty defaults rather than stamping an empty revision as clean.
    if(SW_GIT_REVISION_STATUS EQUAL 0 AND NOT SW_GIT_REVISION STREQUAL "")
        set(SW_BENCHMARK_REVISION "${SW_GIT_REVISION}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE SW_GIT_STATUS_CODE OUTPUT_VARIABLE SW_GIT_STATUS ERROR_QUIET)
        if(SW_GIT_STATUS_CODE EQUAL 0 AND SW_GIT_STATUS STREQUAL "")
            set(SW_BENCHMARK_DIRTY "false")
        endif()
    endif()
endif()
target_compile_definitions(sw_benchmark PRIVATE
    SW_BENCHMARK_BUILD_TYPE="$<CONFIG>"
    SW_BENCHMARK_REVISION="${SW_BENCHMARK_REVISION}"
    SW_BENCHMARK_DIRTY=${SW_BENCHMARK_DIRTY})

include(CTest)
if(BUILD_TESTING)
    foreach(module smith_waterman fasta_reader dna_generator)
        add_executable(test_${module} tests/test_${module}.cpp)
        target_link_libraries(test_${module} PRIVATE sw_core)
        sw_warnings(test_${module})
        add_test(NAME ${module} COMMAND test_${module})
    endforeach()
    add_executable(test_parallel tests/test_parallel.cpp)
    target_link_libraries(test_parallel PRIVATE sw_parallel)
    sw_warnings(test_parallel)
    add_test(NAME parallel COMMAND test_parallel)
    add_executable(test_parallel_exceptions tests/test_parallel_exceptions.cpp)
    target_link_libraries(test_parallel_exceptions PRIVATE sw_parallel)
    sw_warnings(test_parallel_exceptions)
    add_test(NAME parallel_exceptions COMMAND test_parallel_exceptions)
    add_executable(test_benchmark tests/test_benchmark.cpp)
    target_link_libraries(test_benchmark PRIVATE sw_benchmark_support)
    sw_warnings(test_benchmark)
    add_test(NAME benchmark_math COMMAND test_benchmark)
    add_test(NAME benchmark_cli COMMAND "${CMAKE_COMMAND}"
        "-DBENCH_EXECUTABLE=$<TARGET_FILE:sw_benchmark>"
        "-DSAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/samples"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_benchmark_cli.cmake")
    add_test(NAME cli COMMAND "${CMAKE_COMMAND}"
        "-DSW_EXECUTABLE=$<TARGET_FILE:sw_cli>"
        "-DSAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/samples"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.cmake")
    add_test(NAME parallel_cli COMMAND "${CMAKE_COMMAND}"
        "-DSW_EXECUTABLE=$<TARGET_FILE:sw_cli>"
        "-DSAMPLE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/samples"
        "-DGENERATED_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data/generated"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parallel_cli.cmake")
    add_test(NAME openmp COMMAND test_openmp)
    set_tests_properties(smith_waterman fasta_reader dna_generator parallel parallel_exceptions cli parallel_cli openmp benchmark_math benchmark_cli PROPERTIES TIMEOUT 60)
    if(CMAKE_GENERATOR STREQUAL "Ninja")
        add_test(NAME cmake_metadata COMMAND "${CMAKE_COMMAND}"
            "-DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_metadata.cmake")
        set_tests_properties(cmake_metadata PROPERTIES TIMEOUT 60)
    endif()
    if(WIN32)
        find_program(SW_POWERSHELL_EXECUTABLE powershell)
        if(SW_POWERSHELL_EXECUTABLE)
            add_test(NAME delivery_scripts COMMAND "${SW_POWERSHELL_EXECUTABLE}"
                -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_delivery_scripts.ps1"
                -ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}" -ExecutablePath "$<TARGET_FILE:sw_cli>"
                -BuildDirectory "${CMAKE_CURRENT_BINARY_DIR}" -BuildType "$<CONFIG>")
            set_tests_properties(delivery_scripts PROPERTIES TIMEOUT 120)
        else()
            message(WARNING "Windows PowerShell not found; delivery script tests are unavailable.")
        endif()
    endif()
endif()
