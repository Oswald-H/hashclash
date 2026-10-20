/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_CPUPERFORMANCE_HPP
#define HASHCLASH_CPUPERFORMANCE_HPP

#include <vector>
#include <string>
#include <iostream>
#include <cstdio>

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <chrono>

#include "types.hpp"

namespace hashclash {

inline uint64 cpu_timestamp() {
#if defined(_MSC_VER)
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32 highpart, lowpart;
    asm volatile("rdtsc" : "=d"(highpart), "=a"(lowpart));
    return (uint64(highpart) << 32) | uint64(lowpart);
#else
    // portable fallback on architectures without a cycle counter
    return static_cast<uint64>(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

inline void start_update_counter(uint64 &performancecounter) {
    performancecounter -= cpu_timestamp();
}
inline void end_update_counter(uint64 &performancecounter) {
    performancecounter += cpu_timestamp();
}

class update_performance_counter {
    uint64 &_counter;

  public:
    update_performance_counter(uint64 &performance_counter) : _counter(performance_counter) {
        _counter -= cpu_timestamp();
    }
    ~update_performance_counter() { _counter += cpu_timestamp(); }
};

class performance_counter_manager {
    std::vector<uint64 *> counters;
    std::vector<std::string> descriptions;

  public:
    void add_performance_counter(uint64 &counter, const std::string &description) {
        counters.push_back(&counter);
        descriptions.push_back(description);
    }

    void show_results() {
        for (unsigned i = 0; i < counters.size(); ++i) {
            std::cout << "Counter " << i << ": \t" << descriptions[i] << '\n';
            std::cout << "Counter " << i << ": \tValue = " << (*counters[i]) << '\n';
        }
    }
};
} // namespace hashclash

#endif // HASHCLASH_CPUPERFORMANCE_HPP
