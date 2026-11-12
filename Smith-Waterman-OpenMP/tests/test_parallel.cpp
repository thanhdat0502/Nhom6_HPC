#include "smith_waterman_parallel.h"
#include "dna_generator.h"
#include "test_support.h"

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <limits>
#include <vector>

namespace {

std::vector<int> thread_levels() {
    std::vector<int> levels;
    for (int threads : {1, 2, 4, 8}) {
        if (threads <= sw::max_batch_threads()) levels.push_back(threads);
    }
    if (levels.back() != sw::max_batch_threads()) levels.push_back(sw::max_batch_threads());
    return levels;
}

std::vector<sw::FastaRecord> records(std::initializer_list<std::string> sequences) {
    std::vector<sw::FastaRecord> result;
    for (const auto& dna : sequences) result.push_back({"id_" + std::to_string(result.size()), "", dna});
    return result;
}

void compare_all(const std::string& query, const std::vector<sw::FastaRecord>& references,
                 const sw::ScoringConfig& scoring = {}) {
    std::vector<int> expected;
    for (const auto& reference : references) {
        expected.push_back(sw::smith_waterman_score(query, reference.sequence, scoring));
    }
    const auto serial = sw::smith_waterman_batch(query, references, scoring);
    test::require(serial.scores == expected && serial.mode == sw::ExecutionMode::Serial, "Serial batch oracle.");
    test::require(serial.actual_threads == (references.empty() ? 0 : 1), "Serial team metadata.");
    for (const int threads : thread_levels()) {
        for (const auto schedule : {sw::Schedule::Static, sw::Schedule::Dynamic}) {
            const sw::BatchOptions options{sw::ExecutionMode::Parallel, threads, schedule};
            const auto parallel = sw::smith_waterman_batch(query, references, scoring, options);
            test::require(parallel.scores == expected, "Parallel scores or reference order differ.");
            test::require(parallel.mode == options.mode && parallel.schedule == schedule, "Execution metadata.");
            test::require(parallel.requested_threads == threads, "Requested team metadata.");
            test::require(parallel.actual_threads == (references.empty() ? 0 : threads), "Actual team metadata.");
            test::require(std::isfinite(parallel.compute_seconds) && parallel.compute_seconds >= 0, "Compute timer.");
            if (references.empty()) test::require(parallel.compute_seconds == 0, "Empty batch must not start a region.");
        }
    }
}

}  // namespace

