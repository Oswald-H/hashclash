#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <hashclash/conditions.hpp>
#include <hashclash/sdr.hpp>

namespace {

const std::array<hashclash::bitcondition, 15> all_bitconditions{
    hashclash::bc_constant, hashclash::bc_plus,   hashclash::bc_minus, hashclash::bc_zero,
    hashclash::bc_one,      hashclash::bc_prev,   hashclash::bc_prevn, hashclash::bc_prev2,
    hashclash::bc_prev2n,   hashclash::bc_or2,    hashclash::bc_next,  hashclash::bc_nextn,
    hashclash::bc_next2,    hashclash::bc_next2n, hashclash::bc_or2b};

int sdr_get_reference(const hashclash::sdr &s, unsigned b) {
    const std::uint32_t positive = (s.sign >> b) & 1u;
    const std::uint32_t negative = ((s.sign ^ s.mask) >> b) & 1u;
    return static_cast<int>(positive) - static_cast<int>(negative);
}

unsigned naf_weight_reference(std::uint32_t n) {
    const std::int64_t signed_value = static_cast<std::int32_t>(n);
    std::uint32_t magnitude =
        static_cast<std::uint32_t>(signed_value < 0 ? -signed_value : signed_value);
    unsigned weight = 0;
    while (magnitude != 0) {
        if (magnitude & 1u) {
            const int digit = 2 - static_cast<int>(magnitude & 3u);
            magnitude = static_cast<std::uint32_t>(static_cast<std::int64_t>(magnitude) - digit);
            ++weight;
        }
        magnitude >>= 1;
    }
    return weight;
}

std::string sdr_to_text(const hashclash::sdr &s) {
    std::ostringstream out;
    out << s;
    return out.str();
}

std::vector<hashclash::sdr> sample_sdrs() {
    using hashclash::sdr;
    std::vector<sdr> values{sdr(),
                            sdr(0u),
                            sdr(1u),
                            sdr(0x80000001u),
                            sdr(0xFFFFFFFFu),
                            sdr(0x0000000Fu, 0x000000F0u),
                            sdr(0xAAAAAAAAu, 0x55555555u),
                            sdr(0xFFFFFFFFu, 0u),
                            sdr(0u, 0xFFFFFFFFu)};
    std::mt19937 rng(0xC0FFEEu);
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t first = rng();
        const std::uint32_t second = rng();
        values.emplace_back(first, second);
    }
    return values;
}

} // namespace

TEST_CASE("bitcondition streams its documented single-character code") {
    using hashclash::bitcondition;
    const std::array<std::pair<bitcondition, const char *>, 15> cases{{
        {hashclash::bc_constant, "."},
        {hashclash::bc_plus, "+"},
        {hashclash::bc_minus, "-"},
        {hashclash::bc_zero, "0"},
        {hashclash::bc_one, "1"},
        {hashclash::bc_prev, "^"},
        {hashclash::bc_prevn, "!"},
        {hashclash::bc_prev2, "m"},
        {hashclash::bc_prev2n, "#"},
        {hashclash::bc_or2, "?"},
        {hashclash::bc_next, "V"},
        {hashclash::bc_nextn, "Y"},
        {hashclash::bc_next2, "W"},
        {hashclash::bc_next2n, "H"},
        {hashclash::bc_or2b, "Q"},
    }};

    for (const auto &[cond, text] : cases) {
        std::ostringstream out;
        out << cond;
        CHECK(out.str() == text);

        std::istringstream in(text);
        bitcondition parsed = hashclash::bc_max;
        in >> parsed;
        CHECK_FALSE(in.fail());
        CHECK(parsed == cond);
    }
}

TEST_CASE("bitcondition streaming rejects unknown codes") {
    std::istringstream in("x");
    hashclash::bitcondition parsed = hashclash::bc_constant;
    in >> parsed;
    CHECK(in.fail());

    std::ostringstream out;
    CHECK_THROWS_AS(out << hashclash::bc_max, std::logic_error);
}

TEST_CASE("sdr text format matches the CLI fixtures") {
    CHECK(sdr_to_text(hashclash::sdr()) == "[!!]");
    CHECK(sdr_to_text(hashclash::sdr(0u)) == "[!!]");
    CHECK(sdr_to_text(hashclash::sdr(1u)) == "[!0!]");
    CHECK(sdr_to_text(hashclash::sdr(0x80000001u)) == "[!0,31!]");
    CHECK(sdr_to_text(hashclash::sdr(0x00000002u, 0u)) == "[!-1!]");
    CHECK(sdr_to_text(hashclash::sdr(0x0000000Fu, 0x000000F0u)) == "[!-0,-1,-2,-3,4,5,6,7!]");
}

