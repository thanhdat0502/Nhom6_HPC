#include "smith_waterman.h"

#include <algorithm>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace sw {
namespace {

void check_score_range(std::size_t m, std::size_t n, int match = match_score) {
    if (std::min(m, n) > static_cast<std::size_t>(std::numeric_limits<int>::max() / match)) {
        throw std::length_error("DNA lengths exceed the supported integer score range.");
    }
}

int substitution(char a, char b) { return a == b ? match_score : mismatch_score; }

Alignment traceback(const std::string& a, const std::string& b,
                    const ScoreMatrix& matrix, const MaxCell& maximum) {
    Alignment result;
    result.score = maximum.score;
    std::size_t i = maximum.row;
    std::size_t j = maximum.column;
    result.end_a = i;
    result.end_b = j;
    // Deterministic ties: diagonal, then up, then left. Stop at zero.
    while (i > 0 && j > 0 && matrix.at(i, j) > 0) {
        const int value = matrix.at(i, j);
        if (value == matrix.at(i - 1, j - 1) + substitution(a[i - 1], b[j - 1])) {
            result.aligned_a += a[--i];
            result.aligned_b += b[--j];
        } else if (value == matrix.at(i - 1, j) + gap_score) {
            result.aligned_a += a[--i];
            result.aligned_b += '-';
        } else if (value == matrix.at(i, j - 1) + gap_score) {
            result.aligned_a += '-';
            result.aligned_b += b[--j];
        } else {
            throw std::logic_error("Traceback is inconsistent with the score matrix.");
        }
    }
    result.start_a = i;
    result.start_b = j;
    std::reverse(result.aligned_a.begin(), result.aligned_a.end());
    std::reverse(result.aligned_b.begin(), result.aligned_b.end());
    return result;
}

}  // namespace

void validate_scoring(const ScoringConfig& scoring) {
    if (scoring.match <= 0 || scoring.mismatch > 0 || scoring.gap >= 0) {
        throw std::invalid_argument("Scoring requires match > 0, mismatch <= 0 and gap < 0.");
    }
}

void validate_dna(std::string_view dna) {
    for (const char c : dna) {
        if (c != 'A' && c != 'C' && c != 'G' && c != 'T' &&
            c != 'a' && c != 'c' && c != 'g' && c != 't') {
            throw std::invalid_argument("Invalid DNA; only A, C, G, T are supported.");
        }
    }
}

std::string normalize_dna(std::string_view dna) {
    std::string result;
    result.reserve(dna.size());
    for (std::size_t i = 0; i < dna.size(); ++i) {
        char c = dna[i];
        switch (c) {
            case 'a': c = 'A'; break;
            case 'c': c = 'C'; break;
            case 'g': c = 'G'; break;
            case 't': c = 'T'; break;
            default: break;
        }
        if (c != 'A' && c != 'C' && c != 'G' && c != 'T') {
            throw std::invalid_argument("Invalid DNA character (byte " +
                                        std::to_string(static_cast<unsigned char>(dna[i])) +
                                        ") at position " + std::to_string(i + 1) +
                                        "; only A, C, G, T are supported.");
        }
        result += c;
    }
    return result;
}

ScoreMatrix::ScoreMatrix(std::size_t rows, std::size_t columns)
    : rows_(rows), columns_(columns) {
    if (rows == 0 || columns == 0) {
        throw std::invalid_argument("Score matrix dimensions must be positive.");
    }
    if (rows > cells_.max_size() / columns) {
        throw std::length_error("Score matrix is too large; use score-only mode.");
    }
    cells_.assign(rows * columns, 0);
}

int& ScoreMatrix::at(std::size_t row, std::size_t column) {
    if (row >= rows_ || column >= columns_) throw std::out_of_range("Score matrix index.");
    return cells_[row * columns_ + column];
}

int ScoreMatrix::at(std::size_t row, std::size_t column) const {
    if (row >= rows_ || column >= columns_) throw std::out_of_range("Score matrix index.");
    return cells_[row * columns_ + column];
}