int main() {
    // This affects only this test process. The library must not mutate runtime ICVs.
    omp_set_dynamic(0);
    test::Runner tests;
    std::cout << "Available batch threads: " << sw::max_batch_threads() << "\nTested thread levels:";
    for (const int n : thread_levels()) std::cout << ' ' << n;
    std::cout << '\n';

    tests.run("one query and one reference, known score", [] {
        const auto refs = records({"ACGT"});
        test::require(sw::smith_waterman_batch("ACGT", refs).scores == std::vector<int>{8}, "Known score 8.");
        compare_all("ACGT", refs);
    });
    tests.run("multiple references preserve distinct score order", [] {
        const auto refs = records({"TTTT", "ACG", "ACGT", "A", "AC"});
        test::require(sw::smith_waterman_batch("ACGT", refs).scores == std::vector<int>({2, 6, 8, 2, 4}), "Known order.");
        compare_all("ACGT", refs);
    });
    tests.run("identical references", [] { compare_all("ACGTACGT", records({"ACGTACGT", "ACGTACGT", "ACGTACGT"})); });
    tests.run("no positive alignment", [] {
        const auto refs = records({"TTTT", "CCCC", "GGGG"});
        test::require(sw::smith_waterman_batch("AAAA", refs).scores == std::vector<int>({0, 0, 0}), "Zero scores.");
        compare_all("AAAA", refs);
    });
    tests.run("different lengths, mismatch and gaps", [] {
        compare_all("AAAACCCC", records({"A", "AAAAGCCCC", "AAAATCCC", "CCCC", "AAAACCCCTTTT", ""}));
    });
    tests.run("ties and duplicate sequences remain separate entries", [] { compare_all("AC", records({"CA", "CA", "A", "C"})); });
    tests.run("empty query and empty references at API level", [] { compare_all("", records({"ACGT", "", "A"})); });
    tests.run("empty batch does not create a team", [] { compare_all("ACGT", {}); compare_all("", {}); });
    tests.run("case-insensitive DNA", [] { compare_all("acGT", records({"ACgt", "acg", "ttTT"})); });
    tests.run("small seeded dataset", [] { compare_all("ACGTACGT", sw::generate_dna(23, 32, 42)); });
    tests.run("medium seeded dataset", [] {
        const auto refs = sw::generate_dna(17, 512, 2026);
        compare_all(refs[0].sequence, refs);
    });
    tests.run("heterogeneous lengths and more references than threads", [] {
        auto refs = sw::generate_dna(73, 192, 9123);
        for (std::size_t i = 0; i < refs.size(); ++i) refs[i].sequence.resize((i * 31) % 193);
        compare_all("ACGTAAAACCCCGGGGTTTTACGT", refs);
    });
    tests.run("fewer references than requested threads", [] { compare_all("ACGT", records({"ACG", "ACGT"})); });
    tests.run("parallel with one worker is still recorded as parallel", [] {
        const auto result = sw::smith_waterman_batch("A", records({"A"}), {},
            {sw::ExecutionMode::Parallel, 1, sw::Schedule::Dynamic});
        test::require(result.mode == sw::ExecutionMode::Parallel && result.actual_threads == 1 && result.scores[0] == 2, "One worker metadata.");
    });
    tests.run("twenty repeated runs per schedule", [] {
        const auto refs = sw::generate_dna(37, 80, 20261112);
        const auto expected = sw::smith_waterman_batch("AAAACCCCGGGGTTTT", refs).scores;
        for (const auto schedule : {sw::Schedule::Static, sw::Schedule::Dynamic}) {
            for (int repetition = 0; repetition < 20; ++repetition) {
                const auto result = sw::smith_waterman_batch("AAAACCCCGGGGTTTT", refs, {},
                    {sw::ExecutionMode::Parallel, std::min(4, sw::max_batch_threads()), schedule});
                test::require(result.scores == expected, "Repeated run changed values or order.");
            }
        }
    });
    tests.run("configurable linear scoring with known expected results", [] {
        const sw::ScoringConfig scoring{3, -2, -1};
        const auto refs = records({"AAAACCCC", "AAAAGCCCC", "TTTT"});
        test::require(sw::smith_waterman_batch("AAAACCCC", refs, scoring).scores == std::vector<int>({24, 23, 0}), "Custom scoring.");
        compare_all("AAAACCCC", refs, scoring);
        compare_all("ACGT", records({"ACGT", "TGCA"}), {1, 0, -1});
    });
    tests.run("integer score boundary and overflow rejected before dispatch", [] {
        const sw::ScoringConfig scoring{std::numeric_limits<int>::max(), -1, -2};
        compare_all("A", records({"A", "AA", "T"}), scoring);
        for (const auto mode : {sw::ExecutionMode::Serial, sw::ExecutionMode::Parallel}) {
            test::throws<std::length_error>([&] {
                sw::smith_waterman_batch("AA", records({"AA"}), scoring, {mode, 1, sw::Schedule::Static});
            });
        }
    });
    tests.run("invalid DNA validated even for empty query or batch", [] {
        const sw::BatchOptions options{sw::ExecutionMode::Parallel, std::min(4, sw::max_batch_threads()), sw::Schedule::Dynamic};
        for (const auto& invalid : {"AN", "AC!T", "A C", "U"}) {
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_batch(invalid, {}, {}, options); });
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_batch("", records({"ACGT", invalid}), {}, options); });
        }
    });
    tests.run("invalid scoring rejected", [] {
        for (const sw::ScoringConfig scoring : {sw::ScoringConfig{0, -1, -2}, {-1, -1, -2}, {2, 1, -2}, {2, -1, 0}, {2, -1, 1}}) {
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_batch("", {}, scoring); });
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_score("A", "A", scoring); });
        }
    });
    tests.run("invalid thread counts and serial multithreading rejected", [] {
        for (const int threads : {-1, 0, sw::max_batch_threads() + 1}) {
            test::throws<std::invalid_argument>([&] {
                sw::smith_waterman_batch("A", records({"A"}), {}, {sw::ExecutionMode::Parallel, threads, sw::Schedule::Static});
            });
        }
        test::throws<std::invalid_argument>([] {
            sw::smith_waterman_batch("A", records({"A"}), {}, {sw::ExecutionMode::Serial, 2, sw::Schedule::Static});
        });
    });
    tests.run("invalid mode and schedule rejected", [] {
        test::throws<std::invalid_argument>([] {
            sw::smith_waterman_batch("A", {}, {}, {static_cast<sw::ExecutionMode>(42), 1, sw::Schedule::Static});
        });
        test::throws<std::invalid_argument>([] {
            sw::smith_waterman_batch("A", {}, {}, {sw::ExecutionMode::Parallel, 1, static_cast<sw::Schedule>(42)});
        });
    });
    tests.run("batch leaves OpenMP runtime settings unchanged", [] {
        const int max_threads = omp_get_max_threads();
        const int dynamic = omp_get_dynamic();
        omp_sched_t schedule_before;
        int chunk_before;
        omp_get_schedule(&schedule_before, &chunk_before);
        compare_all("ACGT", records({"ACGT", "ACG"}));
        omp_sched_t schedule_after;
        int chunk_after;
        omp_get_schedule(&schedule_after, &chunk_after);
        test::require(max_threads == omp_get_max_threads() && dynamic == omp_get_dynamic() &&
                      schedule_before == schedule_after && chunk_before == chunk_after, "Runtime ICVs mutated.");
    });
    tests.run("independent concurrent callers", [] {
        const auto refs = sw::generate_dna(19, 96, 31415);
        const auto run = [&](const std::string& query) {
            return sw::smith_waterman_batch(query, refs, {},
                {sw::ExecutionMode::Parallel, std::min(2, sw::max_batch_threads()), sw::Schedule::Dynamic}).scores;
        };
        auto first = std::async(std::launch::async, run, "ACGTACGT");
        auto second = std::async(std::launch::async, run, "AAAACCCC");
        test::require(first.get() == sw::smith_waterman_batch("ACGTACGT", refs).scores, "First concurrent call.");
        test::require(second.get() == sw::smith_waterman_batch("AAAACCCC", refs).scores, "Second concurrent call.");
    });
    return tests.finish();
}
