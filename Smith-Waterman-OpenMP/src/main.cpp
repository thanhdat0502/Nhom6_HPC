#include "dna_generator.h"
#include "fasta_reader.h"
#include "smith_waterman.h"
#include "smith_waterman_parallel.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

namespace {

void help() {
    std::cout <<
        "Smith-Waterman serial / OpenMP batch - Week 2 (C++17)\n"
        "Scoring: match +2, mismatch -1, gap -2. DNA: A/C/G/T (case-insensitive).\n\n"
        "Usage:\n"
        "  sw_cli --help\n"
        "  sw_cli align --seq1 DNA --seq2 DNA [--alignment]\n"
        "  sw_cli search --query FILE --query-id ID --references FILE [--alignment-id ID]\n"
        "                [--mode serial|parallel] [--threads N] [--schedule static|dynamic] [--stats]\n"
        "  sw_cli generate --count N --length N --seed N --output FILE\n\n"
        "align: score-only by default; --alignment prints one optimal traceback.\n"
        "search: default serial (1 thread); parallel defaults to min(4, available).\n"
        "        Thread requests must fit the processor/OpenMP limit; no oversubscription.\n"
        "        Dynamic scheduling is default; result order always follows FASTA.\n"
        "        --alignment-id traces one pair serially after batch scoring.\n"
        "        --stats writes mode, requested/actual threads and compute seconds to stderr.\n"
        "generate: positive count/length, unsigned 32-bit seed; refuses to overwrite.\n"
        "FASTA: unique IDs, nonempty records, multiline DNA, LF/CRLF supported.\n"
        "Coordinates: zero-based half-open intervals [start, end).\n"
        "Exit codes: 0 success, 1 invalid input/I/O/resource failure.\n";
    std::cout << "Max supported batch threads: " << sw::max_batch_threads() << '\n';
}

using Options = std::map<std::string, std::string>;

Options parse_options(int argc, char* argv[], const std::set<std::string>& allowed,
                      const std::set<std::string>& flags = {}) {
    Options options;
    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (allowed.count(option) == 0) throw std::invalid_argument("Unknown option: " + option);
        if (options.count(option) != 0) throw std::invalid_argument("Repeated option: " + option);
        if (flags.count(option) != 0) {
            options.emplace(option, "");
        } else {
            if (i + 1 >= argc || std::string(argv[i + 1]).rfind("--", 0) == 0) {
                throw std::invalid_argument("Missing value for " + option);
            }
            options.emplace(option, argv[++i]);
        }
    }
    return options;
}

const std::string& required(const Options& options, const char* key) {
    const auto it = options.find(key);
    if (it == options.end()) throw std::invalid_argument("Missing required option: " + std::string(key));
    return it->second;
}

std::uint64_t number(const std::string& text, const std::string& name, std::uint64_t max, bool positive) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        text.empty() || value > max || (positive && value == 0)) {
        throw std::invalid_argument("Invalid integer for " + name + ": " + text);
    }
    return value;
}

const sw::FastaRecord& select_record(const std::vector<sw::FastaRecord>& records, const std::string& id) {
    const auto it = std::find_if(records.begin(), records.end(), [&](const auto& record) { return record.id == id; });
    if (it == records.end()) throw std::invalid_argument("FASTA ID not found: " + id);
    return *it;
}

