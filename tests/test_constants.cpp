#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include <hashclash/md5detail.hpp>
#include <hashclash/sha1detail.hpp>

// The tables are now inline constexpr variables; these asserts also prove they
// are usable in constant expressions.
static_assert(hashclash::md5_iv[0] == 0x67452301u);
static_assert(hashclash::md5_iv[3] == 0x10325476u);
static_assert(hashclash::md5_ac[0] == 0xd76aa478u);
static_assert(hashclash::md5_ac[63] == 0xeb86d391u);
static_assert(hashclash::md5_rc[0] == 7u);
static_assert(hashclash::md5_rc[63] == 21u);
static_assert(hashclash::md5_wt[0] == 0u);
static_assert(hashclash::md5_wt[63] == 9u);
static_assert(hashclash::sha1_iv[0] == 0x67452301u);
static_assert(hashclash::sha1_iv[4] == 0xc3d2e1f0u);
static_assert(hashclash::sha1_ac[0] == 0x5a827999u);
static_assert(hashclash::sha1_ac[3] == 0xca62c1d6u);

TEST_CASE("MD5/SHA-1 constant table sizes are unchanged") {
    CHECK(sizeof(hashclash::md5_iv) == 4 * sizeof(hashclash::uint32));
    CHECK(sizeof(hashclash::md5_ac) == 64 * sizeof(hashclash::uint32));
    CHECK(sizeof(hashclash::md5_rc) == 64 * sizeof(unsigned));
    CHECK(sizeof(hashclash::md5_wt) == 64 * sizeof(unsigned));
    CHECK(sizeof(hashclash::sha1_iv) == 5 * sizeof(hashclash::uint32));
    CHECK(sizeof(hashclash::sha1_ac) == 4 * sizeof(hashclash::uint32));
}
