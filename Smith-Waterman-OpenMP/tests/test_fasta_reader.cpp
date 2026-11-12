#include "fasta_reader.h"
#include "test_support.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::vector<sw::FastaRecord> parse(const std::string& text) {
    std::istringstream input(text);
    return sw::parse_fasta(input, "fixture.fasta");
}
}

int main() {
    test::Runner tests;
    tests.run("multiple records, multiline DNA, lowercase and descriptions", [] {
        const auto records = parse(">id1 first description\nacg\nT\n\n>id2\ngg\ncc\n");
        test::require(records.size() == 2, "Record count.");
        test::require(records[0].id == "id1" && records[0].description == "first description" &&
                      records[0].sequence == "ACGT", "First record.");
        test::require(records[1].id == "id2" && records[1].description.empty() && records[1].sequence == "GGCC", "Second record.");
    });
    tests.run("CRLF, whitespace-only blank lines and no final newline", [] {
        const auto records = parse("\r\n \t\r\n>one note\r\nac\r\ngt\r\n>two\r\nTT");
        test::require(records.size() == 2 && records[0].sequence == "ACGT" && records[1].sequence == "TT", "CRLF content.");
    });
    tests.run("invalid nucleotide includes source and line", [] {
        bool caught = false;
        try { parse(">bad\nACNT\n"); } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            test::require(message.find("fixture.fasta: line 2") != std::string::npos &&
                          message.find("only A, C, G, T") != std::string::npos, "Diagnostic context.");
            caught = true;
        }
        test::require(caught, "Invalid N must fail.");
    });
    tests.run("never discard spaces, gaps, comments or ambiguous bases", [] {
        for (const std::string text : {"AC GT", " ACGT", "ACGT ", "AC\tGT", "A-C", "ACU", "ACR", ";comment", "AC>GT"}) {
            test::throws<std::runtime_error>([&] { parse(">id\n" + text + "\n"); });
        }
    });
    tests.run("missing header or misplaced header", [] {
        test::throws<std::runtime_error>([] { parse("ACGT\n"); });
        test::throws<std::runtime_error>([] { parse(" >id\nACGT\n"); });
    });
    tests.run("empty file and blank file", [] {
        test::throws<std::runtime_error>([] { parse(""); });
        test::throws<std::runtime_error>([] { parse("\n\r\n \t\n"); });
    });
    tests.run("missing ID", [] {
        test::throws<std::runtime_error>([] { parse(">\nACGT\n"); });
        test::throws<std::runtime_error>([] { parse("> \t\nACGT\n"); });
    });
    tests.run("empty first, middle or final record", [] {
        for (const std::string text : {">id\n", ">id1\n\n>id2\nAC\n", ">id1\nAC\n>id2\n>id3\nGT\n", ">id1\nAC\n>id2\n"}) {
            test::throws<std::runtime_error>([&] { parse(text); });
        }
    });
    tests.run("duplicate IDs rejected even with different descriptions", [] {
        test::throws<std::runtime_error>([] { parse(">id first\nAC\n>id second\nGT\n"); });
    });
    tests.run("actual file path with spaces", [] {
        test::TempFile file;
        {
            std::ofstream output(file.path(), std::ios::binary);
            output << ">from_disk description\r\nac\r\ngt\r\n";
        }
        const auto records = sw::read_fasta(file.path());
        test::require(records.size() == 1 && records[0].id == "from_disk" && records[0].sequence == "ACGT", "File record.");
    });
    tests.run("nonexistent file", [] {
        test::TempFile file;
        test::throws<std::runtime_error>([&] { sw::read_fasta(file.path()); });
    });
    tests.run("stream I/O failure", [] {
        std::istringstream input(">id\nACGT\n");
        input.setstate(std::ios::badbit);
        test::throws<std::runtime_error>([&] { sw::parse_fasta(input); });
    });
    return tests.finish();
}
