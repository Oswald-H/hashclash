/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_MD5DETAIL_HPP
#define HASHCLASH_MD5DETAIL_HPP

#include <stdexcept>

#include "types.hpp"

namespace hashclash {

void md5compress(uint32 ihv[4], const uint32 block[16]);

const uint32 Qoff = 3;

inline uint32 md5_ff(uint32 b, uint32 c, uint32 d) { return d ^ (b & (c ^ d)); }
inline uint32 md5_gg(uint32 b, uint32 c, uint32 d) { return c ^ (d & (b ^ c)); }
inline uint32 md5_hh(uint32 b, uint32 c, uint32 d) { return b ^ c ^ d; }
inline uint32 md5_ii(uint32 b, uint32 c, uint32 d) { return c ^ (b | ~d); }

inline constexpr uint32 md5_iv[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};

inline constexpr uint32 md5_ac[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x2441453,  0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

inline constexpr unsigned md5_rc[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                        5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

inline constexpr unsigned md5_wt[64] = {
    0,  1,  2,  3,  4, 5, 6,  7, 8,  9,  10, 11, 12, 13, 14, 15, 1, 6,  11, 0, 5, 10,
    15, 4,  9,  14, 3, 8, 13, 2, 7,  12, 5,  8,  11, 14, 1,  4,  7, 10, 13, 0, 3, 6,
    9,  12, 15, 2,  0, 7, 14, 5, 12, 3,  10, 1,  8,  15, 6,  13, 4, 11, 2,  9};

template <unsigned t>
inline uint32 md5_step(uint32 Qtm0, uint32 Qtm1, uint32 Qtm2, uint32 Qtm3, uint32 wt) {
    uint32 tt;
    switch (t >> 4) {
    case 0:
        tt = md5_ff(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 1:
        tt = md5_gg(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 2:
        tt = md5_hh(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 3:
        tt = md5_ii(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    default:
        throw std::logic_error("md5detail.hpp: md5_step: invalid round");
    }
}

template <unsigned t>
inline uint32 md5_step_bw(uint32 Qtp1, uint32 Qtm0, uint32 Qtm1, uint32 Qtm2, uint32 wt) {
    uint32 rr;
    switch (t >> 4) {
    case 0:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_ff(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 1:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_gg(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 2:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_hh(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 3:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_ii(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    default:
        throw std::logic_error("md5detail.hpp: md5_step_bw: invalid round");
    }
}

inline uint32 md5_step(unsigned t, uint32 Qtm0, uint32 Qtm1, uint32 Qtm2, uint32 Qtm3, uint32 wt) {
    uint32 tt;
    switch (t >> 4) {
    case 0:
        tt = md5_ff(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 1:
        tt = md5_gg(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 2:
        tt = md5_hh(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    case 3:
        tt = md5_ii(Qtm0, Qtm1, Qtm2) + Qtm3 + wt + md5_ac[t];
        return Qtm0 + rotate_left(tt, md5_rc[t]);
    default:
        throw std::logic_error("md5detail.hpp: md5_step: invalid round");
    }
}

inline uint32 md5_step_bw(unsigned t, uint32 Qtp1, uint32 Qtm0, uint32 Qtm1, uint32 Qtm2,
                          uint32 wt) {
    uint32 rr;
    switch (t >> 4) {
    case 0:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_ff(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 1:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_gg(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 2:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_hh(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    case 3:
        rr = Qtp1 - Qtm0;
        return rotate_right(rr, md5_rc[t]) - md5_ii(Qtm0, Qtm1, Qtm2) - wt - md5_ac[t];
    default:
        throw std::logic_error("md5detail.hpp: md5_step_bw: invalid round");
    }
}

} // namespace hashclash

#endif // HASHCLASH_MD5DETAIL_HPP
