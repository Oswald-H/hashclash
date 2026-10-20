/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_SHA1DETAIL_HPP
#define HASHCLASH_SHA1DETAIL_HPP

#include "types.hpp"

namespace hashclash {

void sha1compress(uint32 ihv[5], const uint32 block[16]);
void sha1compress_me(uint32 ihv[5], const uint32 me[80]);

const uint32 Qoffsha1 = 4;

FUNC_PREFIX inline uint32 sha1_f1(uint32 b, uint32 c, uint32 d) { return d ^ (b & (c ^ d)); }
FUNC_PREFIX inline uint32 sha1_f2(uint32 b, uint32 c, uint32 d) { return b ^ c ^ d; }
FUNC_PREFIX inline uint32 sha1_f3(uint32 b, uint32 c, uint32 d) { return (b & (c | d)) | (c & d); }
FUNC_PREFIX inline uint32 sha1_f4(uint32 b, uint32 c, uint32 d) { return b ^ c ^ d; }

inline constexpr uint32 sha1_iv[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
inline constexpr uint32 sha1_ac[4] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};

FUNC_PREFIX inline void sha1_me(uint32 block[80], const uint32 msg[16]) {
    unsigned i;
    for (i = 0; i < 16; ++i)
        block[i] = (rotate_left(msg[i], 24) & 0xFF00FF00) | (rotate_left(msg[i], 8) & 0x00FF00FF);
    for (i = 16; i < 80; ++i)
        block[i] = rotate_left(block[i - 3] ^ block[i - 8] ^ block[i - 14] ^ block[i - 16], 1);
}

FUNC_PREFIX inline void sha1_me_simple(uint32 block[80], const uint32 msg[16]) {
    unsigned i;
    for (i = 0; i < 16; ++i)
        block[i] = msg[i];
    for (i = 16; i < 80; ++i)
        block[i] = rotate_left(block[i - 3] ^ block[i - 8] ^ block[i - 14] ^ block[i - 16], 1);
}

FUNC_PREFIX inline void sha1_me_generalised(uint32 block[80], const uint32 msg[16],
                                            unsigned offset) {
    int i;
    for (i = 0; i < 16; ++i)
        block[offset + i] = msg[i];
    for (i = offset + 16; i < 80; ++i)
        block[i] = rotate_left(block[i - 3] ^ block[i - 8] ^ block[i - 14] ^ block[i - 16], 1);
    for (i = int(offset) - 1; i >= 0; --i)
        block[i] = rotate_right(block[i + 16], 1) ^ block[i + 13] ^ block[i + 8] ^ block[i + 2];
}

FUNC_PREFIX inline void sha1_step_round1(unsigned t, uint32 Q[], const uint32 me[]) {
    const int offset = 4;
    uint32 Ft = sha1_f1(Q[offset + t - 1], rotate_left(Q[offset + t - 2], 30),
                        rotate_left(Q[offset + t - 3], 30));
    Q[offset + t + 1] = Ft + sha1_ac[0] + me[t] + rotate_left(Q[offset + t], 5) +
                        rotate_left(Q[offset + t - 4], 30);
}
FUNC_PREFIX inline void sha1_step_round2(unsigned t, uint32 Q[], const uint32 me[]) {
    const int offset = 4;
    uint32 Ft = sha1_f2(Q[offset + t - 1], rotate_left(Q[offset + t - 2], 30),
                        rotate_left(Q[offset + t - 3], 30));
    Q[offset + t + 1] = Ft + sha1_ac[1] + me[t] + rotate_left(Q[offset + t], 5) +
                        rotate_left(Q[offset + t - 4], 30);
}
FUNC_PREFIX inline void sha1_step_round3(unsigned t, uint32 Q[], const uint32 me[]) {
    const int offset = 4;
    uint32 Ft = sha1_f3(Q[offset + t - 1], rotate_left(Q[offset + t - 2], 30),
                        rotate_left(Q[offset + t - 3], 30));
    Q[offset + t + 1] = Ft + sha1_ac[2] + me[t] + rotate_left(Q[offset + t], 5) +
                        rotate_left(Q[offset + t - 4], 30);
}
FUNC_PREFIX inline void sha1_step_round4(unsigned t, uint32 Q[], const uint32 me[]) {
    const int offset = 4;
    uint32 Ft = sha1_f4(Q[offset + t - 1], rotate_left(Q[offset + t - 2], 30),
                        rotate_left(Q[offset + t - 3], 30));
    Q[offset + t + 1] = Ft + sha1_ac[3] + me[t] + rotate_left(Q[offset + t], 5) +
                        rotate_left(Q[offset + t - 4], 30);
}

FUNC_PREFIX inline void sha1_step(unsigned t, uint32 Q[], const uint32 me[]) {
    if (t < 40) {
        if (t < 20)
            sha1_step_round1(t, Q, me);
        else
            sha1_step_round2(t, Q, me);
    } else {
        if (t < 60)
            sha1_step_round3(t, Q, me);
        else
            sha1_step_round4(t, Q, me);
    }
}

template <unsigned t> FUNC_PREFIX inline void sha1_step(uint32 Q[], const uint32 me[]) {
    if (t < 40) {
        if (t < 20)
            sha1_step_round1(t, Q, me);
        else
            sha1_step_round2(t, Q, me);
    } else {
        if (t < 60)
            sha1_step_round3(t, Q, me);
        else
            sha1_step_round4(t, Q, me);
    }
}

} // namespace hashclash

#endif // HASHCLASH_SHA1DETAIL_HPP
