/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_SIGNED_DIGIT_REPRESENTATION_HPP
#define HASHCLASH_SIGNED_DIGIT_REPRESENTATION_HPP

#include <vector>
#include <ostream>
#include <functional>

#include <bit>
#include <compare>

#include <cereal/cereal.hpp>

#include "types.hpp"

namespace hashclash {

// A class that holds a binary Signed Digit Representation (SDR) (modulo 2^32):
class sdr;
// calculate hamming weight:
inline unsigned hw(uint32 n);
//	inline unsigned hw(unsigned n) { return hw(uint32(n)); }
inline unsigned hw(int n) { return hw(uint32(n)); }
inline unsigned hw(uint64 n) { return hw(uint32(n >> 32)) + hw(uint32(n)); }
// calculate hamming weight of the NAF of n - is the lowest hamming weight among all SDR's:
inline unsigned hwnaf(uint32 n);
// calculate the NAF of n:
sdr naf(uint32 n);

// when rotating a difference, 4 possible differences exist after rotation. Use this function to
// find them: returns possible differences with probability >= than minprob/2^32
void rotate_difference(uint32 diff, int rc, std::vector<uint32> &rotateddiff,
                       uint32 minprob = uint32(1) << 30);
void rotate_difference(uint32 diff, int rc, std::vector<std::pair<uint32, double>> &rotateddiff);
uint32 best_rotated_difference(uint32 diff, int rc);

class sdr {
  public:
    /**** constructors ****/
    sdr() : mask(0), sign(0) {}
    sdr(uint32 n) : mask(n), sign(n) {}
    sdr(uint32 n1, uint32 n2) : mask(n1 ^ n2), sign(n2 & ~n1) {}
    sdr(const sdr &r) : mask(r.mask), sign(r.sign) {}

    /**** assign functions/operators ****/
    sdr &set(const sdr &r) {
        mask = r.mask;
        sign = r.sign;
        return *this;
    }
    sdr &set(uint32 n) {
        mask = sign = n;
        return *this;
    }
    sdr &set(uint32 n1, uint32 n2) {
        mask = n1 ^ n2;
        sign = n2 & ~n1;
        return *this;
    }
    sdr &operator=(const sdr &r) { return set(r); }
    sdr &operator=(const uint32 n) { return set(n); }
    sdr &clear() { return set(0); }

    /**** compare operators ****/
    auto operator<=>(const sdr &r) const = default;
    bool operator==(const sdr &r) const = default;

    /**** arithmetic operators ****/
    sdr operator-() const {
        sdr tmp(*this);
        tmp.sign ^= tmp.mask;
        return tmp;
    }

    sdr &operator+=(const sdr &r) {
        uint32 set1 = set1conditions() + r.set1conditions();
        return set(set1, set1 + adddiff() + r.adddiff());
    }
    sdr operator+(const sdr &r) const { return sdr(*this) += r; }

    sdr &operator-=(const sdr &r) {
        uint32 set1 = set1conditions() + r.sign;
        return set(set1, set1 + adddiff() - r.adddiff());
    }
    sdr operator-(const sdr &r) const { return sdr(*this) -= r; }

    sdr &operator^=(const sdr &r) {
        mask ^= r.mask;
        sign = mask & ~(sign ^ r.sign);
        return *this;
    }
    sdr operator^(const sdr &r) const { return sdr(*this) ^= r; }

    sdr &operator<<=(unsigned n) {
        mask <<= n;
        sign <<= n;
        return *this;
    }
    sdr operator<<(unsigned n) const { return sdr(*this) <<= n; }

    sdr &operator>>=(unsigned n) {
        mask >>= n;
        sign >>= n;
        return *this;
    }
    sdr operator>>(unsigned n) const { return sdr(*this) >>= n; }

    sdr rotate_left(unsigned n) const {
        sdr tmp(*this);
        tmp.mask = hashclash::rotate_left(mask, n);
        tmp.sign = hashclash::rotate_left(sign, n);
        return tmp;
    }
    sdr rotate_right(unsigned n) const {
        sdr tmp(*this);
        tmp.mask = hashclash::rotate_right(mask, n);
        tmp.sign = hashclash::rotate_right(sign, n);
        return tmp;
    }

    /**** other functions ****/
    uint32 adddiff() const { return sign - (sign ^ mask); }
    uint32 xordiff() const { return mask; }
    uint32 set0conditions() const { return ~sign; }
    uint32 set1conditions() const { return sign ^ mask; }

    int get(unsigned b) const { return int((sign >> b) & 1) - int(((sign ^ mask) >> b) & 1); }
    int operator[](unsigned b) const { return get(b); }

    unsigned hw() const { return hashclash::hw(mask); }
    unsigned hwnaf() const { return hashclash::hwnaf(adddiff()); }
    sdr naf() const { return hashclash::naf(adddiff()); }

    /**** members ****/
    uint32 mask, sign;
};

inline void swap(sdr &l, sdr &r) {
    std::swap(l.mask, r.mask);
    std::swap(l.sign, r.sign);
}

std::ostream &operator<<(std::ostream &o, const sdr &n);
std::istream &operator>>(std::istream &i, sdr &n);

inline unsigned hw(uint32 n) { return static_cast<unsigned>(std::popcount(n)); }

inline unsigned hwnaf(uint32 n) {
    uint32 a = n >> 1;
    uint32 w = a ^ (n + a);
    return hw(w);
}

inline sdr naf(uint32 n) {
    uint32 a = n >> 1;
    return sdr(a, a + n);
}

inline unsigned hw(const sdr &n) { return n.hw(); }
inline unsigned hwnaf(const sdr &n) { return n.hwnaf(); }
inline sdr naf(const sdr &n) { return n.naf(); }

unsigned count_sdrs(uint32 n, unsigned maxweight = 32);
void table_sdrs(std::vector<sdr> &result, uint32 n, unsigned maxweight);

unsigned count_sdrs(uint32 n, unsigned weight, bool signpos);
void table_sdrs(std::vector<sdr> &result, uint32 n, unsigned weight, bool signpos);

unsigned count_sdrs(sdr n, unsigned maxweight, unsigned rot);
void table_sdrs(std::vector<sdr> &result, sdr n, unsigned maxweight, unsigned rot);

// Optional: forces the SDR lookup tables to be built eagerly. hw()/hwnaf() no
// longer depend on global construction order (std::popcount), and the tables
// are built lazily on first use, so calling this is only needed when the first
// use must happen outside of a normal function (e.g. in a global constructor).
void hashclash_sdr_hpp_init();

} // namespace hashclash

namespace cereal {

template <class Archive> void serialize(Archive &ar, hashclash::sdr &d) {
    ar(make_nvp("mask", d.mask), make_nvp("sign", d.sign));
}

} // namespace cereal

#endif // HASHCLASH_SIGNED_DIGIT_REPRESENTATION_HPP
