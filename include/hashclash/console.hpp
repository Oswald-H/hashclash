#ifndef HASHCLASH_CONSOLE_HPP
#define HASHCLASH_CONSOLE_HPP

#include <mutex>

namespace hashclash {
// std::cout tolerates concurrent characters, but libc++'s formatting state
// (width/fill/precision) is not synchronized. Route console output from
// worker/loader threads through this mutex to avoid the data race TSan
// reports and interleaved formatting.
inline std::mutex console_mutex;
} // namespace hashclash

#endif // HASHCLASH_CONSOLE_HPP
