#include "utest.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>


std::unordered_map<std::string, std::vector<std::function<void()>>>& utest_suites()
{
    static std::unordered_map<std::string, std::vector<std::function<void()>>> _s;
    return _s;
}

std::unordered_map<std::string, std::vector<std::unique_ptr<BaseFixture>>>& utest_suite_proofs()
{
    static std::unordered_map<std::string, std::vector<std::unique_ptr<BaseFixture>>> _p;
    return _p;
}

std::string& utest_active_suite_name()
{
    static std::string _n;
    return _n;
}

std::vector<ProofFailure>& proof_failures()
{
    static std::vector<ProofFailure> _f;
    return _f;
}

std::vector<std::string> passed_proofs;

// Used for error reporting uncaught exceptions
std::string current_proof;

bool register_suite_function(const char* name, std::function<void()> suite_function)
{
    utest_suites()[name].push_back(suite_function);
    return true;
}

size_t suite_count()
{
    return utest_suites().size();
}

void register_passed_proof(const std::string& suite_name,
                           const std::string& proof_name)
{
    passed_proofs.emplace_back(suite_name + "::" + proof_name);
}

void populate_suite_proofs()
{
    if (suite_count() > 0) {
        for (auto& [suite_name, suite_functions] : utest_suites()) {
            utest_active_suite_name() = suite_name;

            for (auto& suite_function : suite_functions) {
                suite_function();
            }
        }
    }
}

void run_suite_proofs()
{
    using namespace std::literals;

    const char* suite_filter = getenv("SUITE");
    const char* proof_filter = getenv("PROOF");
    bool quiet = getenv("Q") != nullptr;

    std::regex suite_re(".*");
    std::regex proof_re(".*");
    if (suite_filter) {
        suite_re = ".*"s + suite_filter + ".*";
    }
    if (proof_filter) {
        proof_re = ".*"s + proof_filter + ".*";
    }

    for (auto& [suite_name, proofs] : utest_suite_proofs()) {
        if (!std::regex_match(suite_name, suite_re)) {
            continue;
        }
        if (!quiet) {
            std::cout << "== " << suite_name << " ==" << std::endl;
        }
        for (auto& fixture : proofs) {
            auto proof_name = fixture->utest_proof_name;

            if (!std::regex_match(proof_name, proof_re)) {
                continue;
            }
            if (!quiet) {
                std::cout << " * " << proof_name << std::endl;
            }

            auto pre_failed_proofs_size = proof_failures().size();
            current_proof = std::string(suite_name) + "::" + proof_name;

            fixture->utest_wrapper(fixture.get());
            if (pre_failed_proofs_size == proof_failures().size()) {
                register_passed_proof(suite_name, fixture->utest_proof_name);
            }
        }
    }
}

void report_result()
{
    std::cout << "Result: " << (proof_failures().empty() ?
                                "OK" : "FAILED") << std::endl;

    for (auto& failure : proof_failures()) {
        std::cerr << " - " << failure.suite_name << " @ " << failure.filename
                  << ":" << failure.line_no << "\n"
                  << "   \"" << failure.proof_name << "\": " << failure.test
                  << " (expected '" << failure.actual_str << "' to be " << failure.expected
                  << ", actual = " << failure.actual << ")" << std::endl;
    }
}

void report_exception(const std::string& msg = "")
{
    std::cout << "Result: FAILED" << std::endl;
    std::cout << " - Uncaught exception in '" + current_proof + "'";
    if (!msg.empty()) {
        std::cout << ": " << msg;
    }
    std::cout << std::endl;
}

void write_results_file()
{
    const auto res_file = getenv("RESULTS_FILE");
    if (!res_file) {
        return;
    }

    std::filesystem::path res_file_path(res_file);

    std::cout << " - Writing results to: " << res_file_path.string() << std::endl;

    std::ofstream f(res_file_path);
    f << "[\n";
    bool first = true;
    for (auto proof : passed_proofs) {
        if (!first) {
            f << ",\n";
        }
        else {
            first = false;
        }
        std::replace(proof.begin(), proof.end(), '"', '\'');
        f << "  {\n";
        f << "    \"type\": \"unittest\",\n";
        f << "    \"name\": \"" << proof << "\",\n";
        f << "    \"passed\": true\n";
        f << "  }";
    }
    for (auto& failure : proof_failures()) {
        if (!first) {
            f << ",\n";
        }
        else {
            first = false;
        }
        f << "  {\n";
        f << "    \"type\": \"unittest\",\n";
        f << "    \"name\": \"" << failure.suite_name << "::" << failure.proof_name << "\",\n";
        f << "    \"passed\": false\n";
        f << "  }";
    }
    f << "\n]\n";
}

int main(int argc, char* argv[])
{
    populate_suite_proofs();
    try {
        run_suite_proofs();
        try {
            report_result();
            write_results_file();
        }
        catch (...) {
            std::cout << std::endl << "INTERNAL FAILURE" << std::endl;
            return 1;
        }

        return proof_failures().empty() ? 0 : 1;
    }
    catch (const std::exception& ex) {
        report_exception(ex.what());
        return 1;
    }
    catch (...) {
        report_exception();
        return 1;
    }
}
