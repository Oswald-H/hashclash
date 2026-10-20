#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <hashclash/conditions.hpp>
#include <hashclash/sdr.hpp>
#include <hashclash/sha1differentialpath.hpp>

TEST_CASE("sha1 differential path const accessor returns stored conditions") {
    hashclash::sha1differentialpath path;
    path.setbitcondition(3, 0, hashclash::bc_plus);
    path.setbitcondition(3, 5, hashclash::bc_minus);

    const hashclash::sha1differentialpath &cpath = path;
    CHECK(cpath[3].get(0) == hashclash::bc_plus);
    CHECK(cpath[3].get(5) == hashclash::bc_minus);
    CHECK(cpath[3].bytes[0].get(0) == hashclash::bc_plus);
    CHECK(std::as_const(path)[3].get(5) == hashclash::bc_minus);
}

TEST_CASE("sha1 differential path const accessor throws when out of range") {
    hashclash::sha1differentialpath path;
    path.setbitcondition(3, 0, hashclash::bc_plus);
    const hashclash::sha1differentialpath &cpath = path;

    CHECK(cpath.tbegin() == 3);
    CHECK(cpath.tend() == 4);
    REQUIRE_THROWS_AS(cpath[2], std::out_of_range);
    REQUIRE_THROWS_AS(cpath[-1], std::out_of_range);
    REQUIRE_THROWS_AS(cpath[4], std::out_of_range);
    REQUIRE_THROWS_AS(cpath[100], std::out_of_range);
    CHECK_NOTHROW(cpath[3]);
}

TEST_CASE("sha1 differential path safe accessor returns bc_constant out of range") {
    hashclash::sha1differentialpath path;
    path.setbitcondition(3, 0, hashclash::bc_plus);

    CHECK(path(3, 0) == hashclash::bc_plus);
    CHECK(path(3, 1) == hashclash::bc_constant);
    CHECK(path(-1, 0) == hashclash::bc_constant);
    CHECK(path(2, 0) == hashclash::bc_constant);
    CHECK(path(4, 0) == hashclash::bc_constant);
}

TEST_CASE("sha1 differential path prepends steps on first negative access") {
    hashclash::sha1differentialpath path;
    path.setbitcondition(3, 0, hashclash::bc_plus);
    CHECK(path.tbegin() == 3);
    CHECK(path.tend() == 4);

    path[-2].set(1, hashclash::bc_one);

    CHECK(path.tbegin() == -2);
    CHECK(path.tend() == 4);
    CHECK(path(-2, 1) == hashclash::bc_one);
    CHECK(path(3, 0) == hashclash::bc_plus);
    CHECK(path[3].get(0) == hashclash::bc_plus);

    const hashclash::sha1differentialpath &cpath = path;
    REQUIRE_THROWS_AS(cpath[-3], std::out_of_range);
}

TEST_CASE("sha1 differential path copy preserves offset and conditions") {
    hashclash::sha1differentialpath original;
    original.setbitcondition(-1, 3, hashclash::bc_plus);
    original.setbitcondition(2, 4, hashclash::bc_zero);
    original.setbitcondition(2, 5, hashclash::bc_one);

    CHECK(original.tbegin() == -1);
    CHECK(original.tend() == 3);

    hashclash::sha1differentialpath copy(original);
    CHECK(copy.tbegin() == original.tbegin());
    CHECK(copy.tend() == original.tend());
    CHECK(copy == original);
    const hashclash::sha1differentialpath &ccopy = copy;
    CHECK(ccopy[-1].get(3) == hashclash::bc_plus);
    CHECK(ccopy[2].get(4) == hashclash::bc_zero);
    CHECK(ccopy[2].get(5) == hashclash::bc_one);

    hashclash::sha1differentialpath assigned;
    assigned.setbitcondition(7, 0, hashclash::bc_minus);
    assigned = original;
    CHECK(assigned.tbegin() == original.tbegin());
    CHECK(assigned.tend() == original.tend());
    CHECK(assigned == original);
    const hashclash::sha1differentialpath &cassigned = assigned;
    REQUIRE_THROWS_AS(cassigned[7], std::out_of_range);
}

TEST_CASE("sha1 differential path default offset is initialized") {
    const hashclash::sha1differentialpath path;
    CHECK(path.tbegin() == 0);
    CHECK(path.tend() == 0);
    CHECK(path.nrcond() == 0);

    const hashclash::sha1differentialpath other;
    CHECK(path == other);
    CHECK_FALSE(path != other);
}

TEST_CASE("sha1 differential path equality compares message differences on one-sided steps") {
    hashclash::sha1differentialpath a;
    a.getme(3) = hashclash::sdr(1);
    const hashclash::sha1differentialpath empty;

    CHECK(a != empty);
    CHECK(empty != a);
    CHECK_FALSE(a == empty);

    hashclash::sha1differentialpath same;
    same.getme(3) = hashclash::sdr(1);
    CHECK(a == same);
    CHECK_FALSE(a != same);

    hashclash::sha1differentialpath other;
    other.getme(3) = hashclash::sdr(2);
    CHECK(a != other);
}

TEST_CASE("sha1 differential path trailing message differences are not ignored") {
    hashclash::sha1differentialpath a, b;
    a.getme(0) = hashclash::sdr(1);
    b.getme(0) = hashclash::sdr(1);
    b.getme(1) = hashclash::sdr(1);

    CHECK(a != b);
    CHECK_FALSE(a == b);
    CHECK((a < b) != (b < a));
}

TEST_CASE("sha1 differential path equality is transitive across missing me steps") {
    hashclash::sha1differentialpath a, b, c;
    a.getme(0) = hashclash::sdr(1);
    c.getme(0) = hashclash::sdr(2);

    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);
    CHECK((a < b) != (b < a));
    CHECK((b < c) != (c < b));
    CHECK((a < c) != (c < a));

    b.getme(0) = hashclash::sdr(1);
    c.getme(0) = hashclash::sdr(1);
    CHECK(a == b);
    CHECK(b == c);
    CHECK(a == c);
    CHECK_FALSE(a < b);
    CHECK_FALSE(b < a);
}

TEST_CASE("sha1 differential paths that differ only in me survive unique") {
    std::vector<hashclash::sha1differentialpath> paths(3);
    paths[0].getme(0) = hashclash::sdr(1);
    paths[1].getme(0) = hashclash::sdr(1);
    paths[2].getme(0) = hashclash::sdr(1);
    paths[2].getme(1) = hashclash::sdr(1);

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

    REQUIRE(paths.size() == 2);
    CHECK(paths[0] != paths[1]);
}

TEST_CASE("sha1 differential path comparisons satisfy ordering axioms") {
    std::vector<hashclash::sha1differentialpath> paths(5);
    paths[1].getme(2) = hashclash::sdr(1);
    paths[2].getme(2) = hashclash::sdr(1);
    paths[2].getme(4) = hashclash::sdr(3);
    paths[3].setbitcondition(6, 0, hashclash::bc_plus);
    paths[4].setbitcondition(6, 0, hashclash::bc_plus);
    paths[4].getme(6) = hashclash::sdr(2);

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
