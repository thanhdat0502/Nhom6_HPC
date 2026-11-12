#include "benchmark.h"
#include "dna_generator.h"

#include <omp.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using Options = std::map<std::string, std::string>;

std::string json_string(const std::string& text) {
    std::ostringstream out;
    out << '"';
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u00" << hex[c / 16] << hex[c % 16];
        else out << static_cast<char>(c);
    }
    out << '"';
    return out.str();
}

int number(const std::string& text, int minimum, int maximum) {
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < minimum || value > maximum) {
        throw std::invalid_argument("Invalid integer: " + text);
    }
    return value;
}

std::vector<std::string> split(const std::string& text) {
    std::vector<std::string> values;
    std::size_t start = 0;
    for (;;) {
        const auto end = text.find(',', start);
        const auto part = text.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty()) throw std::invalid_argument("Empty list item.");
        values.push_back(part);
        if (end == std::string::npos) return values;
        start = end + 1;
    }
}

Options parse(int argc, char** argv) {
    const std::set<std::string> allowed{"--output", "--suite", "--threads", "--schedules", "--warmup", "--runs",
                                        "--query", "--query-id", "--references", "--dataset-id", "--allow-debug"};
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (!allowed.count(key)) throw std::invalid_argument("Unknown option: " + key);
        if (options.count(key)) throw std::invalid_argument("Repeated option: " + key);
        if (key == "--allow-debug") options[key] = "";
        else {
            if (i + 1 == argc || std::string(argv[i + 1]).rfind("--", 0) == 0) throw std::invalid_argument("Missing value: " + key);
            options[key] = argv[++i];
        }
    }
    return options;
}

const std::string& required(const Options& o, const char* key) {
    const auto it = o.find(key);
    if (it == o.end()) throw std::invalid_argument("Missing required option: " + std::string(key));
    return it->second;
}

void metadata(const std::filesystem::path& path, const std::vector<sw::bench::Dataset>& datasets,
              const std::vector<sw::BatchOptions>& configs, int warmups, int runs, bool complete) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write run metadata.");
    out << "{\n  \"status\": " << json_string(complete ? "complete" : "running")
        << ",\n  \"build_type\": " << json_string(SW_BENCHMARK_BUILD_TYPE)
        << ",\n  \"source_revision\": " << json_string(SW_BENCHMARK_REVISION)
        << ",\n  \"source_dirty_at_configure\": " << (SW_BENCHMARK_DIRTY ? "true" : "false")
        << ",\n  \"compiler\": " << json_string(__VERSION__)
        << ",\n  \"openmp_macro\": " << _OPENMP
        << ",\n  \"omp_processors\": " << omp_get_num_procs()
        << ",\n  \"thread_limit\": " << omp_get_thread_limit()
        << ",\n  \"warmups\": " << warmups << ",\n  \"runs\": " << runs
        << ",\n  \"timer\": \"BatchResult.compute_seconds; same compute region for serial and parallel\""
        << ",\n  \"order\": \"shuffled configuration blocks; immediate warmups; warmups excluded from summary\""
        << ",\n  \"order_seed_recipe\": \"uint32 seed=7301; for each dataset-id byte: seed=seed*33+byte; mt19937/std::shuffle\""
        << ",\n  \"environment\": {";
    bool first = true;
    for (const char* name : {"OMP_DYNAMIC", "OMP_NUM_THREADS", "OMP_THREAD_LIMIT", "OMP_PROC_BIND", "OMP_PLACES", "OMP_WAIT_POLICY"}) {
        if (!first) out << ',';
        first = false;
        const char* value = std::getenv(name);
        out << '\n' << "    " << json_string(name) << ": " << (value ? json_string(value) : "null");
    }
    out << "\n  },\n  \"configurations\": [";
    first = true;
    for (const auto& c : configs) {
        if (!first) out << ',';
        first = false;
        out << "\n    {\"mode\": " << json_string(sw::bench::mode_name(c.mode))
            << ", \"schedule\": " << json_string(sw::bench::schedule_name(c)) << ", \"requested_threads\": " << c.threads << '}';
    }
    out << "\n  ],\n  \"datasets\": [";
    first = true;
    for (const auto& d : datasets) {
        if (!first) out << ',';
        first = false;
        out << "\n    {\"id\": " << json_string(d.id) << ", \"distribution\": " << json_string(d.distribution)
            << ", \"seed\": " << json_string(d.seed) << ", \"query_length\": " << d.query.sequence.size()
            << ", \"reference_count\": " << d.references.size() << ", \"dp_cells\": " << sw::bench::dp_cells(d)
            << ", \"query_fasta\": " << json_string("data/" + d.id + "_query.fasta")
            << ", \"references_fasta\": " << json_string("data/" + d.id + "_references.fasta")
            << ", \"reference_lengths\": [";
        for (std::size_t i = 0; i < d.references.size(); ++i) {
            if (i) out << ',';
            out << d.references[i].sequence.size();
        }
        out << "]}";
    }
    out << "\n  ]\n}\n";
    out.close();
    if (!out) throw std::runtime_error("Failed to flush run metadata.");
}

