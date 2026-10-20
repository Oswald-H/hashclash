#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <tuple>

#include <hashclash/conditions.hpp>
#include <hashclash/sdr.hpp>
#include <hashclash/types.hpp>

TEST_CASE("triple compares like its member tuple") {
    using triple = hashclash::triple<int, int, int>;
    using std::tuple;

    const triple a(1, 2, 3);
    const triple b(1, 2, 3);
    const triple c(1, 2, 4);
    const triple d(1, 3, 0);
    const triple e(2, 0, 0);

    CHECK(a == b);
    CHECK_FALSE(a != b);

    const auto key = [](const triple &t) { return tuple(t.first, t.second, t.third); };
    for (const triple *l : {&a, &c, &d, &e})
        for (const triple *r : {&a, &c, &d, &e}) {
            CHECK((*l == *r) == (key(*l) == key(*r)));
            CHECK((*l != *r) == (key(*l) != key(*r)));
            CHECK((*l < *r) == (key(*l) < key(*r)));
            CHECK((*l > *r) == (key(*l) > key(*r)));
            CHECK((*l <= *r) == (key(*l) <= key(*r)));
            CHECK((*l >= *r) == (key(*l) >= key(*r)));
        }

    CHECK(a < c);
    CHECK(a < d);
    CHECK(c < d);
    CHECK(d < e);
    CHECK(a <= b);
    CHECK(a >= b);
    CHECK_FALSE(a < b);
}

TEST_CASE("sdr compares like its (mask, sign) tuple") {
    using hashclash::sdr;
    using std::tuple;

    const sdr a(0xFFFF0000u, 0x0000FFFFu);
    const sdr b(0xFFFF0000u, 0x0000FFFFu);
    const sdr c(0xFFFFFFFFu, 0x00000000u);
    const sdr d(0x00000001u, 0x00000001u);
    const sdr e(0x00FF00FFu, 0x00000001u);

    CHECK(a == b);
    CHECK_FALSE(a != b);
    CHECK(hashclash::sdr(0u) == 0);
    CHECK(0 == hashclash::sdr(0u));

    const auto key = [](const sdr &x) { return tuple(x.mask, x.sign); };
    for (const sdr *l : {&a, &c, &d, &e})
        for (const sdr *r : {&a, &c, &d, &e}) {
            CHECK((*l == *r) == (key(*l) == key(*r)));
            CHECK((*l != *r) == (key(*l) != key(*r)));
            CHECK((*l < *r) == (key(*l) < key(*r)));
            CHECK((*l > *r) == (key(*l) > key(*r)));
            CHECK((*l <= *r) == (key(*l) <= key(*r)));
            CHECK((*l >= *r) == (key(*l) >= key(*r)));
        }
}

TEST_CASE("byteconditions compares by value") {
    using hashclash::bc_one;
    using hashclash::bc_zero;
    using hashclash::byteconditions;

    const byteconditions a;
    const byteconditions b;
    const byteconditions c = byteconditions().set(0, bc_one);
    const byteconditions d = byteconditions().set(0, bc_zero);
    const byteconditions e = byteconditions().set(7, bc_one);

    CHECK(a == b);
    CHECK(a == byteconditions(0u));
    CHECK(a == 0);
    CHECK(a != c);

    for (const byteconditions *l : {&a, &c, &d, &e})
        for (const byteconditions *r : {&a, &c, &d, &e}) {
            CHECK((*l == *r) == (l->val == r->val));
            CHECK((*l != *r) == (l->val != r->val));
            CHECK((*l < *r) == (l->val < r->val));
            CHECK((*l > *r) == (l->val > r->val));
            CHECK((*l <= *r) == (l->val <= r->val));
            CHECK((*l >= *r) == (l->val >= r->val));
        }
}

TEST_CASE("wordconditions compares lexicographically over bytes") {
    using hashclash::bc_one;
    using hashclash::bc_zero;
    using hashclash::wordconditions;

    const wordconditions a;
    const wordconditions b;
    const wordconditions c = wordconditions().set(0, bc_one);
    const wordconditions d = wordconditions().set(1, bc_zero);
    const wordconditions e = wordconditions().set(8, bc_one);
    const wordconditions f = wordconditions().set(31, bc_one);

    CHECK(a == b);
    CHECK(a == wordconditions());
    CHECK(a != c);

    const auto key = [](const wordconditions &w) {
        return std::array<std::uint32_t, 4>{w.bytes[0].val, w.bytes[1].val, w.bytes[2].val,
                                            w.bytes[3].val};
    };
    for (const wordconditions *l : {&a, &c, &d, &e, &f})
        for (const wordconditions *r : {&a, &c, &d, &e, &f}) {
            CHECK((*l == *r) == (key(*l) == key(*r)));
            CHECK((*l != *r) == (key(*l) != key(*r)));
            CHECK((*l < *r) == (key(*l) < key(*r)));
            CHECK((*l > *r) == (key(*l) > key(*r)));
            CHECK((*l <= *r) == (key(*l) <= key(*r)));
            CHECK((*l >= *r) == (key(*l) >= key(*r)));
        }
}

TEST_CASE("rotates are well defined for zero and wrap around") {
    const std::uint32_t x = 0x12345678u;

    CHECK(hashclash::rotate_left(x, 0) == x);
    CHECK(hashclash::rotate_right(x, 0) == x);
    CHECK(hashclash::rotate_left(0x80000001u, 1) == 3u);
    CHECK(hashclash::rotate_right(0x3u, 1) == 0x80000001u);

    for (unsigned n = 0; n < 32; ++n) {
        CHECK(hashclash::rotate_right(hashclash::rotate_left(x, n), n) == x);
        CHECK(hashclash::rotate_left(hashclash::rotate_right(x, n), n) == x);
    }
}
