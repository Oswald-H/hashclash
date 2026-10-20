/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_ALIGNED_BUFFER_HPP
#define HASHCLASH_ALIGNED_BUFFER_HPP

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace hashclash {

// Owns raw storage aligned to a requested boundary; objects are constructed separately.
class aligned_buffer {
  public:
    aligned_buffer() noexcept = default;

    aligned_buffer(std::size_t size, std::size_t alignment) { allocate(size, alignment); }

    aligned_buffer(const aligned_buffer &) = delete;
    aligned_buffer &operator=(const aligned_buffer &) = delete;

    aligned_buffer(aligned_buffer &&other) noexcept
        : storage(std::exchange(other.storage, nullptr)) {}

    aligned_buffer &operator=(aligned_buffer &&other) noexcept {
        if (this != &other) {
            release();
            storage = std::exchange(other.storage, nullptr);
        }
        return *this;
    }

    ~aligned_buffer() { release(); }

    void allocate(std::size_t size, std::size_t alignment) {
        release();
        if (size == 0)
            return;
#if defined(_MSC_VER)
        storage = _aligned_malloc(size, alignment);
#else
        if (::posix_memalign(&storage, alignment, size) != 0)
            storage = nullptr;
#endif
        if (storage == nullptr)
            throw std::bad_alloc();
    }

    void release() noexcept {
#if defined(_MSC_VER)
        _aligned_free(storage);
#else
        std::free(storage);
#endif
        storage = nullptr;
    }

    void *data() noexcept { return storage; }
    const void *data() const noexcept { return storage; }

    explicit operator bool() const noexcept { return storage != nullptr; }

  private:
    void *storage = nullptr;
};

} // namespace hashclash

#endif // HASHCLASH_ALIGNED_BUFFER_HPP
