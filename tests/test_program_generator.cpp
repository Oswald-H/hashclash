#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <hashclash/conditions.hpp>
#include <hashclash/sdr.hpp>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/sha1messagespace.hpp>

#include "main.hpp"

using namespace hashclash;

// The generator's translation units expect the globals that
// collisionfinding.cpp normally provides; the test provides them directly.
sha1differentialpath maindiffpath;
sha1messagespace mainmespace;
parameters_type mainparameters;

namespace {

// Switches the process to a scratch directory for the duration of the scope so
// that generate_program()'s collfind.cpp lands there.
struct scratch_cwd {
    std::filesystem::path original;
    std::filesystem::path scratch;

    scratch_cwd()
        : original(std::filesystem::current_path()),
          scratch(std::filesystem::temp_directory_path() / "hashclash_test_program_generator") {
        std::filesystem::remove_all(scratch);
        std::filesystem::create_directories(scratch);
        std::filesystem::current_path(scratch);
    }

    ~scratch_cwd() {
        std::error_code ec;
        std::filesystem::current_path(original, ec);
        std::filesystem::remove_all(scratch, ec);
    }
};

std::size_t count_occurrences(const std::string &haystack, const std::string &needle) {
    std::size_t count = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string::npos;
         pos = haystack.find(needle, pos + needle.size()))
        ++count;
    return count;
}

// Fills maindiffpath/mainmespace with a synthetic but structurally valid path
// and message space, then runs the real emission path and returns the program
// the generator wrote.
std::string emitted_collision_finder() {
    uint32 msg[16] = {0};
    msg[0] = 1u; // bit 0 of m[0] differs
    uint32 mex[80];
    sha1_me_generalised(mex, msg, 0);

    sha1differentialpath path;
    for (int t = 0; t < 80; ++t) {
        path.getme(t).mask = mex[t];
        path.getme(t).sign = mex[t];
    }
    for (int t = -4; t < 80; ++t)
        path[t].clear();
    for (int t = 0; t < 80; ++t) {
        path.setbitcondition(t, 31, bc_one);
        if (t >= 1)
            path.setbitcondition(t, 30, bc_prev);
        if (t >= 2)
            path.setbitcondition(t, 29, bc_prev2);
        path.setbitcondition(t, 0, bc_plus);
        path.setbitcondition(t, 1, bc_minus);
    }
    maindiffpath = path;

    mainmespace.clear();
    for (unsigned t = 0; t < 16; ++t)
        for (unsigned b = 0; b < 32; ++b)
            mainmespace.buildbasis_addfreebit(t, b);

    scratch_cwd guard;
    const bool written = generate_program();
    std::ifstream in("collfind.cpp");
    std::ostringstream buffer;
    buffer << in.rdbuf();
    REQUIRE(written);
    return buffer.str();
}

} // namespace

TEST_CASE("the generated collision finder reports consistency errors without exiting") {
    const std::string emitted = emitted_collision_finder();
    REQUIRE(!emitted.empty());

    // An internal consistency failure must terminate the generated program
    // through the exception path, never through a process-terminating exit()
    // call (which reported success for most of the checks).
    CHECK(emitted.find("exit(") == std::string::npos);

    // The two verify_block consistency guards and the compute_qt out-of-range
    // guard throw the tagged error type, and the generated main turns it into a
    // nonzero exit status.
    CHECK(emitted.find("struct near_collision_error {};") != std::string::npos);
    CHECK(count_occurrences(emitted, "throw near_collision_error{};") == 3);
    CHECK(emitted.find("catch (const near_collision_error&) { return 1; }") != std::string::npos);

    // No bare `throw;` may be emitted: the generated program has no catch
    // handler that rethrows, so a bare `throw;` would call std::terminate when
    // one of the internal consistency guards fires. The loop_data::add guards
    // throw the same std::logic_error the checked-in collfind.cpp throws.
    CHECK(emitted.find("throw;") == std::string::npos);
    CHECK(count_occurrences(emitted,
                            "throw std::logic_error(\"collfind.cpp: loop_data::add: success "
                            "percentage out of range: \" + std::to_string(proc));") == 2);
}

TEST_CASE("generate_program reports an inconsistent message difference as an exception") {
    sha1differentialpath saved = maindiffpath;

    // Every expanded message word is zero except m[16], which violates the SHA-1 recurrence
    // m[t] == rotate_left(m[t-3] ^ m[t-8] ^ m[t-14] ^ m[t-16], 1) for t >= 16.
    sha1differentialpath path;
    for (int t = 0; t < 80; ++t) {
        path.getme(t).mask = 0;
        path.getme(t).sign = 0;
    }
    path.getme(16).mask = 1;
    maindiffpath = path;

    bool threw = false;
    std::string message;
    try {
        generate_program();
    } catch (const std::logic_error &e) {
        threw = true;
        message = e.what();
    }

    maindiffpath = saved;

    CHECK(threw);
    CHECK(message.find("mxordiff inconsistent") != std::string::npos);
    CHECK(message.find("t=16") != std::string::npos);
}
