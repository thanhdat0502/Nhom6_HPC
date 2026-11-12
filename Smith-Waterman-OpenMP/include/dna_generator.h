#pragma once

#include "fasta_reader.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace sw {

std::vector<FastaRecord> generate_dna(std::size_t count, std::size_t length, std::uint32_t seed);
// Refuse to replace an existing path; wrap sequence lines at 80 bases by default.
void write_fasta(const std::filesystem::path& path, const std::vector<FastaRecord>& records,
                 std::size_t line_width = 80);

}  // namespace sw
