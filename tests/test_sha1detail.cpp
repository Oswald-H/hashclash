#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <hashclash/sha1detail.hpp>
#include <hashclash/types.hpp>

namespace {

std::string sha1_hex(std::string_view message) {
    using hashclash::uint32;

    std::array<uint32, 5> ihv = {hashclash::sha1_iv[0], hashclash::sha1_iv[1],
                                 hashclash::sha1_iv[2], hashclash::sha1_iv[3],
                                 hashclash::sha1_iv[4]};

    std::string padded(message);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(padded.size()) * 8;
    padded.push_back('\x80');
    while (padded.size() % 64 != 56)
        padded.push_back('\0');
    for (int i = 7; i >= 0; --i)
        padded.push_back(static_cast<char>((bit_length >> (8 * i)) & 0xFF));

    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::array<uint32, 16> block{};
        for (std::size_t i = 0; i < block.size(); ++i) {
            const auto *bytes =
                reinterpret_cast<const unsigned char *>(padded.data() + offset + 4 * i);
            block[i] = static_cast<uint32>(bytes[0]) | (static_cast<uint32>(bytes[1]) << 8) |
                       (static_cast<uint32>(bytes[2]) << 16) |
                       (static_cast<uint32>(bytes[3]) << 24);
        }
        hashclash::sha1compress(ihv.data(), block.data());
    }

    constexpr char hex[] = "0123456789abcdef";
    std::string digest;
    digest.reserve(40);
    for (uint32 word : ihv)
        for (int i = 3; i >= 0; --i) {
            const unsigned byte = (word >> (8 * i)) & 0xFF;
            digest.push_back(hex[byte >> 4]);
            digest.push_back(hex[byte & 0xF]);
        }
    return digest;
}

struct sha1_vector {
    std::string_view message;
    std::string_view digest;
};

const std::array<hashclash::uint32, 5> sha1_iv_copy = {hashclash::sha1_iv[0], hashclash::sha1_iv[1],
                                                       hashclash::sha1_iv[2], hashclash::sha1_iv[3],
                                                       hashclash::sha1_iv[4]};

const std::array<hashclash::uint32, 16> abc_padded_block = {
    0x80636261u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0x18000000u};

} // namespace

TEST_CASE("sha1compress reproduces the FIPS 180 test vectors") {
    const std::array<sha1_vector, 4> vectors = {{
        {"", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
        {"abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
        {"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopq"
         "rlmnopqrsmnopqrstnopqrstu",
         "a49b2446a02c645bf419f995b67091253a04a259"},
    }};

    for (const sha1_vector &vector : vectors)
        CHECK(sha1_hex(vector.message) == vector.digest);

    const std::string multi_block(80, 'a');
    CHECK(sha1_hex(multi_block) == "86f33652fcffd7fa1443e246dd34fe5d00e25ffd");
}

TEST_CASE("sha1compress matches a directly constructed single padded block") {
    using hashclash::uint32;

    std::array<uint32, 5> ihv = sha1_iv_copy;

    hashclash::sha1compress(ihv.data(), abc_padded_block.data());

    CHECK(ihv[0] == 0xa9993e36u);
    CHECK(ihv[1] == 0x4706816au);
    CHECK(ihv[2] == 0xba3e2571u);
    CHECK(ihv[3] == 0x7850c26cu);
    CHECK(ihv[4] == 0x9cd0d89du);
}

TEST_CASE("sha1compress_me agrees with sha1compress after message expansion") {
    using hashclash::uint32;

    std::array<uint32, 80> me{};
    hashclash::sha1_me(me.data(), abc_padded_block.data());

    std::array<uint32, 5> direct = sha1_iv_copy;
    std::array<uint32, 5> expanded = sha1_iv_copy;
    hashclash::sha1compress(direct.data(), abc_padded_block.data());
    hashclash::sha1compress_me(expanded.data(), me.data());

    CHECK(expanded == direct);
    CHECK(me[0] == 0x61626380u);
    CHECK(me[15] == 0x00000018u);
}
