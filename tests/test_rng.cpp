#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <hashclash/rng.hpp>

TEST_CASE("localxrng is deterministic for equal seeds") {
    hashclash::localxrng first(0x123456789abcdef0ull);
    hashclash::localxrng second(0x123456789abcdef0ull);

    for (unsigned i = 0; i < 256; ++i)
        CHECK(first() == second());
}

TEST_CASE("localxrng produces different streams for different seeds") {
    hashclash::localxrng first(1);
    hashclash::localxrng second(2);

    bool differs = false;
    for (unsigned i = 0; i < 64 && !differs; ++i)
        differs = first() != second();

    CHECK(differs);
}

TEST_CASE("xrng128_locked is callable from multiple threads") {
    std::atomic<unsigned> samples{0};
    std::vector<std::jthread> threads;
    for (unsigned t = 0; t < 4; ++t)
        threads.emplace_back([&samples] {
            for (unsigned i = 0; i < 512; ++i) {
                static_cast<void>(hashclash::xrng128_locked());
                samples.fetch_add(1, std::memory_order_relaxed);
            }
        });

    threads.clear(); // jthread destructors join

    CHECK(samples == 4 * 512);
}
