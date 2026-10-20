#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <hashclash/workload.hpp>

namespace {

struct my_test_exception {};

} // namespace

TEST_CASE("run_workload invokes the callable once per index and waits for all jobs") {
    constexpr std::size_t jobs = 4;
    std::vector<int> seen(jobs, -1);
    std::atomic<int> invocations{0};
    std::atomic<int> finished{0};
    std::atomic<bool> jobs_argument_ok{true};

    hashclash::run_workload(jobs, [&](std::size_t index, std::size_t n) {
        seen[index] = static_cast<int>(index);
        if (n != jobs)
            jobs_argument_ok = false;
        ++invocations;
        ++finished;
    });

    std::sort(seen.begin(), seen.end());
    CHECK(seen == std::vector<int>{0, 1, 2, 3});
    CHECK(invocations.load() == static_cast<int>(jobs));
    CHECK(finished.load() == static_cast<int>(jobs));
    CHECK(jobs_argument_ok.load());
}

TEST_CASE("run_workload with zero jobs runs nothing and returns") {
    std::atomic<int> invocations{0};

    hashclash::run_workload(0, [&](std::size_t, std::size_t) { ++invocations; });

    CHECK(invocations.load() == 0);
}

TEST_CASE("run_workload gives every thread its own copy of an rvalue callable") {
    constexpr std::size_t jobs = 4;

    struct worker {
        std::vector<int> payload{1, 2, 3};
        std::vector<std::size_t> *observed;
        void operator()(std::size_t index, std::size_t) { (*observed)[index] = payload.size(); }
    };

    std::vector<std::size_t> observed(jobs, 0);
    hashclash::run_workload(jobs, worker{{1, 2, 3}, &observed});

    CHECK(observed == std::vector<std::size_t>(jobs, 3));
}

TEST_CASE("run_workload gives every thread its own copy of an lvalue callable") {
    constexpr std::size_t jobs = 4;

    struct worker {
        std::vector<int> payload{1, 2, 3};
        std::vector<std::size_t> *observed;
        void operator()(std::size_t index, std::size_t) { (*observed)[index] = payload.size(); }
    };

    std::vector<std::size_t> observed(jobs, 0);
    worker callable{{1, 2, 3}, &observed};
    hashclash::run_workload(jobs, callable);

    CHECK(observed == std::vector<std::size_t>(jobs, 3));
    CHECK(callable.payload == std::vector<int>{1, 2, 3});
}

TEST_CASE("run_workload lets each thread mutate its own copy of the callable") {
    struct worker {
        int count = 0;
        std::vector<int> *observed;
        void operator()(std::size_t index, std::size_t) {
            ++count;
            ++count;
            (*observed)[index] = count;
        }
    };

    std::vector<int> observed(3, 0);
    worker callable{0, &observed};
    hashclash::run_workload(observed.size(), callable);

    CHECK(observed == std::vector<int>{2, 2, 2});
}

TEST_CASE("guard_exception catches the expected exception and sets the flag") {
    std::atomic<bool> flag{false};
    auto guarded = hashclash::guard_exception<my_test_exception>(
        [](std::size_t, std::size_t) { throw my_test_exception{}; }, flag);

    REQUIRE_NOTHROW(guarded(0, 1));
    CHECK(flag.load());
}

TEST_CASE("guard_exception lets a different exception type propagate") {
    std::atomic<bool> flag{false};
    auto guarded = hashclash::guard_exception<std::runtime_error>(
        [](std::size_t, std::size_t) { throw std::logic_error("unexpected"); }, flag);

    REQUIRE_THROWS_AS(guarded(0, 1), std::logic_error);
    CHECK_FALSE(flag.load());
}

TEST_CASE("guard_exception leaves the flag unchanged when the worker succeeds") {
    std::atomic<bool> flag{false};
    std::size_t observed_index = 0;
    std::size_t observed_jobs = 0;
    auto guarded = hashclash::guard_exception<my_test_exception>(
        [&](std::size_t index, std::size_t jobs) {
            observed_index = index;
            observed_jobs = jobs;
        },
        flag);

    REQUIRE_NOTHROW(guarded(2, 5));
    CHECK(observed_index == 2);
    CHECK(observed_jobs == 5);
    CHECK_FALSE(flag.load());
}

TEST_CASE("guard_exception reports a worker exception across the thread boundary") {
    std::atomic<bool> flag{false};
    std::atomic<int> completed{0};

    hashclash::run_workload(4, hashclash::guard_exception<my_test_exception>(
                                   [&](std::size_t index, std::size_t) {
                                       if (index == 2)
                                           throw my_test_exception{};
                                       ++completed;
                                   },
                                   flag));

    CHECK(flag.load());
    CHECK(completed.load() == 3);
}
