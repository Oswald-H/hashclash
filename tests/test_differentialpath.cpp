#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

#include <hashclash/conditions.hpp>
#include <hashclash/differentialpath.hpp>

using hashclash::bc_minus;
using hashclash::bc_plus;

TEST_CASE("md5 differential path default paths are equal") {
    const hashclash::differentialpath a;
    const hashclash::differentialpath b;
    CHECK(a == b);
    CHECK_FALSE(a != b);
    CHECK_FALSE(a < b);
    CHECK_FALSE(b < a);
}

TEST_CASE("md5 differential path equality ignores missing trivial steps") {
    const hashclash::differentialpath empty;
    hashclash::differentialpath trivial;
    trivial[10];
    CHECK(empty == trivial);
    CHECK(trivial == empty);
    CHECK_FALSE(empty < trivial);
    CHECK_FALSE(trivial < empty);
}

TEST_CASE("md5 differential path comparison handles non-overlapping ranges") {
    hashclash::differentialpath low, high;
    low.setbitcondition(0, 0, bc_minus);
    high.setbitcondition(10, 0, bc_plus);

    CHECK_FALSE(low == high);
    CHECK_FALSE(high == low);
    CHECK(low != high);

    const bool lowless = low < high;
    const bool highless = high < low;
    CHECK(lowless != highless);
}

TEST_CASE("md5 differential path comparison handles non-overlapping empty range") {
    const hashclash::differentialpath empty;
    hashclash::differentialpath only;
    only.setbitcondition(10, 0, bc_plus);

    CHECK(empty != only);
    CHECK(only != empty);
    CHECK((empty < only) != (only < empty));
}

TEST_CASE("md5 differential path comparison is consistent with equality") {
    hashclash::differentialpath a, b;
    a.setbitcondition(5, 1, bc_plus);
    b.setbitcondition(5, 1, bc_plus);
    a.setbitcondition(12, 2, bc_minus);
    CHECK(a != b);
    CHECK((a < b) != (b < a));

    b.setbitcondition(12, 2, bc_minus);
    CHECK(a == b);
    CHECK_FALSE(a < b);
    CHECK_FALSE(b < a);
}

TEST_CASE("md5 differential paths are usable as ordered set keys") {
    std::set<hashclash::differentialpath> paths;
    hashclash::differentialpath a;
    a.setbitcondition(10, 3, bc_plus);
    hashclash::differentialpath b;
    b.setbitcondition(0, 1, bc_minus);

    paths.insert(a);
    paths.insert(b);
    paths.insert(a);
    CHECK(paths.size() == 2);
}

TEST_CASE("md5 differential path comparisons satisfy ordering axioms") {
    std::vector<hashclash::differentialpath> paths(4);
    paths[1].setbitcondition(3, 0, bc_plus);
    paths[2].setbitcondition(10, 1, bc_minus);
    paths[3].setbitcondition(3, 0, bc_plus);
    paths[3].setbitcondition(10, 1, bc_minus);

    for (const auto &a : paths)
        for (const auto &b : paths) {
            CHECK((a == b) == (b == a));
            CHECK((a == b) == (!(a < b) && !(b < a)));
            CHECK(int(a < b) + int(b < a) + int(a == b) == 1);
        }
    for (const auto &a : paths)
        for (const auto &b : paths)
            for (const auto &c : paths) {
                if (a == b && b == c)
                    CHECK(a == c);
                if (a < b && b < c)
                    CHECK(a < c);
            }
}
