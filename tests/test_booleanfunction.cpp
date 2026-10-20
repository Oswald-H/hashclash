#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include <hashclash/booleanfunction.hpp>
#include <hashclash/md5detail.hpp>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1differentialpath.hpp>

TEST_CASE("booleanfunction wraps stateless functions") {
    const std::uint32_t a = 0x67452301u, b = 0xefcdab89u, c = 0x98badcfeu;

    CHECK(hashclash::MD5_F_data.description() == "MD5_FF");
    CHECK(hashclash::MD5_F_data.F(a, b, c) == hashclash::md5_ff(a, b, c));
    CHECK(hashclash::MD5_G_data.F(a, b, c) == hashclash::md5_gg(a, b, c));
    CHECK(hashclash::MD5_H_data.F(a, b, c) == hashclash::md5_hh(a, b, c));
    CHECK(hashclash::MD5_I_data.F(a, b, c) == hashclash::md5_ii(a, b, c));
}

TEST_CASE("booleanfunction computes lookup tables lazily") {
    const hashclash::bf_conditions all_zero{hashclash::bc_zero, hashclash::bc_zero,
                                            hashclash::bc_zero};

    const auto &outcome = hashclash::MD5_F_data.outcome(all_zero);
    REQUIRE(outcome.size() == 1);
    CHECK(outcome[0] == hashclash::bc_constant);

    const auto &forward = hashclash::MD5_F_data.forwardconditions(all_zero, hashclash::bc_constant);
    CHECK(forward.first == hashclash::bc_zero);
    CHECK(forward.second == hashclash::bc_zero);
    CHECK(forward.third == hashclash::bc_zero);
}

TEST_CASE("booleanfunction can be constructed locally") {
    const std::uint32_t a = 0x01234567u, b = 0x89abcdefu, c = 0xfedcba98u;

    hashclash::booleanfunction local(hashclash::sha1_f1, "local");
    CHECK(local.description() == "local");
    CHECK(local.F(a, b, c) == hashclash::sha1_f1(a, b, c));
    CHECK(local.F(a, b, c) == hashclash::SHA1_F1_data.F(a, b, c));
}

TEST_CASE("bf_outcome bit values use unsigned arithmetic") {
    const hashclash::bf_outcome plus(hashclash::bf_outcome::fplus);
    const hashclash::bf_outcome minus(hashclash::bf_outcome::fminus);
    const hashclash::bf_outcome constant(hashclash::bf_outcome::fconstant);

    CHECK(plus(0, 31) == 0x80000000u);
    CHECK(minus(0, 31) == 0x80000000u);
    CHECK(minus(0, 0) == 0xFFFFFFFFu);
    CHECK(minus(2, 7) == 0xFFFFFF80u);
    CHECK(plus(1, 5) == 0xFFFFFFE0u);
    const hashclash::bf_outcome plus_constant(hashclash::bf_outcome::fplus |
                                              hashclash::bf_outcome::fconstant);
    CHECK(plus_constant(1, 5) == 0x00000020u);
    CHECK(constant(0, 31) == 0u);
    CHECK(constant(1, 31) == 0x80000000u);
}
