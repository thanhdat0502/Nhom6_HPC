#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace sw {

inline constexpr int match_score = 2;
inline constexpr int mismatch_score = -1;
inline constexpr int gap_score = -2;

struct ScoringConfig {
    int match = match_score;
    int mismatch = mismatch_score;
    int gap = gap_score;
};

// Linear-gap local alignment: positive match, nonpositive mismatch, negative gap.
void validate_scoring(const ScoringConfig& scoring);
void validate_dna(std::string_view dna);

// Accept lowercase DNA, reject every character outside A/C/G/T. Empty is valid.
std::string normalize_dna(std::string_view dna);

class ScoreMatrix {
public:
    ScoreMatrix(std::size_t rows, std::size_t columns);
    std::size_t rows() const noexcept { return rows_; }
    std::size_t columns() const noexcept { return columns_; }
    int& at(std::size_t row, std::size_t column);
    int at(std::size_t row, std::size_t column) const;

private:
    std::size_t rows_;
    std::size_t columns_;
    std::vector<int> cells_;
};

struct MaxCell {
    std::size_t row = 0;
    std::size_t column = 0;
    int score = 0;
};

struct Alignment {
    int score = 0;
    std::string aligned_a;
    std::string aligned_b;
    // Zero-based half-open intervals in the original, ungapped sequences.
    std::size_t start_a = 0;
    std::size_t end_a = 0;
    std::size_t start_b = 0;
    std::size_t end_b = 0;
};

ScoreMatrix build_score_matrix(std::string_view a, std::string_view b);
MaxCell find_max_cell(const ScoreMatrix& matrix);
int smith_waterman_score(std::string_view a, std::string_view b);
int smith_waterman_score(std::string_view a, std::string_view b, const ScoringConfig& scoring);
Alignment smith_waterman_alignment(std::string_view a, std::string_view b);
void print_alignment(std::ostream& output, const Alignment& alignment);

}  // namespace sw