void run(int argc, char* argv[]) {
    if (argc < 2) throw std::invalid_argument("Missing command. Run sw_cli --help.");
    const std::string command = argv[1];
    if (command == "--help") {
        if (argc != 2) throw std::invalid_argument("--help takes no other arguments.");
        help();
    } else if (command == "align") {
        const auto options = parse_options(argc, argv, {"--seq1", "--seq2", "--alignment"}, {"--alignment"});
        const auto& a = required(options, "--seq1");
        const auto& b = required(options, "--seq2");
        if (options.count("--alignment") != 0) {
            sw::print_alignment(std::cout, sw::smith_waterman_alignment(a, b));
        } else {
            const int score = sw::smith_waterman_score(a, b);
            std::cout << "Score: " << score << '\n';
        }
    } else if (command == "search") {
        const auto options = parse_options(argc, argv,
            {"--query", "--query-id", "--references", "--alignment-id", "--mode", "--threads", "--schedule", "--stats"},
            {"--stats"});
        const auto& query_file = required(options, "--query");
        const auto& query_id = required(options, "--query-id");
        const auto& reference_file = required(options, "--references");
        sw::BatchOptions batch_options;
        if (options.count("--mode") != 0) {
            const auto& mode = options.at("--mode");
            if (mode == "parallel") {
                batch_options.mode = sw::ExecutionMode::Parallel;
                batch_options.threads = std::min(4, sw::max_batch_threads());
            } else if (mode != "serial") {
                throw std::invalid_argument("Invalid --mode: expected serial or parallel.");
            }
        }
        if (options.count("--threads") != 0) {
            batch_options.threads = static_cast<int>(number(options.at("--threads"), "--threads",
                                                          static_cast<std::uint64_t>(sw::max_batch_threads()), true));
        }
        if (options.count("--schedule") != 0) {
            const auto& schedule = options.at("--schedule");
            if (schedule == "static") batch_options.schedule = sw::Schedule::Static;
            else if (schedule != "dynamic") throw std::invalid_argument("Invalid --schedule: expected static or dynamic.");
        }
        if (batch_options.mode == sw::ExecutionMode::Serial && batch_options.threads != 1) {
            throw std::invalid_argument("Serial mode requires --threads 1.");
        }
        const auto queries = sw::read_fasta(query_file);
        const auto references = sw::read_fasta(reference_file);
        const auto& query = select_record(queries, query_id);
        const sw::FastaRecord* chosen = nullptr;
        if (options.count("--alignment-id") != 0) chosen = &select_record(references, options.at("--alignment-id"));
        const auto batch = sw::smith_waterman_batch(query.sequence, references, {}, batch_options);
        std::optional<sw::Alignment> alignment;
        if (chosen != nullptr) alignment = sw::smith_waterman_alignment(query.sequence, chosen->sequence);
        std::cout << "Query ID: " << query.id << "\nReference ID\tScore\n";
        for (std::size_t i = 0; i < references.size(); ++i) {
            std::cout << references[i].id << '\t' << batch.scores[i] << '\n';
        }
        if (chosen != nullptr) {
            std::cout << "\nAlignment: " << query.id << " vs " << chosen->id << '\n';
            sw::print_alignment(std::cout, *alignment);
        }
        if (options.count("--stats") != 0) {
            std::cerr << "Mode: " << (batch.mode == sw::ExecutionMode::Serial ? "serial" : "parallel")
                      << "\nSchedule: " << (batch.schedule == sw::Schedule::Static ? "static" : "dynamic")
                      << "\nRequested threads: " << batch.requested_threads
                      << "\nActual threads: " << batch.actual_threads
                      << "\nCompute seconds: " << batch.compute_seconds << '\n';
        }
    } else if (command == "generate") {
        const auto options = parse_options(argc, argv, {"--count", "--length", "--seed", "--output"});
        const auto count = static_cast<std::size_t>(number(required(options, "--count"), "--count",
                                                         std::numeric_limits<std::size_t>::max(), true));
        const auto length = static_cast<std::size_t>(number(required(options, "--length"), "--length",
                                                          std::numeric_limits<std::size_t>::max(), true));
        const auto seed = static_cast<std::uint32_t>(number(required(options, "--seed"), "--seed",
                                                          std::numeric_limits<std::uint32_t>::max(), false));
        const auto& path = required(options, "--output");
        if (std::filesystem::exists(path)) throw std::runtime_error("Refusing to overwrite existing file: " + path);
        sw::write_fasta(path, sw::generate_dna(count, length, seed));
        std::cout << "Generated " << count << " sequences, length " << length << ", seed " << seed
                  << " -> " << path << '\n';
    } else {
        throw std::invalid_argument("Unknown command: " + command + ". Run sw_cli --help.");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        run(argc, argv);
        return 0;
    } catch (const std::bad_alloc&) {
        std::cerr << "Error: Not enough memory. Use score-only mode or smaller input.\n";
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
    }
    return 1;
}
