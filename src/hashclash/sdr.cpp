/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <iostream>
#include <cstdio>
#include <set>
#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <mutex>
#include <thread>

#include <hashclash/saveload_bz2.hpp>
#include <hashclash/saveload_gz.hpp>
#include <hashclash/sdr.hpp>

namespace hashclash {

std::ostream &operator<<(std::ostream &o, const sdr &n) {
    o << "[!";
    bool first = true;
    for (unsigned b = 0; b < 32; ++b) {
        if (n[b] != 0) {
            if (first)
                first = false;
            else
                o << ",";
            if (n[b] == -1)
                o << "-";
            o << b;
        }
    }
    o << "!]";
    return o;
}

std::istream &operator>>(std::istream &i, sdr &n) {
    n.clear();
    char c;
    if (!(i >> c))
        return i;
    if (c != '[') {
        i.putback(c);
        i.setstate(std::ios::failbit);
        return i;
    }
    if (!(i >> c))
        return i;
    if (c != '!') {
        i.putback(c);
        i.setstate(std::ios::failbit);
        return i;
    }
    // check for empty sdr
    if (!(i >> c))
        return i;
    if (c == '!') {
        if (!(i >> c))
            return i;
        if (c != ']') {
            i.putback(c);
            i.setstate(std::ios::failbit);
        }
        return i;
    }
    i.putback(c);

    char s;
    bool neg = true;
    if (!(i >> s))
        return i;
    if (s != '-') {
        neg = false;
        i.putback(s);
    }
    unsigned bit;
    while (i >> bit >> c) {
        if (!(bit < 32 && (c == ',' || c == '!'))) {
            i.putback(c);
            i.setstate(std::ios::failbit);
            return i;
        }
        n.mask |= 1 << bit;
        if (!neg)
            n.sign |= 1 << bit;

        if (c == '!')
            break;
        neg = true;
        if (!(i >> s))
            return i;
        if (s != '-') {
            neg = false;
            i.putback(s);
        }
    }
    if (!(i >> c))
        return i;
    if (c != ']') {
        i.putback(c);
        i.setstate(std::ios::failbit);
        return i;
    }
    return i;
}

uint32 best_rotated_difference(uint32 diff, int rc) {
    if (diff == 0 || (rc & 31) == 0)
        return diff;

    int rc2 = 32 - rc;
    uint32 bound = 1 << rc2;
    uint32 bound2 = 1 << rc;
    uint32 y = diff >> rc2;
    uint32 x = diff - (y << rc2);
    uint32 p1 = (bound - x) * y;
    uint32 d1 = ((x << rc) | y);
    if ((y << 1) > bound2)
        d1 -= bound2;
    else
        p1 = ((bound - x) << rc) - p1;

    if (x == 0)
        return d1;

    y += 1;
    y &= ~bound2; // erase possible carry at rc-th bit
    uint32 p2 = x * y;
    uint32 d2 = ((x << rc) | y);
    if ((y << 1) > bound2)
        d2 -= bound2;
    else
        p2 = (x << rc) - p2;
    if (p1 > p2)
        return d1;
    return d2;
}

void rotate_difference(uint32 diff, int rc, std::vector<uint32> &rotateddiff, uint32 minprob) {
    rotateddiff.clear();
    if (diff == 0 || (rc & 31) == 0) {
        rotateddiff.push_back(diff);
        return;
    }
    // now p1,p2,p3,p4 < 1 * 2^32, no overflows

    int rc2 = 32 - rc;
    uint32 bound = 1 << rc2;
    uint32 bound2 = 1 << rc;
    uint32 y = diff >> rc2;
    uint32 x = diff - (y << rc2);

    uint32 p1 = (bound - x) * y;
    if (p1 >= minprob)
        rotateddiff.push_back(((x << rc) | y) - bound2);
    uint32 p2 = ((bound - x) << rc) - p1;
    if (p2 >= minprob)
        rotateddiff.push_back((x << rc) | y);

    if (x != 0) {
        y += 1;
        y &= ~bound2; // erase possible carry at rc-th bit

        uint32 p3 = x * y;
        if (p3 >= minprob)
            rotateddiff.push_back(((x << rc) | y) - bound2);
        uint32 p4 = (x << rc) - p1;
        if (p4 >= minprob)
            rotateddiff.push_back((x << rc) | y);
    }
}

void rotate_difference(uint32 diff, int rc, std::vector<std::pair<uint32, double>> &rotateddiff) {
    double pinv = pow(double(2), -32);

    rotateddiff.clear();
    if (diff == 0 || (rc & 31) == 0) {
        rotateddiff.push_back(std::pair<uint32, double>(diff, 1));
        return;
    }

    int rc2 = 32 - rc;
    uint32 bound = 1 << rc2;
    uint32 bound2 = 1 << rc;
    uint32 y = diff >> rc2;
    uint32 x = diff - (y << rc2);

    uint32 p1 = 0;
    if (y) {
        p1 = (bound - x) * y;
        rotateddiff.push_back(
            std::pair<uint32, double>(((x << rc) | y) - bound2, double(p1) * pinv));
    }

    uint32 p2 = ((bound - x) << rc) - p1;
    rotateddiff.push_back(std::pair<uint32, double>((x << rc) | y, double(p2) * pinv));

    uint32 p3 = 0, p4 = 0;
    if (x != 0) {
        y += 1;
        y &= ~bound2; // erase possible carry at rc-th bit

        if (y) {
            p3 = x * y;
            rotateddiff.push_back(
                std::pair<uint32, double>(((x << rc) | y) - bound2, double(p3) * pinv));
        }

        p4 = (x << rc) - p3;
        rotateddiff.push_back(std::pair<uint32, double>((x << rc) | y, double(p4) * pinv));
    }
}

struct sdr_carry {
    std::vector<std::vector<sdr>> positive, negative;
};

// The carry tables are expensive to build and not needed by every tool; build
// them lazily on first use (thread-safe function-local static).
static std::vector<sdr_carry> &sc_table() {
    static std::vector<sdr_carry> sc = [] {
        std::vector<sdr_carry> table(0x800);
        sdr temp;
        for (temp.mask = 0; temp.mask < 0x800; ++temp.mask)
            for (temp.sign = 0; temp.sign < 0x800; ++temp.sign) {
                if (temp.mask != (temp.sign | temp.mask))
                    continue;
                uint32 n = temp.adddiff();
                unsigned w = temp.hw();
                if (n & 0x80000000) // negative
                {
                    n += 0x800;
                    if (table[n].negative.size() < w + 1)
                        table[n].negative.resize(w + 1);
                    table[n].negative[w].push_back(temp);
                } else // positive
                {
                    if (table[n].positive.size() < w + 1)
                        table[n].positive.resize(w + 1);
                    table[n].positive[w].push_back(temp);
                }
            }
        return table;
    }();
    return sc;
}
namespace {
// Built lazily on first use and immutable afterwards, so concurrent readers
// are safe (the previous check-then-init with a mutex had a TOCTOU race).
const std::vector<std::vector<std::pair<unsigned, unsigned>>> &scn_table() {
    static const auto table = [] {
        std::vector<std::vector<std::pair<unsigned, unsigned>>> result(1 << 16);
        sdr temp(0);
        for (temp.mask = 0; temp.mask < 0x10000; ++temp.mask)
            for (temp.sign = 0; temp.sign <= temp.mask; ++temp.sign) {
                if (temp.sign & (~temp.mask))
                    continue;
                unsigned w = temp.hw();
                uint32 n = temp.adddiff();
                if (n & 0x80000000) {
                    n += 0x10000;
                    if (result[n].size() < w + 1)
                        result[n].resize(w + 1);
                    ++result[n][w].second;
                } else {
                    if (result[n].size() < w + 1)
                        result[n].resize(w + 1);
                    ++result[n][w].first;
                }
            }
        return result;
    }();
    return table;
}
} // namespace

unsigned count_sdrs(uint32 n, unsigned maxw) {
    const auto &scn = scn_table();
    uint32 l = n & 0xFFFF;
    uint32 h1 = n - l;
    uint32 h2 = h1 + 0x10000;
    unsigned h1w = hwnaf(h1);
    unsigned h2w = hwnaf(h2);
    unsigned hw = h1w;
    if (h2w < hw)
        hw = h2w;

    const auto &vl = scn[l];
    const auto &vh1 = scn[h1 >> 16];
    const auto &vh2 = scn[h2 >> 16];
    unsigned tot = 0;

    for (unsigned i = 0; i < vl.size() && i + hw <= maxw; ++i) {
        if (vl[i].first == 0 && vl[i].second == 0)
            continue;
        unsigned m = 0;
        for (unsigned j = 0; j < vh1.size() && i + j <= maxw; ++j)
            m += vh1[j].first + vh1[j].second;
        tot += vl[i].first * m;
        m = 0;
        for (unsigned j = 0; j < vh2.size() && i + j <= maxw; ++j)
            m += vh2[j].first + vh2[j].second;
        tot += vl[i].second * m;
    }
    return tot;
}

using vec_vec_sdr_t = std::vector<std::vector<sdr>>;
void table_sdrs(std::vector<sdr> &result, uint32 n, unsigned maxw) {
    result.clear();
    if (maxw < hwnaf(n))
        return;
    std::vector<triple<uint32, uint32, uint32>> breakn(8);
    std::vector<triple<vec_vec_sdr_t *, vec_vec_sdr_t *, vec_vec_sdr_t *>> breaknsdr(8);
    uint32 m0 = n & 0x7FF;
    uint32 n0 = n - m0;
    uint32 m1 = n0 & 0x3FFFFF;
    uint32 m2 = n0 - m1;
    breakn[0] = make_triple(m0, m1, m2);
    breakn[4] = make_triple(m0, m1, m2);
    breaknsdr[0] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].positive,
                               &sc_table()[m2 >> 21].positive);
    breaknsdr[4] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].positive,
                               &sc_table()[m2 >> 21].negative);

    m2 += 0x400000;
    breakn[2] = make_triple(m0, m1, m2);
    breakn[6] = make_triple(m0, m1, m2);
    breaknsdr[2] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].negative,
                               &sc_table()[m2 >> 21].positive);
    breaknsdr[6] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].negative,
                               &sc_table()[m2 >> 21].negative);

    n0 += 0x800;
    m1 = n0 & 0x3FFFFF;
    m2 = n0 - m1;
    breakn[1] = make_triple(m0, m1, m2);
    breakn[5] = make_triple(m0, m1, m2);
    breaknsdr[1] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].positive,
                               &sc_table()[m2 >> 21].positive);
    breaknsdr[5] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].positive,
                               &sc_table()[m2 >> 21].negative);

    m2 += 0x400000;
    breakn[3] = make_triple(m0, m1, m2);
    breakn[7] = make_triple(m0, m1, m2);
    breaknsdr[3] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].negative,
                               &sc_table()[m2 >> 21].positive);
    breaknsdr[7] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].negative,
                               &sc_table()[m2 >> 21].negative);
    std::vector<sdr>::const_iterator cit, citend;
    for (unsigned l = 0; l < breakn.size(); ++l) {
        vec_vec_sdr_t &v0 = *breaknsdr[l].first;
        vec_vec_sdr_t &v1 = *breaknsdr[l].second;
        vec_vec_sdr_t &v2 = *breaknsdr[l].third;
        unsigned w0 = 0;
        while (w0 < v0.size() && 0 == v0[w0].size())
            ++w0;
        unsigned w1 = 0;
        while (w1 < v1.size() && 0 == v1[w1].size())
            ++w1;
        unsigned w2 = 0;
        while (w2 < v2.size() && 0 == v2[w2].size())
            ++w2;
        unsigned w0max = maxw - w1 - w2;
        unsigned w1max = maxw - w2;
        for (unsigned i0 = w0; i0 <= w0max && i0 < v0.size(); ++i0)
            for (unsigned j0 = 0; j0 < v0[i0].size(); ++j0) {
                sdr temp0 = v0[i0][j0];
                for (unsigned i1 = w1; i0 + i1 <= w1max && i1 < v1.size(); ++i1)
                    for (unsigned j1 = 0; j1 < v1[i1].size(); ++j1) {
                        sdr temp1 = temp0;
                        temp1.mask ^= v1[i1][j1].mask << 11;
                        temp1.sign ^= v1[i1][j1].sign << 11;
                        for (unsigned i2 = w2; i0 + i1 + i2 <= maxw && i2 < v2.size(); ++i2) {
                            cit = v2[i2].begin();
                            citend = v2[i2].end();
                            for (; cit != citend; ++cit) {
                                sdr temp2 = temp1;
                                temp2.mask ^= cit->mask << 21;
                                temp2.sign ^= cit->sign << 21;
                                result.push_back(temp2);
                            }
                        }
                    }
            }
    }
}

