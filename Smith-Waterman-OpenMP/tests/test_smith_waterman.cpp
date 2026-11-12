#include "smith_waterman.h"
#include "dna_generator.h"
#include "test_support.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <tuple>
#include <vector>

namespace {

void check_alignment(const std::string& a_input, const std::string& b_input, const sw::Alignment& result) {
    const auto a = sw::normalize_dna(a_input);
    const auto b = sw::normalize_dna(b_input);
    test::require(result.aligned_a.size() == result.aligned_b.size(), "Unequal alignment lengths.");
    test::require(result.start_a <= result.end_a && result.end_a <= a.size(), "A coordinates.");
    test::require(result.start_b <= result.end_b && result.end_b <= b.size(), "B coordinates.");
    std::string ungapped_a, ungapped_b;
    int score = 0;
    for (std::size_t i = 0; i < result.aligned_a.size(); ++i) {
        const char x = result.aligned_a[i];
        const char y = result.aligned_b[i];
        test::require(x != '-' || y != '-', "Double gap.");
        if (x != '-') ungapped_a += x;
        if (y != '-') ungapped_b += y;
        score += (x == '-' || y == '-') ? -2 : (x == y ? 2 : -1);
    }
    test::require(score == result.score, "Traceback rescoring.");
    test::require(ungapped_a == a.substr(result.start_a, result.end_a - result.start_a), "Traceback A substring.");
    test::require(ungapped_b == b.substr(result.start_b, result.end_b - result.start_b), "Traceback B substring.");
    test::require((result.score == 0) == result.aligned_a.empty(), "Empty alignment convention.");
}

// Independent short-input oracle: enumerate substring pairs, then compute GLOBAL
// alignment with negative boundaries and WITHOUT the Smith-Waterman zero reset.
int global_short_score(std::string_view a, std::string_view b) {
    int scores[4][4] = {};
    for (std::size_t i = 1; i <= a.size(); ++i) scores[i][0] = -2 * static_cast<int>(i);
    for (std::size_t j = 1; j <= b.size(); ++j) scores[0][j] = -2 * static_cast<int>(j);
    for (std::size_t i = 1; i <= a.size(); ++i) {
        for (std::size_t j = 1; j <= b.size(); ++j) {
            scores[i][j] = std::max({scores[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 2 : -1),
                                      scores[i - 1][j] - 2, scores[i][j - 1] - 2});
        }
    }
    return scores[a.size()][b.size()];
}

int exhaustive_local_score(const std::string& a, const std::string& b) {
    int best = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t length_a = 1; length_a <= a.size() - i; ++length_a) {
            for (std::size_t j = 0; j < b.size(); ++j) {
                for (std::size_t length_b = 1; length_b <= b.size() - j; ++length_b) {
                    best = std::max(best, global_short_score(std::string_view(a).substr(i, length_a),
                                                            std::string_view(b).substr(j, length_b)));
                }
            }
        }
    }
    return best;
}

}  // namespace

