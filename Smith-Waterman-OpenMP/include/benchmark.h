#pragma once

#include "smith_waterman_parallel.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace sw::bench {

struct Dataset {
    std::string id;
    std::string distribution;
    std::string seed;  // Empty for external FASTA with unknown provenance.
    FastaRecord query;
    std::vector<FastaRecord> references;
};
struct TimingStats {
    double median, mean, minimum, maximum, population_stddev;
};
struct Sample {
    BatchOptions options;
    int actual_threads;
    bool warmup;
    int run_index;
    int order_index;
    double seconds;
    std::uint64_t score_checksum;
};
struct Summary {
    BatchOptions options;
    int actual_threads;
    std::size_t samples;
    TimingStats timing;
    double speedup, efficiency, gcups;
};

TimingStats timing_stats(std::vector<double> values);
std::uint64_t dp_cells(const Dataset& dataset);
std::vector<Dataset> make_suite(const std::string& scale);
std::vector<BatchOptions> configurations(const std::vector<int>& threads,
                                       const std::vector<Schedule>& schedules);
std::vector<Sample> measure(const Dataset& dataset, const std::vector<BatchOptions>& configs,
                            int warmups, int runs);
std::vector<Summary> summarize(const std::vector<Sample>& samples, std::uint64_t cells);
std::string csv_field(const std::string& value);
std::string mode_name(ExecutionMode mode);
std::string schedule_name(const BatchOptions& options);
void raw_header(std::ostream& out);
void summary_header(std::ostream& out);
void write_raw(std::ostream& out, const Dataset& dataset, const std::vector<Sample>& samples,
               const std::string& build_type);
void write_summary(std::ostream& out, const Dataset& dataset, const std::vector<Summary>& summaries,
                   int expected_runs, const std::string& build_type);

}  // namespace sw::bench