unsigned count_sdrs(uint32 n, unsigned w, bool signpos) {
    const auto &scn = scn_table();
    uint32 l = n & 0xFFFF;
    uint32 h1 = n - l;
    uint32 h2 = h1 + 0x10000;
    unsigned h1w = hwnaf(h1);
    unsigned h2w = hwnaf(h2);
    unsigned hw = h1w;
    if (h2w < hw)
        hw = h2w;

    const auto &vl = scn[l];
    const auto &vh1 = scn[h1 >> 16];
    const auto &vh2 = scn[h2 >> 16];
    unsigned tot = 0;

    for (unsigned i = 0; i < vl.size() && i + hw <= w; ++i) {
        if (signpos) {
            if (w - i < vh1.size())
                tot += vl[i].first * vh1[w - i].first;
            if (w - i < vh2.size())
                tot += vl[i].second * vh2[w - i].first;
        } else {
            if (w - i < vh1.size())
                tot += vl[i].first * vh1[w - i].second;
            if (w - i < vh2.size())
                tot += vl[i].second * vh2[w - i].second;
        }
    }
    return tot;
}

unsigned count_sdrs(sdr n, unsigned maxw, unsigned rot) {
    sdr low = n, high = n;
    high.mask >>= 32 - rot;
    high.sign >>= 32 - rot;
    low.mask &= (~uint32(0)) >> rot;
    low.sign &= low.mask;

    uint32 highdiff = high.adddiff();
    uint32 lowdiff = low.adddiff();
    bool lowpos = (lowdiff & 0x80000000) == 0;
    bool highpos = (highdiff & 0x80000000) == 0;

    unsigned lowcnt[33];
    unsigned highcnt[33];
    unsigned lowhw = hwnaf(lowdiff);
    unsigned highhw = hwnaf(highdiff);
    lowdiff <<= rot;
    highdiff <<= 32 - rot;
    for (unsigned i = lowhw; i + highhw <= maxw; ++i)
        lowcnt[i] = count_sdrs(lowdiff, i, lowpos);
    for (unsigned i = highhw; i + lowhw <= maxw; ++i)
        highcnt[i] = count_sdrs(highdiff, i, highpos);
    unsigned tot = 0;
    for (unsigned wl = lowhw; wl + highhw <= maxw; ++wl) {
        unsigned m = 0;
        for (unsigned wh = highhw; wh + wl <= maxw; ++wh)
            m += highcnt[wh];
        tot += lowcnt[wl] * m;
    }
    return tot;
}