int main() {
    test::Runner tests;
    const std::vector<std::tuple<std::string, std::string, int>> cases = {
        {"ACGT", "ACGT", 8}, {"ACGT", "ACG", 6}, {"AAAA", "TTTT", 0},
        {"A", "A", 2}, {"A", "G", 0}, {"", "", 0}, {"", "ACGT", 0}
    };
    for (std::size_t i = 0; i < cases.size(); ++i) {
        tests.run("required case " + std::to_string(i + 1), [&] {
            const auto& [a, b, expected] = cases[i];
            const auto alignment = sw::smith_waterman_alignment(a, b);
            test::require(sw::smith_waterman_score(a, b) == expected, "Expected score-only value.");
            test::require(alignment.score == expected, "Expected alignment score.");
            check_alignment(a, b, alignment);
        });
    }
    tests.run("gap traceback in both orientations", [] {
        for (const bool swap : {false, true}) {
            const std::string a = swap ? "AAAAGCCCC" : "AAAACCCC";
            const std::string b = swap ? "AAAACCCC" : "AAAAGCCCC";
            const auto alignment = sw::smith_waterman_alignment(a, b);
            test::require(alignment.score == 14 && sw::smith_waterman_score(a, b) == 14, "Gap score.");
            test::require((swap ? alignment.aligned_b : alignment.aligned_a) == "AAAA-CCCC", "Gap location.");
            check_alignment(a, b, alignment);
        }
    });
    tests.run("mismatch traceback", [] {
        const auto alignment = sw::smith_waterman_alignment("AAAACCCC", "AAAATCCC");
        test::require(alignment.score == 13, "Mismatch score.");
        check_alignment("AAAACCCC", "AAAATCCC", alignment);
    });
    tests.run("deterministic tied maximum and zero origin", [] {
        const auto alignment = sw::smith_waterman_alignment("AC", "CA");
        test::require(alignment.score == 2 && alignment.aligned_a == "A" && alignment.aligned_b == "A", "Tie base.");
        test::require(alignment.start_a == 0 && alignment.end_a == 1 && alignment.start_b == 1 && alignment.end_b == 2, "First maximum.");
        const auto zero = sw::smith_waterman_alignment("AAAA", "TTTT");
        test::require(zero.start_a == 0 && zero.end_a == 0 && zero.start_b == 0 && zero.end_b == 0, "Zero origin.");
    });
    tests.run("known matrix and bounds", [] {
        const auto matrix = sw::build_score_matrix("AC", "AG");
        const int expected[3][3] = {{0, 0, 0}, {0, 2, 0}, {0, 0, 1}};
        for (std::size_t i = 0; i < 3; ++i) {
            for (std::size_t j = 0; j < 3; ++j) test::require(matrix.at(i, j) == expected[i][j], "Matrix cell.");
        }
        const auto maximum = sw::find_max_cell(matrix);
        test::require(maximum.row == 1 && maximum.column == 1 && maximum.score == 2, "Matrix maximum.");
        test::throws<std::out_of_range>([&] { matrix.at(3, 0); });
        test::throws<std::length_error>([] { sw::ScoreMatrix huge(std::numeric_limits<std::size_t>::max(), 2); });
        test::throws<std::invalid_argument>([] { sw::ScoreMatrix empty(0, 1); });
    });
    tests.run("invalid DNA including empty opposite input", [] {
        for (const std::string invalid : {"ACNT", "A C", "AC-G", "U", "A\tC"}) {
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_score(invalid, ""); });
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_score("", invalid); });
            test::throws<std::invalid_argument>([&] { sw::smith_waterman_alignment(invalid, "A"); });
        }
    });
    tests.run("lowercase DNA", [] {
        test::require(sw::smith_waterman_score("acGT", "ACgt") == 8, "Lowercase score.");
        const auto result = sw::smith_waterman_alignment("acGT", "ACgt");
        test::require(result.score == 8, "Lowercase alignment score.");
        check_alignment("acGT", "ACgt", result);
    });
    tests.run("all 7225 DNA pairs up to length 3 against independent oracle", [] {
        std::vector<std::string> sequences = {""};
        std::vector<std::string> level = {""};
        for (int length = 1; length <= 3; ++length) {
            std::vector<std::string> next;
            for (const auto& prefix : level) {
                for (const char base : std::string("ACGT")) next.push_back(prefix + base);
            }
            sequences.insert(sequences.end(), next.begin(), next.end());
            level = std::move(next);
        }
        test::require(sequences.size() == 85, "Oracle corpus size.");
        for (const auto& a : sequences) {
            for (const auto& b : sequences) {
                const int expected = exhaustive_local_score(a, b);
                const auto alignment = sw::smith_waterman_alignment(a, b);
                test::require(alignment.score == expected && sw::smith_waterman_score(a, b) == expected,
                              "Oracle mismatch for " + a + " vs " + b);
                check_alignment(a, b, alignment);
            }
        }
    });
    tests.run("250 seeded longer pairs: score, symmetry, coordinates, traceback", [] {
        const auto records = sw::generate_dna(500, 96, 12345);
        for (std::size_t i = 0; i < records.size(); i += 2) {
            const auto a = records[i].sequence.substr(i % 97);
            const auto& b = records[i + 1].sequence;
            const auto result = sw::smith_waterman_alignment(a, b);
            test::require(result.score == sw::smith_waterman_score(a, b), "Mode agreement.");
            test::require(result.score == sw::smith_waterman_score(b, a), "Symmetry.");
            check_alignment(a, b, result);
        }
    });
    tests.run("1200-base identity and asymmetric rolling rows", [] {
        const auto dna = sw::generate_dna(1, 1200, 42).front().sequence;
        const auto result = sw::smith_waterman_alignment(dna, dna);
        test::require(result.score == 2400 && sw::smith_waterman_score(dna, dna) == 2400, "Long identity.");
        check_alignment(dna, dna, result);
        test::require(sw::smith_waterman_score(dna, "ACGT") == sw::smith_waterman_score("ACGT", dna), "Shorter row.");
    });
    tests.run("formatted alignment and no-match message", [] {
        std::ostringstream output;
        sw::print_alignment(output, sw::smith_waterman_alignment("ACGT", "ACGT"));
        test::require(output.str().find("Score: 8") != std::string::npos && output.str().find("||||") != std::string::npos, "Alignment output.");
        output.str("");
        sw::print_alignment(output, sw::smith_waterman_alignment("A", "T"));
        test::require(output.str().find("No positive local alignment") != std::string::npos, "No match output.");
    });
    return tests.finish();
}
