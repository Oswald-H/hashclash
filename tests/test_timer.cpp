#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include <hashclash/timer.hpp>

TEST_CASE("timer is not running before start") {
    hashclash::timer t;
    CHECK_FALSE(t.isrunning());

    t.start();
    CHECK(t.isrunning());

    t.stop();
    CHECK_FALSE(t.isrunning());
}

TEST_CASE("direct-started timer reports non-negative elapsed time") {
    hashclash::timer t(true);
    CHECK(t.isrunning());
    CHECK(t.time() >= 0.0);
}

TEST_CASE("timer measures a sleep within generous bounds") {
    hashclash::timer t(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    t.stop();

    const double elapsed = t.time();
    CHECK(elapsed >= 0.005);
    CHECK(elapsed <= 5.0);
}

TEST_CASE("running timer samples are non-decreasing") {
    hashclash::timer t(true);
    const double first = t.time();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const double second = t.time();
    CHECK(second >= first);
}
