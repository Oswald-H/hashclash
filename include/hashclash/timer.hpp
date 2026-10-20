/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_TIMER_HPP
#define HASHCLASH_TIMER_HPP

#include "types.hpp"

#include <chrono>

namespace hashclash {

// Note: the CUDA sources (e.g. src/md5birthdaysearch/cuda_md5.cu) keep their own
// separate copy of the timer implementation and do not include this header.
class timer {
  public:
    timer(bool direct_start = false);
    void start();
    void stop();
    [[nodiscard]] double time() const; // get time between start and stop (or now if still
                                       // running) in seconds
    [[nodiscard]] bool isrunning() const { return running; } // check if timer is running

  private:
    using clock_type = std::chrono::steady_clock;

    clock_type::time_point tstart{};
    clock_type::time_point tend{};
    bool running;
};

} // namespace hashclash

#endif // HASHCLASH_TIMER_HPP
