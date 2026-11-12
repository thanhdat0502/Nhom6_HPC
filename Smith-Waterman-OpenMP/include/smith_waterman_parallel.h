#pragma once

#include "fasta_reader.h"
#include "smith_waterman.h"

#include <string_view>
#include <vector>

namespace sw {

enum class ExecutionMode { Serial, Parallel };
enum class Schedule { Static, Dynamic };

struct BatchOptions {
    ExecutionMode mode = ExecutionMode::Serial;
    int threads = 1;
    Schedule schedule = Schedule::Dynamic;
};

struct BatchResult {
    std::vector<int> scores;
    ExecutionMode mode = ExecutionMode::Serial;
    Schedule schedule = Schedule::Dynamic;
    int requested_threads = 1;
    // Zero for an empty batch; otherwise the actual team size (serial = 1).
    int actual_threads = 0;
    // Compute region only, including fork/join; excludes validation, result
    // allocation, file I/O, output formatting and optional traceback.
    double compute_seconds = 0.0;
};

// Bound requests by the processors and OpenMP thread limit in this environment.
int max_batch_threads();

// References are read-only and remain in input order. Empty DNA is valid at the
// C++ API level. All input/config validation happens before the parallel region.
// Worker exceptions are captured per index, then rethrown on the calling thread.
BatchResult smith_waterman_batch(std::string_view query,
                                const std::vector<FastaRecord>& references,
                                const ScoringConfig& scoring = {},
                                const BatchOptions& options = {});

}  // namespace sw