void table_sdrs(std::vector<sdr> &result, uint32 n, unsigned w, bool signpos) {
    result.clear();
    if (w < hwnaf(n))
        return;
    std::vector<triple<uint32, uint32, uint32>> breakn(4);
    std::vector<triple<vec_vec_sdr_t *, vec_vec_sdr_t *, vec_vec_sdr_t *>> breaknsdr(4);
    uint32 m0 = n & 0x7FF;
    uint32 n0 = n - m0;
    uint32 m1 = n0 & 0x3FFFFF;
    uint32 m2 = n0 - m1;
    breakn[0] = make_triple(m0, m1, m2);
    if (signpos)
        breaknsdr[0] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].positive,
                                   &sc_table()[m2 >> 21].positive);
    else
        breaknsdr[0] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].positive,
                                   &sc_table()[m2 >> 21].negative);

    m2 += 0x400000;
    breakn[2] = make_triple(m0, m1, m2);
    if (signpos)
        breaknsdr[2] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].negative,
                                   &sc_table()[m2 >> 21].positive);
    else
        breaknsdr[2] = make_triple(&sc_table()[m0].positive, &sc_table()[m1 >> 11].negative,
                                   &sc_table()[m2 >> 21].negative);

    n0 += 0x800;
    m1 = n0 & 0x3FFFFF;
    m2 = n0 - m1;
    breakn[1] = make_triple(m0, m1, m2);
    if (signpos)
        breaknsdr[1] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].positive,
                                   &sc_table()[m2 >> 21].positive);
    else
        breaknsdr[1] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].positive,
                                   &sc_table()[m2 >> 21].negative);

    m2 += 0x400000;
    breakn[3] = make_triple(m0, m1, m2);
    if (signpos)
        breaknsdr[3] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].negative,
                                   &sc_table()[m2 >> 21].positive);
    else
        breaknsdr[3] = make_triple(&sc_table()[m0].negative, &sc_table()[m1 >> 11].negative,
                                   &sc_table()[m2 >> 21].negative);
    std::vector<sdr>::const_iterator cit, citend;
    for (unsigned l = 0; l < breakn.size(); ++l) {
        vec_vec_sdr_t &v0 = *breaknsdr[l].first;
        vec_vec_sdr_t &v1 = *breaknsdr[l].second;
        vec_vec_sdr_t &v2 = *breaknsdr[l].third;
        unsigned w0 = 0;
        while (w0 < v0.size() && 0 == v0[w0].size())
            ++w0;
        unsigned w1 = 0;
        while (w1 < v1.size() && 0 == v1[w1].size())
            ++w1;
        unsigned w2 = 0;
        while (w2 < v2.size() && 0 == v2[w2].size())
            ++w2;
        unsigned w0max = w - w1 - w2;
        unsigned w1max = w - w2;
        for (unsigned i0 = w0; i0 <= w0max && i0 < v0.size(); ++i0)
            for (unsigned j0 = 0; j0 < v0[i0].size(); ++j0) {
                sdr temp0 = v0[i0][j0];
                for (unsigned i1 = w1; i0 + i1 <= w1max && i1 < v1.size(); ++i1)
                    if (w - i0 - i1 < v2.size())
                        for (unsigned j1 = 0; j1 < v1[i1].size(); ++j1) {
                            sdr temp1 = temp0;
                            temp1.mask ^= v1[i1][j1].mask << 11;
                            temp1.sign ^= v1[i1][j1].sign << 11;
                            cit = v2[w - i0 - i1].begin();
                            citend = v2[w - i0 - i1].end();
                            for (; cit != citend; ++cit) {
                                sdr temp2 = temp1;
                                temp2.mask ^= cit->mask << 21;
                                temp2.sign ^= cit->sign << 21;
                                result.push_back(temp2);
                            }
                        }
            }
    }
}

