#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <new>
#include <utility>

#include <hashclash/aligned_buffer.hpp>

namespace {

struct probe {
    int id;
    double value;
};

} // namespace

TEST_CASE("default-constructed aligned_buffer is null") {
    hashclash::aligned_buffer buffer;
    CHECK_FALSE(buffer);
    CHECK(buffer.data() == nullptr);
}

TEST_CASE("aligned_buffer provides storage aligned to the requested boundary") {
    hashclash::aligned_buffer buffer(64, 64);
    REQUIRE(buffer);
    REQUIRE(buffer.data() != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(buffer.data()) % 64 == 0);

    const hashclash::aligned_buffer &view = buffer;
    CHECK(view.data() == buffer.data());
}

TEST_CASE("aligned_buffer storage supports placement new and explicit destruction") {
    hashclash::aligned_buffer buffer(sizeof(probe), alignof(probe));
    REQUIRE(buffer);

    auto *object = new (buffer.data()) probe{7, 2.5};
    CHECK(object->id == 7);
    CHECK(object->value == 2.5);

    object->id = 42;
    object->value = -1.25;
    CHECK(object->id == 42);
    CHECK(object->value == -1.25);

    object->~probe();
}

TEST_CASE("aligned_buffer can be reallocated with a new size and alignment") {
    hashclash::aligned_buffer buffer(256, 128);
    REQUIRE(buffer);
    CHECK(reinterpret_cast<std::uintptr_t>(buffer.data()) % 128 == 0);

    buffer.allocate(64, 64);
    REQUIRE(buffer);
    CHECK(reinterpret_cast<std::uintptr_t>(buffer.data()) % 64 == 0);
}

TEST_CASE("aligned_buffer move construction transfers ownership") {
    hashclash::aligned_buffer source(64, 64);
    REQUIRE(source);
    const void *const storage = source.data();

    hashclash::aligned_buffer moved(std::move(source));
    CHECK_FALSE(source);
    CHECK(source.data() == nullptr);
    REQUIRE(moved);
    CHECK(moved.data() == storage);
}

TEST_CASE("aligned_buffer move assignment transfers ownership") {
    hashclash::aligned_buffer source(64, 64);
    REQUIRE(source);
    const void *const storage = source.data();

    hashclash::aligned_buffer target(32, 64);
    REQUIRE(target);

    target = std::move(source);
    CHECK_FALSE(source);
    CHECK(source.data() == nullptr);
    REQUIRE(target);
    CHECK(target.data() == storage);
}

TEST_CASE("aligned_buffer zero-size allocation stays null") {
    hashclash::aligned_buffer buffer(64, 64);
    REQUIRE(buffer);

    buffer.allocate(0, 64);
    CHECK_FALSE(buffer);
    CHECK(buffer.data() == nullptr);
}

TEST_CASE("aligned_buffer release is idempotent") {
    hashclash::aligned_buffer buffer(64, 64);
    REQUIRE(buffer);

    buffer.release();
    CHECK_FALSE(buffer);
    CHECK(buffer.data() == nullptr);

    buffer.release();
    CHECK_FALSE(buffer);
    CHECK(buffer.data() == nullptr);
}