TEST_CASE("sdr text round-trips through streams") {
    using hashclash::sdr;

    for (const sdr &value : sample_sdrs()) {
        const std::string text = sdr_to_text(value);
        std::istringstream in(text);
        sdr parsed;
        in >> parsed;
        INFO("sdr text: " << text);
        CHECK_FALSE(in.fail());
        CHECK(parsed == value);
    }
}

TEST_CASE("sdr extraction sets failbit on malformed fields") {
    for (const char *text : {"ok p=1", "[", "[!0", "[!0,"}) {
        std::istringstream in(text);
        hashclash::sdr parsed(0x12345678u);
        in >> parsed;
        INFO("malformed sdr field: " << text);
        CHECK(in.fail());
    }
}

TEST_CASE("sdr accessors agree with a bitwise reference model") {
    using hashclash::sdr;

    for (const sdr &value : sample_sdrs()) {
        std::uint32_t adddiff_ref = 0;
        std::uint32_t xordiff_ref = 0;
        std::uint32_t set0_ref = 0;
        std::uint32_t set1_ref = 0;
        unsigned hw_ref = 0;

        for (unsigned b = 0; b < 32; ++b) {
            const int digit = sdr_get_reference(value, b);
            CHECK(value.get(b) == digit);
            CHECK(value[b] == digit);

            if (digit == 1)
                adddiff_ref += std::uint32_t(1) << b;
            else if (digit == -1)
                adddiff_ref -= std::uint32_t(1) << b;

            if (digit != 0) {
                xordiff_ref |= std::uint32_t(1) << b;
                ++hw_ref;
            }
            if (digit != 1)
                set0_ref |= std::uint32_t(1) << b;
            if (digit == -1)
                set1_ref |= std::uint32_t(1) << b;
        }

        CHECK(value.adddiff() == adddiff_ref);
        CHECK(value.xordiff() == xordiff_ref);
        CHECK(value.set0conditions() == set0_ref);
        CHECK(value.set1conditions() == set1_ref);
        CHECK(value.hw() == hw_ref);
        CHECK(value.hwnaf() == naf_weight_reference(value.adddiff()));
    }
}

TEST_CASE("byteconditions set and get round-trip every bit position") {
    using hashclash::bitcondition;

    for (unsigned b = 0; b < 8; ++b) {
        for (bitcondition cond : all_bitconditions) {
            hashclash::byteconditions bc;
            bc.set(b, cond);
            CHECK(bc.get(b) == cond);
            CHECK(bc[b] == cond);
            for (unsigned other = 0; other < 8; ++other)
                if (other != b)
                    CHECK(bc.get(other) == hashclash::bc_constant);
        }
    }
}

TEST_CASE("byteconditions masks follow the individual conditions") {
    using hashclash::bitcondition;

    hashclash::byteconditions bc;
    bc.set(0, hashclash::bc_plus);
    bc.set(1, hashclash::bc_minus);
    bc.set(2, hashclash::bc_zero);
    bc.set(3, hashclash::bc_one);
    bc.set(5, hashclash::bc_prev);
    bc.set(7, hashclash::bc_next2);

    std::uint32_t mask_ref = 0;
    std::uint32_t plus_ref = 0;
    std::uint32_t minus_ref = 0;
    std::uint32_t zero_ref = 0;
    std::uint32_t one_ref = 0;
    unsigned hw_ref = 0;
    for (unsigned b = 0; b < 8; ++b) {
        const bitcondition cond = bc.get(b);
        if (cond != hashclash::bc_constant) {
            mask_ref |= std::uint32_t(1) << b;
            ++hw_ref;
        }
        if (cond == hashclash::bc_plus)
            plus_ref |= std::uint32_t(1) << b;
        if (cond == hashclash::bc_minus)
            minus_ref |= std::uint32_t(1) << b;
        if (cond == hashclash::bc_zero)
            zero_ref |= std::uint32_t(1) << b;
        if (cond == hashclash::bc_one)
            one_ref |= std::uint32_t(1) << b;
    }

    CHECK(bc.mask() == mask_ref);
    CHECK(bc.hw() == hw_ref);
    CHECK(bc.set0() == ~(zero_ref | plus_ref));
    CHECK(bc.set1() == (one_ref | minus_ref));
    CHECK(bc.diff() == plus_ref - minus_ref);

    for (int c = 0; c < hashclash::bc_max; ++c) {
        std::uint32_t expected = 0;
        for (unsigned b = 0; b < 8; ++b)
            if (bc.get(b) == static_cast<bitcondition>(c))
                expected |= std::uint32_t(1) << b;
        CHECK(bc.mask(static_cast<bitcondition>(c)) == expected);
    }
}

