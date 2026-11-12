#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace sw {

struct FastaRecord {
    std::string id;
    std::string description;
    std::string sequence;
};

// Nonempty, unique IDs and nonempty sequences are required for FASTA records.
std::vector<FastaRecord> parse_fasta(std::istream& input, const std::string& source = "<stream>");
std::vector<FastaRecord> read_fasta(const std::filesystem::path& path);

}  // namespace sw
