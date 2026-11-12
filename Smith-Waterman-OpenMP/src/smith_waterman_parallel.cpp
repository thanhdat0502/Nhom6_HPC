#include "smith_waterman_parallel.h"

#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>

namespace sw {

int max_batch_threads() {
    return std::max(1, std::min(omp_get_num_procs(), omp_get_thread_limit()));
}

BatchResult smith_waterman_batch(std::string_view query,
                                const std::vector<FastaRecord>& references,
                                const ScoringConfig& scoring,
                                const BatchOptions& options) {
    if (options.mode != ExecutionMode::Serial && options.mode != ExecutionMode::Parallel) {
        throw std::invalid_argument("Unknown execution mode.");
    }
    if (options.schedule != Schedule::Static && options.schedule != Schedule::Dynamic) {
        throw std::invalid_argument("Unknown OpenMP schedule.");
    }
    if (options.threads < 1 || options.threads > max_batch_threads()) {
        throw std::invalid_argument("Thread count must be between 1 and " + std::to_string(max_batch_threads()) + '.');
    }
    if (options.mode == ExecutionMode::Serial && options.threads != 1) {
        throw std::invalid_argument("Serial mode requires --threads 1.");
    }
    validate_scoring(scoring);
    validate_dna(query);
    for (const auto& reference : references) {
        validate_dna(reference.sequence);
        if (std::min(query.size(), reference.sequence.size()) >
            static_cast<std::size_t>(std::numeric_limits<int>::max() / scoring.match)) {
            throw std::length_error("DNA lengths exceed the supported integer score range.");
        }
    }
    if (references.size() > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw std::length_error("Too many references for the OpenMP loop.");
    }

    BatchResult result;
    result.mode = options.mode;
    result.schedule = options.schedule;
    result.requested_threads = options.threads;
    result.scores.resize(references.size());
    if (references.empty()) return result;

    const auto reference_count = static_cast<std::ptrdiff_t>(references.size());
    // Preallocate error slots as well: no resizing or shared append in workers.
    std::vector<std::exception_ptr> errors(references.size());
    const auto run_one = [&](std::ptrdiff_t index) noexcept {
        const auto i = static_cast<std::size_t>(index);
        try {
            result.scores[i] = smith_waterman_score(query, references[i].sequence, scoring);
        } catch (...) {
            errors[i] = std::current_exception();
        }
    };

    const auto start = std::chrono::steady_clock::now();
    if (options.mode == ExecutionMode::Serial) {
        result.actual_threads = 1;
        for (std::ptrdiff_t i = 0; i < reference_count; ++i) run_one(i);
    } else {
        const int requested = options.threads;
        const Schedule scheduling = options.schedule;
        int actual = 0;
        // Only independent pairs are distributed. Each score kernel owns its
        // two DP rows; inputs are shared read-only, output/error indices disjoint.
#pragma omp parallel num_threads(requested) default(none) shared(actual, run_one, reference_count, scheduling)
        {
#pragma omp single
            actual = omp_get_num_threads();
            if (scheduling == Schedule::Static) {
#pragma omp for schedule(static)
                for (std::ptrdiff_t i = 0; i < reference_count; ++i) run_one(i);
            } else {
#pragma omp for schedule(dynamic, 1)
                for (std::ptrdiff_t i = 0; i < reference_count; ++i) run_one(i);
            }
        }
        result.actual_threads = actual;
    }
    result.compute_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    // The parallel join has completed: preserve deterministic error ordering.
    for (const auto& error : errors) {
        if (error) std::rethrow_exception(error);
    }
    return result;
}

}  // namespace sw