TEST_CASE("byteconditions text round-trips through streams") {
    hashclash::byteconditions bc;
    bc.set(0, hashclash::bc_plus);
    bc.set(1, hashclash::bc_minus);
    bc.set(2, hashclash::bc_zero);
    bc.set(3, hashclash::bc_one);

    std::ostringstream out;
    out << bc;
    CHECK(out.str() == "....10-+");

    std::istringstream in(out.str());
    hashclash::byteconditions parsed;
    in >> parsed;
    CHECK_FALSE(in.fail());
    CHECK(parsed == bc);

    for (unsigned b = 0; b < 8; ++b)
        for (hashclash::bitcondition cond : all_bitconditions) {
            hashclash::byteconditions value;
            value.set(b, cond);
            std::ostringstream text;
            text << value;
            std::istringstream input(text.str());
            hashclash::byteconditions roundtrip;
            input >> roundtrip;
            CHECK_FALSE(input.fail());
            CHECK(roundtrip == value);
        }
}

TEST_CASE("wordconditions text round-trips a mixed condition pattern") {
    hashclash::wordconditions w;
    w.set(0, hashclash::bc_plus);
    w.set(1, hashclash::bc_minus);
    w.set(2, hashclash::bc_zero);
    w.set(3, hashclash::bc_one);
    w.set(4, hashclash::bc_constant);
    w.set(7, hashclash::bc_plus);
    w.set(8, hashclash::bc_minus);
    w.set(15, hashclash::bc_zero);
    w.set(16, hashclash::bc_one);
    w.set(23, hashclash::bc_plus);
    w.set(24, hashclash::bc_constant);
    w.set(31, hashclash::bc_minus);

    std::ostringstream out;
    out << w;
    CHECK(out.str() == "|-....... +......1 0......- +...10-+|");

    // operator<< separates bytes with single spaces and operator>> consumes them without
    // whitespace-skipping, so the default stream settings must round-trip.
    std::istringstream in(out.str());
    hashclash::wordconditions parsed;
    in >> parsed;
    CHECK_FALSE(in.fail());
    CHECK(parsed == w);
    for (unsigned b = 0; b < 32; ++b)
        CHECK(parsed.get(b) == w.get(b));

    // A missing separator is rejected instead of silently shifting bytes; extra whitespace
    // between bytes remains accepted (the byte extraction skips it).
    std::istringstream missing_separator("|-.......+......1 0......- +...10-+|");
    hashclash::wordconditions rejected2;
    missing_separator >> rejected2;
    CHECK(missing_separator.fail());
}

TEST_CASE("wordconditions getsdr matches set1 and diff") {
    using hashclash::sdr;
    using hashclash::wordconditions;

    for (const sdr &value : sample_sdrs()) {
        const wordconditions w(value);
        const std::uint32_t set1 = w.set1();
        CHECK(w.getsdr() == sdr(set1, set1 + w.diff()));
        CHECK(w.getsdr() == value);
        CHECK(w.diff() == value.adddiff());
    }

    wordconditions mixed;
    mixed.set(0, hashclash::bc_plus);
    mixed.set(1, hashclash::bc_minus);
    mixed.set(2, hashclash::bc_zero);
    mixed.set(3, hashclash::bc_one);
    const std::uint32_t set1 = mixed.set1();
    CHECK(mixed.getsdr() == sdr(set1, set1 + mixed.diff()));
}

TEST_CASE("wordconditions equality and ordering basics") {
    const hashclash::wordconditions empty;
    const hashclash::wordconditions other_empty;
    const hashclash::wordconditions one = hashclash::wordconditions().set(0, hashclash::bc_one);

    CHECK(empty == other_empty);
    CHECK_FALSE(empty != other_empty);
    CHECK(empty != one);
    CHECK(empty < one);
}
