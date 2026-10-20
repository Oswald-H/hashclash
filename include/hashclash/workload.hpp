/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_WORKLOAD_HPP
#define HASHCLASH_WORKLOAD_HPP

#include <atomic>
#include <cstddef>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace hashclash {

// Spawns `jobs` std::jthreads that each invoke f(i, jobs), then joins all of them.
// The callable is stored once and copied into every worker, so each thread starts from the same
// state no matter whether the caller passed an lvalue or an rvalue; a move-only callable cannot
// satisfy that contract and is rejected at compile time.
template <typename F> void run_workload(std::size_t jobs, F &&f) {
    using callable_type = std::decay_t<F>;
    static_assert(std::is_copy_constructible_v<callable_type>,
                  "hashclash::run_workload requires a copy-constructible callable: every worker "
                  "thread runs its own copy of it");
    callable_type callable(std::forward<F>(f));
    std::vector<std::jthread> threadgroup;
    threadgroup.reserve(jobs);
    for (std::size_t i = 0; i < jobs; ++i)
        threadgroup.emplace_back(callable, i, jobs);
    for (auto &thread : threadgroup)
        thread.join();
}

// An exception escaping a thread body would call std::terminate, so a worker cannot report
// one directly to the thread that called run_workload. guard_exception catches the expected
// Exception at the thread boundary and signals it through `flag` instead, while any other
// exception type still propagates (and thus terminates).
template <class Exception, typename F> auto guard_exception(F &&f, std::atomic<bool> &flag) {
    return [f = std::forward<F>(f), &flag](std::size_t i, std::size_t n) mutable {
        try {
            f(i, n);
        } catch (const Exception &) {
            flag = true;
        }
    };
}

} // namespace hashclash

#endif // HASHCLASH_WORKLOAD_HPP
