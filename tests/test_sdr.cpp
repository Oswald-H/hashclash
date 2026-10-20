#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include <hashclash/sdr.hpp>

namespace {

// Independent reference: weight of the non-adjacent form.
unsigned naf_weight_reference(std::uint32_t n) {
    unsigned weight = 0;
    while (n != 0) {
        if (n & 1) {
            const int digit = 2 - static_cast<int>(n & 3);
            n = static_cast<std::uint32_t>(static_cast<std::int64_t>(n) - digit);
            ++weight;
        }
        n >>= 1;
    }
    return weight;
}

} // namespace

TEST_CASE("hw counts set bits") {
    CHECK(hashclash::hw(0x00000000u) == 0);
    CHECK(hashclash::hw(0x00000001u) == 1);
    CHECK(hashclash::hw(0x80000000u) == 1);
    CHECK(hashclash::hw(0xAAAAAAAAu) == 16);
    CHECK(hashclash::hw(0xFFFFFFFFu) == 32);
    CHECK(hashclash::hw(0x0F0F0F0Fu) == 16);
    CHECK(hashclash::hw(0x0000FFFFu) == 16);
    CHECK(hashclash::hw(0x00010001u) == 2);
}

TEST_CASE("hw is consistent for sdr values") {
    CHECK(hashclash::hw(hashclash::sdr(0x00000000u)) == 0);
    CHECK(hashclash::hw(hashclash::sdr(0xFFFFFFFFu)) == 32);
    CHECK(hashclash::sdr(0x0000FFFFu).hw() == 16);
}

TEST_CASE("hwnaf matches a reference non-adjacent form") {
    CHECK(hashclash::hwnaf(0x00000000u) == 0);
    CHECK(hashclash::hwnaf(0x00000001u) == 1);
    CHECK(hashclash::hwnaf(0x00000003u) == 2); // 3 = 4 - 1

    const std::vector<std::uint32_t> values{0x2u,        0x7u,        0xFu,        0x10u,
                                            0xFFu,       0xFFFFu,     0x80000000u, 0xFFFFFFFFu,
                                            0x12345678u, 0xAAAAAAAAu, 0x55555555u};
    for (const auto value : values)
        CHECK(hashclash::hwnaf(value) == naf_weight_reference(value));
}
