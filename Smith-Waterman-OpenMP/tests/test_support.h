#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace test {

inline void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception = std::exception, typename Function>
void throws(Function function) {
    bool caught = false;
    try { function(); } catch (const Exception&) { caught = true; }
    require(caught, "Expected exception was not thrown.");
}

class Runner {
public:
    template <typename Function>
    void run(const std::string& name, Function function) {
        try {
            function();
            ++passed_;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed_;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    int finish() const {
        std::cout << "Passed: " << passed_ << ", Failed: " << failed_ << '\n';
        return failed_ == 0 ? 0 : 1;
    }
private:
    int passed_ = 0;
    int failed_ = 0;
};

// Own one freshly created directory and one known file; never delete recursively.
class TempFile {
public:
    TempFile() {
        const auto tick = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned int i = 0; i < 100; ++i) {
            directory_ = std::filesystem::current_path() / ("test-temp-" + std::to_string(tick) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(directory_)) return;
        }
        throw std::runtime_error("Cannot create a fresh test directory.");
    }
    ~TempFile() {
        std::error_code error;
        std::filesystem::remove(path(), error);
        std::filesystem::remove(directory_, error);
    }
    std::filesystem::path path() const { return directory_ / "records.fasta"; }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
private:
    std::filesystem::path directory_;
};

}  // namespace test
