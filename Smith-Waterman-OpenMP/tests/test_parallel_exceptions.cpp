#include "smith_waterman_parallel.h"
#include "test_support.h"

#include <omp.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <new>

// Fault injection is confined to this test executable, never the libraries.
// These sizes select the DP rows for 17/19-base references. Batch output/error
// allocations have different sizes (24 references), so failure occurs in run_one.
namespace {
std::atomic<bool> armed{false};
std::atomic<unsigned> failures{0};
std::atomic<unsigned> worker_failures{0};
struct RowAllocationFailure : std::bad_alloc {
    explicit RowAllocationFailure(std::size_t value) : bytes(value) {}
    std::size_t bytes;
};
struct InjectRows {
    InjectRows() { failures = 0; worker_failures = 0; armed = true; }
    ~InjectRows() { armed = false; }
};
}  // namespace

void* operator new(std::size_t bytes) {
    if (armed.load() && (bytes == 18 * sizeof(int) || bytes == 20 * sizeof(int))) {
        ++failures;
        if (omp_in_parallel()) ++worker_failures;
        throw RowAllocationFailure(bytes);
    }
    if (void* pointer = std::malloc(bytes == 0 ? 1 : bytes)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    omp_set_dynamic(0);
    test::Runner tests;
    const std::string query(64, 'A');
    std::vector<sw::FastaRecord> references;
    for (int i = 0; i < 24; ++i) {
        references.push_back({"ref_" + std::to_string(i), "", std::string(i == 0 ? 19 : 17, 'A')});
    }
    const auto expected = sw::smith_waterman_batch(query, references).scores;
    const int threads = std::min(4, sw::max_batch_threads());
    const auto exercise = [&](const sw::BatchOptions& options) {
        test::require(sw::smith_waterman_batch(query, references, {}, options).scores == expected, "Warm-up scores.");
        bool caught = false;
        std::size_t first_error_bytes = 0;
        {
            InjectRows injection;
            try { sw::smith_waterman_batch(query, references, {}, options); }
            catch (const RowAllocationFailure& error) { caught = true; first_error_bytes = error.bytes; }
        }
        test::require(caught, "Worker allocation exception must reach the caller without terminate/deadlock.");
        test::require(failures == references.size(), "All iterations must finish before errors are rethrown.");
        test::require(first_error_bytes == 20 * sizeof(int), "Rethrow first failing input, not completion order.");
        if (options.mode == sw::ExecutionMode::Parallel && threads > 1) {
            test::require(worker_failures == references.size(), "Fault injection must occur inside the OpenMP team.");
        }
        test::require(sw::smith_waterman_batch(query, references, {}, options).scores == expected, "A later call must recover.");
    };
    tests.run("serial batch propagates allocation failure and recovers", [&] {
        exercise({sw::ExecutionMode::Serial, 1, sw::Schedule::Dynamic});
    });
    tests.run("static workers join and rethrow first input allocation failure", [&] {
        exercise({sw::ExecutionMode::Parallel, threads, sw::Schedule::Static});
    });
    tests.run("dynamic workers join and rethrow first input allocation failure", [&] {
        exercise({sw::ExecutionMode::Parallel, threads, sw::Schedule::Dynamic});
    });
    return tests.finish();
}
