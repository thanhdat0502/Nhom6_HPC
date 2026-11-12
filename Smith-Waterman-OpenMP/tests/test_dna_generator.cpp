#include "dna_generator.h"
#include "smith_waterman.h"
#include "test_support.h"

#include <fstream>
#include <iterator>
#include <limits>

namespace {
std::string contents(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main() {
    test::Runner tests;
    tests.run("same parameters reproduce records", [] {
        const auto first = sw::generate_dna(4, 128, 42);
        const auto second = sw::generate_dna(4, 128, 42);
        test::require(first.size() == 4, "Record count.");
        for (std::size_t i = 0; i < first.size(); ++i) {
            test::require(first[i].sequence == second[i].sequence && first[i].id == second[i].id &&
                          first[i].description == second[i].description, "Reproducibility.");
            test::require(first[i].sequence.size() == 128 && sw::normalize_dna(first[i].sequence) == first[i].sequence, "DNA content.");
        }
    });
    tests.run("known mt19937 seed-42 mapping", [] {
        test::require(sw::generate_dna(1, 8, 42)[0].sequence == "GTAGGTAA", "Stable mapping.");
    });
    tests.run("different seed changes the fixture", [] {
        test::require(sw::generate_dna(1, 128, 42)[0].sequence != sw::generate_dna(1, 128, 43)[0].sequence, "Different seeds.");
    });
    tests.run("zero and maximum uint32 seeds", [] {
        test::require(sw::generate_dna(1, 1, 0)[0].sequence.size() == 1, "Zero seed.");
        test::require(sw::generate_dna(1, 1, std::numeric_limits<std::uint32_t>::max())[0].sequence.size() == 1, "Maximum seed.");
    });
    tests.run("reject zero counts, zero lengths and size overflow", [] {
        test::throws<std::invalid_argument>([] { sw::generate_dna(0, 10, 42); });
        test::throws<std::invalid_argument>([] { sw::generate_dna(1, 0, 42); });
        test::throws<std::length_error>([] { sw::generate_dna(std::numeric_limits<std::size_t>::max(), 2, 42); });
    });
    tests.run("FASTA write/read round trip, wrapping and byte reproducibility", [] {
        const auto records = sw::generate_dna(3, 161, 99);
        test::TempFile file1, file2;
        sw::write_fasta(file1.path(), records);
        sw::write_fasta(file2.path(), sw::generate_dna(3, 161, 99));
        test::require(contents(file1.path()) == contents(file2.path()), "Byte reproducibility.");
        const auto loaded = sw::read_fasta(file1.path());
        test::require(loaded.size() == records.size(), "Round-trip count.");
        for (std::size_t i = 0; i < loaded.size(); ++i) {
            test::require(loaded[i].id == records[i].id && loaded[i].sequence == records[i].sequence &&
                          loaded[i].description == records[i].description, "Round-trip content.");
        }
        std::ifstream input(file1.path());
        std::string line;
        while (std::getline(input, line)) {
            if (line.front() != '>') test::require(line.size() <= 80, "Line wrapping.");
        }
    });
    tests.run("refuse overwrite and preserve existing bytes", [] {
        test::TempFile file;
        sw::write_fasta(file.path(), sw::generate_dna(1, 8, 42));
        const auto before = contents(file.path());
        test::throws<std::runtime_error>([&] { sw::write_fasta(file.path(), sw::generate_dna(1, 8, 99)); });
        test::require(contents(file.path()) == before, "Existing file preserved.");
    });
    tests.run("invalid export parameters fail before file creation", [] {
        test::TempFile file;
        test::throws<std::invalid_argument>([&] { sw::write_fasta(file.path(), {}); });
        test::throws<std::invalid_argument>([&] { sw::write_fasta(file.path(), {{"id", "", "ACGT"}}, 0); });
        for (const auto& record : std::vector<sw::FastaRecord>{{"", "", "AC"}, {"bad id", "", "AC"},
                   {"id", "bad\nheader", "AC"}, {"id", "", ""}, {"id", "", "AN"}}) {
            test::throws<std::invalid_argument>([&] { sw::write_fasta(file.path(), {record}); });
        }
        test::throws<std::invalid_argument>([&] { sw::write_fasta(file.path(), {{"id", "", "AC"}, {"id", "", "GT"}}); });
        test::require(!std::filesystem::exists(file.path()), "No invalid output file.");
    });
    tests.run("export lowercase normalization", [] {
        test::TempFile file;
        sw::write_fasta(file.path(), {{"id", "description", "acgt"}}, 2);
        test::require(contents(file.path()) == ">id description\nAC\nGT\n", "Normalized output.");
    });
    tests.run("output parent must exist", [] {
        test::TempFile file;
        test::throws<std::runtime_error>([&] { sw::write_fasta(file.path() / "missing.fasta", sw::generate_dna(1, 8, 42)); });
    });
    return tests.finish();
}
