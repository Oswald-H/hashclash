#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <hashclash/md5detail.hpp>
#include <hashclash/types.hpp>

namespace {

std::string md5_hex(std::string_view message) {
    using hashclash::uint32;

    std::array<uint32, 4> ihv = {hashclash::md5_iv[0], hashclash::md5_iv[1], hashclash::md5_iv[2],
                                 hashclash::md5_iv[3]};

    std::string padded(message);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(padded.size()) * 8;
    padded.push_back('\x80');
    while (padded.size() % 64 != 56)
        padded.push_back('\0');
    for (unsigned i = 0; i < 8; ++i)
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
        hashclash::md5compress(ihv.data(), block.data());
    }

    constexpr char hex[] = "0123456789abcdef";
    std::string digest;
    digest.reserve(32);
    for (uint32 word : ihv)
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned byte = (word >> (8 * i)) & 0xFF;
            digest.push_back(hex[byte >> 4]);
            digest.push_back(hex[byte & 0xF]);
        }
    return digest;
}

struct md5_vector {
    std::string_view message;
    std::string_view digest;
};

const std::array<hashclash::uint32, 4> md5_iv_copy = {hashclash::md5_iv[0], hashclash::md5_iv[1],
                                                      hashclash::md5_iv[2], hashclash::md5_iv[3]};

} // namespace

TEST_CASE("md5compress reproduces the RFC 1321 test vectors") {
    const std::array<md5_vector, 7> vectors = {{
        {"", "d41d8cd98f00b204e9800998ecf8427e"},
        {"a", "0cc175b9c0f1b6a831c399e269772661"},
        {"abc", "900150983cd24fb0d6963f7d28e17f72"},
        {"message digest", "f96b697d7cb7938d525a2f31aaf161d0"},
        {"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"},
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
         "d174ab98d277d9f5a5611c2c9f419d9f"},
        {"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
         "57edf4a22be3c955ac49da2e2107b67a"},
    }};

    for (const md5_vector &vector : vectors)
        CHECK(md5_hex(vector.message) == vector.digest);

    const std::string multi_block(80, 'a');
    CHECK(md5_hex(multi_block) == "b15af9cdabbaea0516866a33d8fd0f98");
}

TEST_CASE("md5compress matches a directly constructed single padded block") {
    using hashclash::uint32;

    const std::array<uint32, 16> block = {0x80636261u, 0u, 0u, 0u, 0u, 0u, 0u,    0u,
                                          0u,          0u, 0u, 0u, 0u, 0u, 0x18u, 0u};
    std::array<uint32, 4> ihv = md5_iv_copy;

    hashclash::md5compress(ihv.data(), block.data());

    CHECK(ihv[0] == 0x98500190u);
    CHECK(ihv[1] == 0xb04fd23cu);
    CHECK(ihv[2] == 0x7d3f96d6u);
    CHECK(ihv[3] == 0x727fe128u);
}

TEST_CASE("md5compress updates the state in place over raw 64-byte blocks") {
    using hashclash::uint32;

    std::array<uint32, 4> ihv = md5_iv_copy;
    std::array<uint32, 16> block{};

    block.fill(0x61616161u);
    hashclash::md5compress(ihv.data(), block.data());

    CHECK(ihv[0] == 0x89d4ff56u);
    CHECK(ihv[1] == 0x125cd962u);
    CHECK(ihv[2] == 0x69cade33u);
    CHECK(ihv[3] == 0x0033e325u);

    block.fill(0u);
    block[0] = 0x61616161u;
    block[1] = 0x61616161u;
    block[2] = 0x61616161u;
    block[3] = 0x61616161u;
    block[4] = 0x00000080u;
    block[14] = 0x00000280u;
    hashclash::md5compress(ihv.data(), block.data());

    CHECK(ihv[0] == 0xcdf95ab1u);
    CHECK(ihv[1] == 0x05eabaabu);
    CHECK(ihv[2] == 0x336a8616u);
    CHECK(ihv[3] == 0x980ffdd8u);
}
