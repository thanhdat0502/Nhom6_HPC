#include "fasta_reader.h"
#include "smith_waterman.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <istream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sw {

std::vector<FastaRecord> parse_fasta(std::istream& input, const std::string& source) {
    std::vector<FastaRecord> records;
    std::set<std::string> ids;
    std::string line;
    std::size_t line_number = 0;
    const auto fail = [&](const std::string& reason) {
        throw std::runtime_error(source + ": line " + std::to_string(line_number) + ": " + reason);
    };
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isspace(c) != 0; })) {
            continue;
        }
        if (line.front() == '>') {
            if (!records.empty() && records.back().sequence.empty()) fail("Empty sequence for ID " + records.back().id);
            FastaRecord record;
            std::istringstream header(line.substr(1));
            if (!(header >> record.id)) fail("FASTA header is missing an ID.");
            std::getline(header >> std::ws, record.description);
            if (!ids.insert(record.id).second) fail("Duplicate FASTA ID: " + record.id);
            records.push_back(std::move(record));
        } else {
            if (records.empty()) fail("Sequence appears before the first >header.");
            try {
                records.back().sequence += normalize_dna(line);
            } catch (const std::invalid_argument& error) {
                fail(error.what());
            }
        }
    }
    if (input.bad() || (input.fail() && !input.eof())) fail("I/O error while reading FASTA.");
    if (records.empty()) fail("No FASTA records found.");
    if (records.back().sequence.empty()) fail("Empty sequence for ID " + records.back().id);
    return records;
}

std::vector<FastaRecord> read_fasta(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open FASTA file: " + path.string());
    return parse_fasta(input, path.string());
}

}  // namespace sw