ScoreMatrix build_score_matrix(std::string_view input_a, std::string_view input_b) {
    const auto a = normalize_dna(input_a);
    const auto b = normalize_dna(input_b);
    check_score_range(a.size(), b.size());
    ScoreMatrix matrix(a.size() + 1, b.size() + 1);
    for (std::size_t i = 1; i <= a.size(); ++i) {
        for (std::size_t j = 1; j <= b.size(); ++j) {
            matrix.at(i, j) = std::max({0,
                matrix.at(i - 1, j - 1) + substitution(a[i - 1], b[j - 1]),
                matrix.at(i - 1, j) + gap_score,
                matrix.at(i, j - 1) + gap_score});
        }
    }
    return matrix;
}

MaxCell find_max_cell(const ScoreMatrix& matrix) {
    MaxCell result;
    // Strictly greater preserves the first maximum in row-major order.
    for (std::size_t i = 0; i < matrix.rows(); ++i) {
        for (std::size_t j = 0; j < matrix.columns(); ++j) {
            if (matrix.at(i, j) > result.score) result = {i, j, matrix.at(i, j)};
        }
    }
    return result;
}

int smith_waterman_score(std::string_view input_a, std::string_view input_b) {
    return smith_waterman_score(input_a, input_b, ScoringConfig{});
}

int smith_waterman_score(std::string_view input_a, std::string_view input_b,
                         const ScoringConfig& scoring) {
    // Validate both inputs even when one is empty. Do not copy whole inputs:
    // auxiliary memory stays O(min(m,n)), including lowercase handling.
    validate_scoring(scoring);
    validate_dna(input_a);
    validate_dna(input_b);
    check_score_range(input_a.size(), input_b.size(), scoring.match);
    if (input_a.empty() || input_b.empty()) return 0;
    if (input_a.size() < input_b.size()) std::swap(input_a, input_b);
    const auto upper = [](char c) { return c >= 'a' && c <= 't' ? static_cast<char>(c - 'a' + 'A') : c; };
    std::vector<int> previous(input_b.size() + 1, 0);
    std::vector<int> current(input_b.size() + 1, 0);
    int best = 0;
    for (const char a : input_a) {
        current[0] = 0;
        for (std::size_t j = 1; j <= input_b.size(); ++j) {
            current[j] = std::max({0, previous[j - 1] +
                                     (upper(a) == upper(input_b[j - 1]) ? scoring.match : scoring.mismatch),
                                   previous[j] + scoring.gap, current[j - 1] + scoring.gap});
            best = std::max(best, current[j]);
        }
        std::swap(previous, current);
    }
    return best;
}

Alignment smith_waterman_alignment(std::string_view input_a, std::string_view input_b) {
    const auto a = normalize_dna(input_a);
    const auto b = normalize_dna(input_b);
    const auto matrix = build_score_matrix(a, b);
    return traceback(a, b, matrix, find_max_cell(matrix));
}

void print_alignment(std::ostream& output, const Alignment& alignment) {
    output << "Score: " << alignment.score << "\nA [" << alignment.start_a << ", " << alignment.end_a
           << ")\nB [" << alignment.start_b << ", " << alignment.end_b << ")\n";
    if (alignment.aligned_a.empty()) {
        output << "No positive local alignment.\n";
        return;
    }
    constexpr std::size_t width = 80;
    for (std::size_t offset = 0; offset < alignment.aligned_a.size(); offset += width) {
        const auto a = alignment.aligned_a.substr(offset, width);
        const auto b = alignment.aligned_b.substr(offset, width);
        std::string markers;
        for (std::size_t k = 0; k < a.size(); ++k) {
            markers += (a[k] == '-' || b[k] == '-') ? ' ' : (a[k] == b[k] ? '|' : '.');
        }
        output << a << '\n' << markers << '\n' << b << "\n\n";
    }
}

}  // namespace sw
