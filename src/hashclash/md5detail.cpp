/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <hashclash/md5detail.hpp>

namespace hashclash {

#define HASHCLASH_MD5COMPRESS_STEP(f, a, b, c, d, m, ac, rc)                                       \
    a += f(b, c, d) + m + ac;                                                                      \
    a = rotate_left(a, rc);                                                                        \
    a += b;

void md5compress(uint32 ihv[4], const uint32 block[16]) {
    uint32 a = ihv[0];
    uint32 b = ihv[1];
    uint32 c = ihv[2];
    uint32 d = ihv[3];

    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[0], 0xd76aa478, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[1], 0xe8c7b756, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[2], 0x242070db, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[3], 0xc1bdceee, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[4], 0xf57c0faf, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[5], 0x4787c62a, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[6], 0xa8304613, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[7], 0xfd469501, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[8], 0x698098d8, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[9], 0x8b44f7af, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[10], 0xffff5bb1, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[11], 0x895cd7be, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[12], 0x6b901122, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[13], 0xfd987193, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[14], 0xa679438e, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[15], 0x49b40821, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[1], 0xf61e2562, 5);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[6], 0xc040b340, 9);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[11], 0x265e5a51, 14);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[0], 0xe9b6c7aa, 20);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[5], 0xd62f105d, 5);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[10], 0x02441453, 9);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[15], 0xd8a1e681, 14);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[4], 0xe7d3fbc8, 20);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[9], 0x21e1cde6, 5);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[14], 0xc33707d6, 9);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[3], 0xf4d50d87, 14);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[8], 0x455a14ed, 20);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[13], 0xa9e3e905, 5);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[2], 0xfcefa3f8, 9);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[7], 0x676f02d9, 14);
    HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[12], 0x8d2a4c8a, 20);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[5], 0xfffa3942, 4);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[8], 0x8771f681, 11);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[11], 0x6d9d6122, 16);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[14], 0xfde5380c, 23);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[1], 0xa4beea44, 4);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[4], 0x4bdecfa9, 11);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[7], 0xf6bb4b60, 16);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[10], 0xbebfbc70, 23);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[13], 0x289b7ec6, 4);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[0], 0xeaa127fa, 11);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[3], 0xd4ef3085, 16);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[6], 0x04881d05, 23);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[9], 0xd9d4d039, 4);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[12], 0xe6db99e5, 11);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[15], 0x1fa27cf8, 16);
    HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[2], 0xc4ac5665, 23);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[0], 0xf4292244, 6);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[7], 0x432aff97, 10);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[14], 0xab9423a7, 15);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[5], 0xfc93a039, 21);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[12], 0x655b59c3, 6);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[3], 0x8f0ccc92, 10);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[10], 0xffeff47d, 15);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[1], 0x85845dd1, 21);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[8], 0x6fa87e4f, 6);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[15], 0xfe2ce6e0, 10);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[6], 0xa3014314, 15);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[13], 0x4e0811a1, 21);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[4], 0xf7537e82, 6);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[11], 0xbd3af235, 10);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[2], 0x2ad7d2bb, 15);
    HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[9], 0xeb86d391, 21);

    ihv[0] += a;
    ihv[1] += b;
    ihv[2] += c;
    ihv[3] += d;
}

} // namespace hashclash