void run(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: sw_benchmark --output NEW_DIRECTORY [--suite all|small|medium|large|smoke]\n"
                     "  [--threads 1,2,4,8,12] [--schedules static,dynamic] [--warmup 2] [--runs 7]\n"
                     "External FASTA: --query FILE --query-id ID --references FILE [--dataset-id ID]\n"
                     "External FASTA cannot be combined with --suite. Files are loaded before timing.\n"
                     "Warmups: 2..1000; runs: 7..10000. Existing output directories are refused.\n"
                     "Only Release builds are accepted unless --allow-debug is used for smoke tests.\n"
                     "All samples are verified against serial; metadata records actual thread counts.\n";
        return;
    }
    const auto o = parse(argc, argv);
    const auto output = std::filesystem::path(required(o, "--output"));
    if (output.empty() || std::filesystem::exists(output)) throw std::invalid_argument("Output directory must be new and nonempty.");
    if (std::string(SW_BENCHMARK_BUILD_TYPE) != "Release" && !o.count("--allow-debug")) {
        throw std::invalid_argument("Benchmark requires Release build; --allow-debug is for tests only.");
    }
    const int warmups = o.count("--warmup") ? number(o.at("--warmup"), 2, 1000) : 2;
    const int runs = o.count("--runs") ? number(o.at("--runs"), 7, 10000) : 7;
    std::vector<int> threads;
    if (o.count("--threads")) {
        for (const auto& item : split(o.at("--threads"))) threads.push_back(number(item, 1, sw::max_batch_threads()));
    } else {
        for (const int n : {1, 2, 4, 8}) if (n <= sw::max_batch_threads()) threads.push_back(n);
        if (threads.back() != sw::max_batch_threads()) threads.push_back(sw::max_batch_threads());
    }
    std::vector<sw::Schedule> schedules;
    for (const auto& item : split(o.count("--schedules") ? o.at("--schedules") : "static,dynamic")) {
        if (item == "static") schedules.push_back(sw::Schedule::Static);
        else if (item == "dynamic") schedules.push_back(sw::Schedule::Dynamic);
        else throw std::invalid_argument("Unknown schedule: " + item);
    }
    const auto configs = sw::bench::configurations(threads, schedules);
    std::vector<sw::bench::Dataset> datasets;
    const bool external = o.count("--query") || o.count("--query-id") || o.count("--references") || o.count("--dataset-id");
    if (external) {
        if (o.count("--suite")) throw std::invalid_argument("Cannot combine external FASTA and --suite.");
        const auto queries = sw::read_fasta(required(o, "--query"));
        auto refs = sw::read_fasta(required(o, "--references"));
        const auto& id = required(o, "--query-id");
        const auto it = std::find_if(queries.begin(), queries.end(), [&](const auto& q) { return q.id == id; });
        if (it == queries.end()) throw std::invalid_argument("Query ID not found: " + id);
        const auto dataset_id = o.count("--dataset-id") ? o.at("--dataset-id") : "external";
        datasets.push_back({dataset_id, "external", "", *it, std::move(refs)});
    } else datasets = sw::bench::make_suite(o.count("--suite") ? o.at("--suite") : "all");
    for (const auto& d : datasets) {
        if (d.id.empty()) throw std::invalid_argument("Dataset ID cannot be empty.");
        for (const unsigned char c : d.id) {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
                throw std::invalid_argument("Dataset ID must contain ASCII letters, digits, underscore or hyphen.");
            }
        }
    }
    if (!std::filesystem::create_directories(output)) throw std::runtime_error("Output directory already exists.");
    std::filesystem::create_directory(output / "data");
    for (const auto& d : datasets) {
        sw::write_fasta(output / "data" / (d.id + "_query.fasta"), {d.query});
        sw::write_fasta(output / "data" / (d.id + "_references.fasta"), d.references);
    }
    metadata(output / "run_metadata.json", datasets, configs, warmups, runs, false);
    std::ofstream raw(output / "raw_benchmark.csv", std::ios::binary);
    std::ofstream summary(output / "summary_benchmark.csv", std::ios::binary);
    if (!raw || !summary) throw std::runtime_error("Cannot create benchmark CSV files.");
    raw.imbue(std::locale::classic()); summary.imbue(std::locale::classic());
    sw::bench::raw_header(raw); sw::bench::summary_header(summary);
    for (const auto& d : datasets) {
        std::cout << "Measuring " << d.id << ": " << sw::bench::dp_cells(d) << " DP cells, "
                  << configs.size() << " configurations" << std::endl;
        const auto samples = sw::bench::measure(d, configs, warmups, runs);
        const auto rows = sw::bench::summarize(samples, sw::bench::dp_cells(d));
        sw::bench::write_raw(raw, d, samples, SW_BENCHMARK_BUILD_TYPE);
        sw::bench::write_summary(summary, d, rows, runs, SW_BENCHMARK_BUILD_TYPE);
        raw.flush(); summary.flush();
        if (!raw || !summary) throw std::runtime_error("Failed to write benchmark CSV.");
        std::cout << "Verified " << samples.size() << " samples for " << d.id << std::endl;
    }
    raw.close(); summary.close();
    if (!raw || !summary) throw std::runtime_error("Failed to close benchmark CSV.");
    metadata(output / "run_metadata.json", datasets, configs, warmups, runs, true);
    std::cout << "Complete: " << output.string() << '\n';
}
}  // namespace

int main(int argc, char** argv) {
    try { run(argc, argv); return 0; }
    catch (const std::exception& error) { std::cerr << "Error: " << error.what() << '\n'; return 1; }
}