void table_sdrs(std::vector<sdr> &result, sdr n, unsigned maxw, unsigned rot) {
    result.clear();
    if (maxw < hwnaf(n))
        return;
    sdr low = n, high = n;
    high.mask >>= 32 - rot;
    high.sign >>= 32 - rot;
    low.mask &= (~uint32(0)) >> rot;
    low.sign &= low.mask;

    uint32 highdiff = high.adddiff();
    uint32 lowdiff = low.adddiff();
    bool lowpos = (lowdiff & 0x80000000) == 0;
    bool highpos = (highdiff & 0x80000000) == 0;

    std::vector<std::vector<sdr>> lowsdrs(33);
    std::vector<std::vector<sdr>> highsdrs(33);
    unsigned lowhw = hwnaf(lowdiff);
    unsigned highhw = hwnaf(highdiff);
    lowdiff <<= rot;
    highdiff <<= 32 - rot;
    for (unsigned i = lowhw; i <= maxw - highhw; ++i)
        table_sdrs(lowsdrs[i], lowdiff, i, lowpos);
    for (unsigned i = highhw; i <= maxw - lowhw; ++i)
        table_sdrs(highsdrs[i], highdiff, i, highpos);
    for (unsigned wl = lowhw; wl <= maxw - highhw; ++wl)
        for (unsigned l0 = 0; l0 < lowsdrs[wl].size(); ++l0) {
            sdr temp0 = lowsdrs[wl][l0];
            if (temp0.hw() != wl)
                throw std::logic_error("sdr.cpp: table_sdrs: low sdr weight mismatch");
            temp0.mask >>= rot;
            temp0.sign >>= rot;
            for (unsigned wh = highhw; wh + wl <= maxw; ++wh)
                for (unsigned h0 = 0; h0 < highsdrs[wh].size(); ++h0) {
                    sdr temp1 = highsdrs[wh][h0];
                    if (temp1.hw() != wh)
                        throw std::logic_error("sdr.cpp: table_sdrs: high sdr weight mismatch");
                    temp1.mask ^= temp0.mask;
                    temp1.sign ^= temp0.sign;
                    result.push_back(temp1);
                }
        }
}

void hashclash_sdr_hpp_init() {
    static_cast<void>(sc_table());
    static_cast<void>(scn_table());
}

} // namespace hashclash
