#include "benchmark.h"
#include "dna_generator.h"
#include "test_support.h"

#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace {
bool close(double a, double b) { return std::abs(a - b) < 1e-10; }
sw::bench::Sample sample(sw::ExecutionMode mode, int requested, int actual, double seconds, bool warmup = false) {
    return {{mode, requested, sw::Schedule::Static}, actual, warmup, 1, 1, seconds, 123};
}
}

int main() {
    test::Runner tests;
    using namespace sw::bench;
    tests.run("median odd and even", [] {
        test::require(close(timing_stats({7, 1, 3}).median, 3), "Odd median.");
        test::require(close(timing_stats({4, 1, 3, 2}).median, 2.5), "Even median.");
    });
    tests.run("mean min max population standard deviation", [] {
        const auto s = timing_stats({1, 2, 3});
        test::require(close(s.mean, 2) && close(s.minimum, 1) && close(s.maximum, 3), "Timing aggregates.");
        test::require(close(s.population_stddev, std::sqrt(2.0 / 3)), "Population standard deviation.");
    });
    tests.run("invalid timing samples rejected", [] {
        test::throws<std::invalid_argument>([] { timing_stats({}); });
        for (const double value : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            test::throws<std::invalid_argument>([&] { timing_stats({1, value}); });
        }
    });
    tests.run("speedup efficiency GCUPS and warmup exclusion", [] {
        // Synthetic arithmetic fixtures only; never exported as measured results.
        const auto serial = sw::ExecutionMode::Serial, parallel = sw::ExecutionMode::Parallel;
        const std::vector<Sample> samples{sample(serial, 1, 1, 100, true), sample(serial, 1, 1, 4),
            sample(serial, 1, 1, 2), sample(serial, 1, 1, 3), sample(parallel, 2, 2, 1)};
        const auto rows = summarize(samples, 1000000000ULL);
        test::require(rows.size() == 2, "Summary group count.");
        test::require(rows[0].samples == 3 && close(rows[0].timing.median, 3), "Warmups excluded.");
        test::require(close(rows[1].speedup, 3) && close(rows[1].efficiency, 1.5) && close(rows[1].gcups, 1), "HPC formulas.");
    });
    tests.run("actual teams grouped separately and used in efficiency", [] {
        const auto rows = summarize({sample(sw::ExecutionMode::Serial, 1, 1, 8),
            sample(sw::ExecutionMode::Parallel, 4, 2, 2), sample(sw::ExecutionMode::Parallel, 4, 4, 1)}, 100);
        test::require(rows.size() == 3, "Actual team groups merged.");
        test::require(rows[1].actual_threads == 2 && close(rows[1].efficiency, 2), "Efficiency must use actual=2.");
        test::require(rows[2].actual_threads == 4 && close(rows[2].efficiency, 2), "Efficiency actual=4.");
        test::throws<std::invalid_argument>([] { summarize({}, 100); });
    });
    tests.run("DP cell count uses full lengths without boundary cells", [] {
        Dataset d{"id", "external", "", {"q", "", "AC"}, {{"a", "", "AAA"}, {"b", "", "TTTT"}}};
        test::require(dp_cells(d) == 14, "DP cells must be m * sum(n).");
    });
    tests.run("six reproducible datasets with equal work per distribution pair", [] {
        const auto first = make_suite("all"), second = make_suite("all");
        test::require(first.size() == 6 && second.size() == 6, "Suite size.");
        for (std::size_t i = 0; i < first.size(); ++i) {
            const auto& a = first[i]; const auto& b = second[i];
            test::require(a.id == b.id && a.seed == b.seed && a.query.sequence == b.query.sequence, "Dataset query replay.");
            for (std::size_t j = 0; j < a.references.size(); ++j) {
                test::require(a.references[j].sequence == b.references[j].sequence, "Reference seed replay.");
                if (j) test::require(a.references[j - 1].sequence.size() <= a.references[j].sequence.size(), "Lengths must be clustered by index.");
            }
            if (i % 2) {
                test::require(dp_cells(a) == dp_cells(first[i - 1]), "Unequal work across distribution pair.");
                test::require(a.query.sequence == first[i - 1].query.sequence, "Query must match distribution pair.");
            }
        }
        test::throws<std::invalid_argument>([] { make_suite("unknown"); });
    });
    tests.run("generated FASTA replay roundtrip", [] {
        const auto a = make_suite("smoke"), b = make_suite("smoke");
        test::TempFile first, second;
        sw::write_fasta(first.path(), a[1].references);
        sw::write_fasta(second.path(), b[1].references);
        const auto loaded = sw::read_fasta(first.path());
        test::require(loaded.size() == a[1].references.size(), "Export count.");
        for (std::size_t i = 0; i < loaded.size(); ++i) test::require(loaded[i].sequence == b[1].references[i].sequence, "Export replay content.");
    });
    tests.run("configuration validation and duplicates", [] {
        test::require(configurations({1}, {sw::Schedule::Static, sw::Schedule::Dynamic}).size() == 3, "Serial plus two parallel configs.");
        test::throws<std::invalid_argument>([] { configurations({}, {sw::Schedule::Static}); });
        test::throws<std::invalid_argument>([] { configurations({1, 1}, {sw::Schedule::Static}); });
        test::throws<std::invalid_argument>([] { configurations({0}, {sw::Schedule::Static}); });
    });
    tests.run("real smoke measurements verified and all rounds recorded", [] {
        const auto d = make_suite("smoke").front();
        const auto samples = measure(d, configurations({1}, {sw::Schedule::Static, sw::Schedule::Dynamic}), 2, 7);
        test::require(samples.size() == 27, "Expected 3 configs * 9 samples.");
        std::set<std::uint64_t> checksums;
        int measured = 0;
        for (const auto& s : samples) { checksums.insert(s.score_checksum); if (!s.warmup) ++measured; }
        test::require(checksums.size() == 1 && measured == 21, "Scores differ or phase count wrong.");
        for (std::size_t start = 0; start < samples.size(); start += 9) {
            const auto& config = samples[start].options;
            for (std::size_t k = 0; k < 9; ++k) {
                const auto& s = samples[start + k];
                test::require(s.options.mode == config.mode && s.options.schedule == config.schedule &&
                              s.options.threads == config.threads, "Configuration block was interrupted.");
                test::require(s.warmup == (k < 2), "Warmups must immediately precede measured runs.");
                test::require(s.run_index == static_cast<int>(k < 2 ? k + 1 : k - 1) &&
                              s.order_index == static_cast<int>(start + k + 1), "Recorded run/order index.");
            }
        }
        const auto rows = summarize(samples, dp_cells(d));
        for (const auto& row : rows) test::require(row.samples == 7, "Wrong measured sample count.");
    });
    tests.run("measurement rejects invalid repetition counts and empty work", [] {
        const auto d = make_suite("smoke").front(); const auto c = configurations({1}, {sw::Schedule::Static});
        test::throws<std::invalid_argument>([&] { measure(d, c, 1, 7); });
        test::throws<std::invalid_argument>([&] { measure(d, c, 2, 6); });
        test::throws<std::invalid_argument>([&] { measure({}, c, 2, 7); });
    });
    tests.run("CSV quoting precision and thread mismatch flags", [] {
        test::require(csv_field("a,b\"c") == "\"a,b\"\"c\"", "CSV escaping.");
        const auto d = make_suite("smoke").front();
        const auto s = sample(sw::ExecutionMode::Parallel, 4, 2, 0.12345678901234567);
        std::ostringstream raw, summary;
        raw_header(raw); write_raw(raw, d, {s}, "Release");
        test::require(raw.str().find("0.123456789012345") != std::string::npos, "Insufficient numeric precision.");
        const auto rows = summarize({sample(sw::ExecutionMode::Serial, 1, 1, 1), s}, dp_cells(d));
        summary_header(summary); write_summary(summary, d, rows, 7, "Release");
        test::require(summary.str().find("parallel,static,4,2,1,0,1,") != std::string::npos, "Incomplete/mismatched group flags.");
    });
    return tests.finish();
}
