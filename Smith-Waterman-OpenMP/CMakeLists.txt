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
    set_tests_properties(smith_waterman fasta_reader dna_generator parallel cli parallel_cli openmp PROPERTIES TIMEOUT 60)
endif()
