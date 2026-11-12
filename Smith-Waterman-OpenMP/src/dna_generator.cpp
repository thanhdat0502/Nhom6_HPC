#include "dna_generator.h"
#include "smith_waterman.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>

namespace sw {

std::vector<FastaRecord> generate_dna(std::size_t count, std::size_t length, std::uint32_t seed) {
    if (count == 0 || length == 0) throw std::invalid_argument("DNA count and length must be positive.");
    if (count > std::numeric_limits<std::size_t>::max() / length) {
        throw std::length_error("Requested DNA dataset is too large.");
    }
    std::mt19937 engine(seed);
    constexpr char bases[] = "ACGT";
    std::vector<FastaRecord> records;
    records.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::string dna(length, 'A');
        // mt19937 output is specified; modulo 4 has no bias for its 32-bit range.
        // Avoid uniform_int_distribution: its mapping can vary between libraries.
        for (char& c : dna) c = bases[engine() % 4];
        records.push_back({"seq_" + std::to_string(i + 1), "seed=" + std::to_string(seed), std::move(dna)});
    }
    return records;
}

void write_fasta(const std::filesystem::path& path, const std::vector<FastaRecord>& records,
                 std::size_t line_width) {
    if (records.empty() || line_width == 0) throw std::invalid_argument("Records and line width must be nonzero.");
    if (std::filesystem::exists(path)) throw std::runtime_error("Refusing to overwrite existing file: " + path.string());
    // Validate everything before creating the output file.
    std::set<std::string> ids;
    for (const auto& record : records) {
        if (record.id.empty() || record.id.find_first_of(" \t\r\n\v\f>") != std::string::npos ||
            record.description.find_first_of("\r\n") != std::string::npos || record.sequence.empty()) {
            throw std::invalid_argument("Invalid or empty FASTA record.");
        }
        if (!ids.insert(record.id).second) throw std::invalid_argument("Duplicate FASTA ID: " + record.id);
        normalize_dna(record.sequence);
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("Cannot create FASTA file: " + path.string());
    for (const auto& record : records) {
        output << '>' << record.id;
        if (!record.description.empty()) output << ' ' << record.description;
        output << '\n';
        const auto dna = normalize_dna(record.sequence);
        for (std::size_t offset = 0; offset < dna.size();) {
            const auto width = std::min(line_width, dna.size() - offset);
            output.write(dna.data() + offset, static_cast<std::streamsize>(width));
            output << '\n';
            offset += width;
        }
    }
    output.close();
    if (!output) throw std::runtime_error("I/O error while writing FASTA: " + path.string());
}

}  // namespace sw
