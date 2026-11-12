# Exercise the real CLI and check stdout/stderr independently.
set_property(GLOBAL PROPERTY SW_CLI_TEST_COUNT 0)

function(check_result label expected_code stdout_pattern stderr_pattern)
    if(NOT "${code}" STREQUAL "${expected_code}")
        message(FATAL_ERROR "${label}: expected exit ${expected_code}, got ${code}\n${output}\n${error}")
    endif()
    if(NOT "${output}" MATCHES "${stdout_pattern}")
        message(FATAL_ERROR "${label}: unexpected stdout [${output}]; expected pattern [${stdout_pattern}]")
    endif()
    if(NOT "${error}" MATCHES "${stderr_pattern}")
        message(FATAL_ERROR "${label}: unexpected stderr [${error}]; expected pattern [${stderr_pattern}]")
    endif()
    get_property(count GLOBAL PROPERTY SW_CLI_TEST_COUNT)
    math(EXPR count "${count} + 1")
    set_property(GLOBAL PROPERTY SW_CLI_TEST_COUNT "${count}")
    message(STATUS "PASS ${label}")
endfunction()

function(check_cli label expected_code stdout_pattern stderr_pattern)
    execute_process(COMMAND "${SW_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
    check_result("${label}" "${expected_code}" "${stdout_pattern}" "${stderr_pattern}")
endfunction()

check_cli(help 0 "Usage:" "^$" --help)
check_cli(score 0 "^Score: 8\n$" "^$" align --seq1 ACGT --seq2 ACGT)
check_cli(traceback 0 "AAAA-CCCC" "^$" align --seq1 AAAACCCC --seq2 AAAAGCCCC --alignment)
check_cli(search 0 "ref_exact[\t]8" "^$" search --query "${SAMPLE_DIR}/queries.fasta"
    --query-id query_1 --references "${SAMPLE_DIR}/references.fasta")
check_cli(search_traceback 0 "Score: 14" "^$" search --query "${SAMPLE_DIR}/queries.fasta"
    --query-id query_2 --references "${SAMPLE_DIR}/references.fasta" --alignment-id ref_gap)
check_cli(valid_mismatch 0 "^Score: 13\n$" "^$" align --seq1 AAAACCCC --seq2 AAAATCCC)
check_cli(no_positive_match 0 "^Score: 0\n$" "^$" align --seq1 AAAA --seq2 TTTT)
check_cli(lowercase_dna 0 "^Score: 8\n$" "^$" align --seq1 acgt --seq2 ACGT)
check_cli(no_command 1 "^$" "^Error: Missing command")
check_cli(unknown_command 1 "^$" "^Error: Unknown command" unknown)
check_cli(missing_option 1 "^$" "^Error: Missing required option" align --seq1 ACGT)
check_cli(missing_value 1 "^$" "^Error: Missing value" align --seq1 ACGT --seq2)
check_cli(duplicate_option 1 "^$" "^Error: Repeated option" align --seq1 A --seq1 C --seq2 G)
check_cli(unknown_option 1 "^$" "^Error: Unknown option" align --seq1 A --seq2 C --bogus)
check_cli(invalid_dna 1 "^$" "^Error: Invalid DNA" align --seq1 ACNT --seq2 ACGT)
check_cli(invalid_second_dna 1 "^$" "^Error: Invalid DNA" align --seq1 ACGT --seq2 ACNT)
check_cli(special_character 1 "^$" "^Error: Invalid DNA" align --seq1 "AC!T" --seq2 ACGT)
check_cli(invalid_alignment_dna 1 "^$" "^Error: Invalid DNA" align --seq1 ACNT --seq2 ACGT --alignment)
check_cli(special_alignment_character 1 "^$" "^Error: Invalid DNA" align --seq1 ACGT --seq2 "AC-T" --alignment)
check_cli(missing_file 1 "^$" "^Error: Cannot open FASTA" search --query "${SAMPLE_DIR}/missing.fasta"
    --query-id query_1 --references "${SAMPLE_DIR}/references.fasta")
check_cli(missing_query_id 1 "^$" "^Error: FASTA ID not found" search --query "${SAMPLE_DIR}/queries.fasta"
    --query-id unknown --references "${SAMPLE_DIR}/references.fasta")
check_cli(missing_reference_id 1 "^$" "^Error: FASTA ID not found" search --query "${SAMPLE_DIR}/queries.fasta"
    --query-id query_1 --references "${SAMPLE_DIR}/references.fasta" --alignment-id unknown)
check_cli(invalid_count 1 "^$" "^Error: Invalid integer" generate --count -1 --length 8 --seed 42 --output ignored.fasta)
check_cli(zero_length 1 "^$" "^Error: Invalid integer" generate --count 1 --length 0 --seed 42 --output ignored.fasta)
check_cli(seed_overflow 1 "^$" "^Error: Invalid integer" generate --count 1 --length 8 --seed 4294967296 --output ignored.fasta)
check_cli(trailing_numeric_text 1 "^$" "^Error: Invalid integer" generate --count 1x --length 8 --seed 42 --output ignored.fasta)

# Keep empty argv values quoted at execute_process itself: expanding ARGN as a
# CMake list would discard empty elements, turning these into missing-value tests.
execute_process(COMMAND "${SW_EXECUTABLE}" align --seq1 "" --seq2 ACGT
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
check_result(empty_first_sequence 0 "^Score: 0\n$" "^$")
execute_process(COMMAND "${SW_EXECUTABLE}" align --seq1 ACGT --seq2 ""
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
check_result(empty_second_sequence 0 "^Score: 0\n$" "^$")
execute_process(COMMAND "${SW_EXECUTABLE}" align --seq1 "" --seq2 ""
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
check_result(both_empty_sequences 0 "^Score: 0\n$" "^$")
execute_process(COMMAND "${SW_EXECUTABLE}" align --seq1 "" --seq2 "" --alignment
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
check_result(empty_alignment 0 "^Score: 0\n.*No positive local alignment" "^$")

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(generated "${WORK_DIR}/cli-generated-${suffix}.fasta")
if(EXISTS "${generated}")
    message(FATAL_ERROR "Test output path already exists: ${generated}")
endif()
check_cli(generate 0 "Generated 2 sequences" "^$" generate --count 2 --length 16 --seed 42 --output "${generated}")
file(READ "${generated}" before)
check_cli(no_overwrite 1 "^$" "^Error: Refusing to overwrite" generate --count 2 --length 16 --seed 99 --output "${generated}")
file(READ "${generated}" after)
if(NOT "${before}" STREQUAL "${after}")
    message(FATAL_ERROR "CLI overwrote existing data.")
endif()
check_cli(generated_search 0 "seq_1[\t]32" "^$" search --query "${generated}" --query-id seq_1 --references "${generated}")
file(REMOVE "${generated}")

# Create invalid FASTA fixtures only at fresh paths inside the build directory.
foreach(kind dna header empty)
    set(invalid_${kind} "${WORK_DIR}/cli-invalid-${kind}-${suffix}.fasta")
    if(EXISTS "${invalid_${kind}}")
        message(FATAL_ERROR "Test fixture already exists: ${invalid_${kind}}")
    endif()
endforeach()
file(WRITE "${invalid_dna}" ">bad\nACNT\n")
file(WRITE "${invalid_header}" "ACGT\n")
file(WRITE "${invalid_empty}" ">bad\n")
check_cli(invalid_fasta_dna 1 "^$" "^Error: .*only A, C, G, T" search
    --query "${invalid_dna}" --query-id bad --references "${SAMPLE_DIR}/references.fasta")
check_cli(invalid_fasta_header 1 "^$" "^Error: .*before the first >header" search
    --query "${SAMPLE_DIR}/queries.fasta" --query-id query_1 --references "${invalid_header}")
check_cli(invalid_fasta_empty_record 1 "^$" "^Error: .*Empty sequence" search
    --query "${invalid_empty}" --query-id bad --references "${SAMPLE_DIR}/references.fasta")
file(REMOVE "${invalid_dna}" "${invalid_header}" "${invalid_empty}")

get_property(count GLOBAL PROPERTY SW_CLI_TEST_COUNT)
message(STATUS "Passed: ${count} CLI cases")
