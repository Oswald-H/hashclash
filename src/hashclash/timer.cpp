/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <hashclash/timer.hpp>

namespace hashclash {

timer::timer(bool direct_start) : running(false) {
    if (direct_start)
        start();
}

void timer::start() {
    running = true;
    tstart = clock_type::now();
}

void timer::stop() {
    tend = clock_type::now();
    running = false;
}

double timer::time() const {
    const clock_type::time_point end = running ? clock_type::now() : tend;
    return std::chrono::duration<double>(end - tstart).count();
}

} // namespace hashclash
