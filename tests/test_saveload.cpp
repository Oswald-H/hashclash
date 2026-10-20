#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <random>
#include <vector>

#include <hashclash/differentialpath.hpp>
#include <hashclash/saveload.hpp>
#include <hashclash/saveload_bz2.hpp>
#include <hashclash/saveload_gz.hpp>
#include <hashclash/sha1differentialpath.hpp>

namespace {

class temp_file {
  public:
    temp_file() {
        static std::uint64_t counter = 0;
        path_ = std::filesystem::temp_directory_path() /
                ("hashclash_test_" + std::to_string(++counter));
    }
    ~temp_file() { std::filesystem::remove(path_); }
    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::vector<std::uint8_t> random_bytes(std::size_t size) {
    std::mt19937 rng(12345);
    std::vector<std::uint8_t> data(size);
    for (auto &byte : data)
        byte = static_cast<std::uint8_t>(rng());
    return data;
}

} // namespace

TEST_CASE("gzip round-trips highly compressible data") {
    const std::vector<std::uint32_t> input(200000, 0);
    temp_file file;

    hashclash::save_gz(input, hashclash::binary_archive, file.path());
    std::vector<std::uint32_t> output;
    hashclash::load_gz(output, hashclash::binary_archive, file.path());

    REQUIRE(output == input);
}

TEST_CASE("gzip round-trips incompressible data") {
    const auto input = random_bytes(1 << 20);
    temp_file file;

    hashclash::save_gz(input, hashclash::binary_archive, file.path());
    std::vector<std::uint8_t> output;
    hashclash::load_gz(output, hashclash::binary_archive, file.path());

    REQUIRE(output == input);
}

TEST_CASE("bz2 round-trips highly compressible data") {
    const std::vector<std::uint32_t> input(200000, 0);
    temp_file file;

    hashclash::save_bz2(input, hashclash::binary_archive, file.path());
    std::vector<std::uint32_t> output;
    hashclash::load_bz2(output, hashclash::binary_archive, file.path());

    REQUIRE(output == input);
}

TEST_CASE("bz2 round-trips incompressible data") {
    const auto input = random_bytes(1 << 20);
    temp_file file;

    hashclash::save_bz2(input, hashclash::binary_archive, file.path());
    std::vector<std::uint8_t> output;
    hashclash::load_bz2(output, hashclash::binary_archive, file.path());

    REQUIRE(output == input);
}

TEST_CASE("differential path round-trips through both archives") {
    hashclash::differentialpath input;
    input.setbitcondition(10, 5, hashclash::bc_plus);
    input.setbitcondition(11, 6, hashclash::bc_zero);
    input.setbitcondition(11, 7, hashclash::bc_one);

    for (const auto archive : {hashclash::binary_archive, hashclash::text_archive}) {
        temp_file file;
        hashclash::save(input, archive, file.path());

        hashclash::differentialpath output;
        hashclash::load(output, archive, file.path());

        REQUIRE(output == input);
    }
}

TEST_CASE("sha1 differential path round-trips through both archives") {
    hashclash::sha1differentialpath input;
    input[10].set(5, hashclash::bc_plus);
    input[11].set(6, hashclash::bc_zero);
    input[11].set(7, hashclash::bc_one);

    for (const auto archive : {hashclash::binary_archive, hashclash::text_archive}) {
        temp_file file;
        const auto &cref = input;
        hashclash::save(cref, archive, file.path());

        hashclash::sha1differentialpath output;
        hashclash::load(output, archive, file.path());

        REQUIRE(output == input);
    }
}

TEST_CASE("sha1 differential path vectors round-trip through both archives") {
    std::vector<hashclash::sha1differentialpath> input(2);
    input[0][3].set(1, hashclash::bc_plus);
    input[1][7].set(2, hashclash::bc_minus);
    input[1][8].set(3, hashclash::bc_one);

    for (const auto archive : {hashclash::binary_archive, hashclash::text_archive}) {
        temp_file file;
        const auto &cref = input;
        hashclash::save(cref, archive, file.path());

        std::vector<hashclash::sha1differentialpath> output;
        hashclash::load(output, archive, file.path());

        REQUIRE(output == input);
    }
}

TEST_CASE("differential path vectors round-trip through both archives") {
    std::vector<hashclash::differentialpath> input(2);
    input[0].setbitcondition(3, 1, hashclash::bc_plus);
    input[1].setbitcondition(6, 2, hashclash::bc_zero);
    input[1].setbitcondition(7, 3, hashclash::bc_one);

    for (const auto archive : {hashclash::binary_archive, hashclash::text_archive}) {
        temp_file file;
        const auto &cref = input;
        hashclash::save(cref, archive, file.path());

        std::vector<hashclash::differentialpath> output;
        hashclash::load(output, archive, file.path());

        REQUIRE(output == input);
    }
}

TEST_CASE("default-constructed differentialpath has an initialized offset") {
    hashclash::differentialpath path;
    CHECK(path.tbegin() == 0);
    CHECK(path.tend() == 0);
    CHECK(path.nrcond() == 0);

    path[5].clear();
    CHECK(path.tbegin() == 5);
    CHECK(path.tend() == 6);
}
