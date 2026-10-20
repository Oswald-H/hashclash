/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

/*

very fast inlined xorshift random number generators with periods 2^32 - 1, 2^64 - 1, 2^96 - 1 and
2^128 - 1 by G. Marsaglia: http://www.jstatsoft.org/v08/i14/xorshift.pdf

*/

#ifndef HASHCLASH_XORSHIFT_RNG_HPP
#define HASHCLASH_XORSHIFT_RNG_HPP

#include <mutex>
#include <random>

#include "types.hpp"

namespace hashclash {

// seed all generators using 32-bit values
// seed state is deterministically dependent on the given values
void seed(uint32 s);
void seed(const uint32 *sbuf, unsigned len);
// add seed to the generators
// seed state is changed by given values
void addseed(uint32 s);
void addseed(const uint32 *sbuf, unsigned len);

// seeds used, these are initialized from OS entropy at startup
extern uint32 seedd;
extern uint32 seed32_1;
extern uint32 seed32_2;
extern uint32 seed32_3;
extern uint32 seed32_4;

/******** Random generator with perdiod (2^32 - 1)*2^32 **********/
inline uint32 xrng32() {
    seed32_1 ^= seed32_1 << 13;
    seed32_1 ^= seed32_1 >> 17;
    return (seed32_1 ^= seed32_1 << 5) + (seedd += 789456123);
}

/******** Random generator with perdiod (2^64 - 1)*2^32 **********/
inline uint32 xrng64() {
    uint32 t = seed32_1 ^ (seed32_1 << 10);
    seed32_1 = seed32_2;
    seed32_2 = (seed32_2 ^ (seed32_2 >> 10)) ^ (t ^ (t >> 13));
    return seed32_1 + (seedd += 789456123);
}

/******** Random generator with perdiod (2^96 - 1)*2^32 **********/
inline uint32 xrng96() {
    uint32 t = seed32_1 ^ (seed32_1 << 10);
    seed32_1 = seed32_2;
    seed32_2 = seed32_3;
    seed32_3 = (seed32_3 ^ (seed32_3 >> 26)) ^ (t ^ (t >> 5));
    return seed32_1 + (seedd += 789456123);
}

/******** Random generator with perdiod (2^128 - 1)*2^32 **********/
inline uint32 xrng128() {
    uint32 t = seed32_1 ^ (seed32_1 << 5);
    seed32_1 = seed32_2;
    seed32_2 = seed32_3;
    seed32_3 = seed32_4;
    seed32_4 = (seed32_4 ^ (seed32_4 >> 1)) ^ (t ^ (t >> 14));
    return seed32_1 + (seedd += 789456123);
}

// call this in a other global constructor using any xrng (without seeding itself)
// due to the inpredictable order of which global constructors are called
// otherwise the correct seeding of xrng is not guaranteed
void hashclash_rng_hpp_init();

// Per-instance xorshift128 generator. The xrng32/64/96/128 generators above
// share global state and are not thread-safe; worker threads must use their own
// localxrng instead.
struct localxrng {
    uint64_t x, y;

    localxrng() {
        std::random_device rd;
        x = uint64_t(rd()) + (uint64_t(rd()) << 32);
        y = uint64_t(rd()) + (uint64_t(rd()) << 32);
        x |= 1;
    }
    explicit localxrng(uint64_t seed) : x(seed | 1), y((seed * 0x9E3779B97F4A7C15ull) | 1) {}

    uint32_t operator()() {
        uint64_t t = x, s = y;
        x = y;
        t ^= t << 23;
        t ^= t >> 18;
        t ^= s ^ (s >> 5);
        y = t;
        return uint32_t((t + s) >> 32);
    }
};

// Thread-safe wrapper around the global generator for the rare worker-thread
// call sites that cannot use a localxrng (e.g. naming output files).
inline uint32 xrng128_locked() {
    static std::mutex rng_mutex;
    std::lock_guard<std::mutex> lock(rng_mutex);
    return xrng128();
}

} // namespace hashclash

#endif // HASHCLASH_XORSHIFT_RNG_HPP
