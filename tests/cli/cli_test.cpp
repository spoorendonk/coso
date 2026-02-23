#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

/// Run a command and capture stdout + stderr, returning exit code and output.
struct CmdResult {
    int exit_code;
    std::string output;
};

CmdResult run_cmd(const std::string& cmd)
{
    std::string full_cmd = cmd + " 2>&1";
    FILE* raw = popen(full_cmd.c_str(), "r");
    REQUIRE(raw);

    std::string output;
    std::array<char, 256> buf;
    while (fgets(buf.data(), static_cast<int>(buf.size()), raw) != nullptr) {
        output += buf.data();
    }

    int status = pclose(raw);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return {exit_code, output};
}

// Path to the coso-solve binary (set by CMake or found relative to test binary).
std::string coso_solve_path()
{
    // The binary is built alongside the test in the build directory.
    // CMake sets COSO_SOLVE_PATH environment variable for convenience.
    if (const char* p = std::getenv("COSO_SOLVE_PATH")) {
        return p;
    }
    // Fallback: assume it's in the same build tree.
    return "./coso-solve";
}

} // namespace

TEST_CASE("CLI: --help prints usage", "[cli]")
{
    auto r = run_cmd(coso_solve_path() + " --help");
    CHECK(r.exit_code == 0);
    CHECK(r.output.find("Usage:") != std::string::npos);
    CHECK(r.output.find("--time-limit") != std::string::npos);
    CHECK(r.output.find("--verbose") != std::string::npos);
}

TEST_CASE("CLI: no arguments prints error", "[cli]")
{
    auto r = run_cmd(coso_solve_path());
    CHECK(r.exit_code == 1);
    CHECK(r.output.find("Error:") != std::string::npos);
}

TEST_CASE("CLI: unknown option prints error", "[cli]")
{
    auto r = run_cmd(coso_solve_path() + " --bogus");
    CHECK(r.exit_code == 1);
    CHECK(r.output.find("Error:") != std::string::npos);
}

TEST_CASE("CLI: missing file produces error output", "[cli]")
{
    auto r = run_cmd(coso_solve_path() + " nonexistent_file.vrp --time-limit 1");
    // The solve function returns an empty (infeasible) result for missing files,
    // so the CLI should still run but report infeasible.
    CHECK(r.output.find("Feasible: no") != std::string::npos);
}

TEST_CASE("CLI: invalid time limit", "[cli]")
{
    auto r = run_cmd(coso_solve_path() + " foo.vrp --time-limit abc");
    CHECK(r.exit_code == 1);
    CHECK(r.output.find("Error:") != std::string::npos);
}

TEST_CASE("CLI: negative time limit", "[cli]")
{
    auto r = run_cmd(coso_solve_path() + " foo.vrp --time-limit -5");
    CHECK(r.exit_code == 1);
    CHECK(r.output.find("Error:") != std::string::npos);
}
