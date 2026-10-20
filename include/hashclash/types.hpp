/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_TYPES_HPP
#define HASHCLASH_TYPES_HPP

#include <hashclash/config.h>

#include <algorithm>
#include <bit>
#include <compare>
#include <utility>
#include <vector>

#ifdef __CUDACC__
#pragma message("CUDA compiler detected.")
#define NOSERIALIZATION
#define FUNC_PREFIX __device__ __host__
#include <cstdint>
#else
#define FUNC_PREFIX
#include <cstdint>
#endif // __CUDACC__

// Since including serialization routines can be intrusive,
// especially if it is not used at all,
// one can disable it by defining NOSERIALIZATION
#include <cereal/cereal.hpp>

namespace hashclash {

using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
//	typedef std::uint16_t uint16;
//	typedef std::uint8_t uint8;
using int8 = std::int8_t;

FUNC_PREFIX inline uint32 rotate_right(const uint32 x, const unsigned n) { return std::rotr(x, n); }
FUNC_PREFIX inline uint32 rotate_left(const uint32 x, const unsigned n) { return std::rotl(x, n); }

/**** class triple ****/
template <class Ty1, class Ty2, class Ty3> struct triple {
    using MyType = triple<Ty1, Ty2, Ty3>;
    using first_type = Ty1;
    using second_type = Ty2;
    using third_type = Ty3;

    triple() : first(Ty1()), second(Ty2()), third(Ty3()) {}

    triple(const Ty1 &val1, const Ty2 &val2, const Ty3 &val3)
        : first(val1), second(val2), third(val3) {}

    template <class O1, class O2, class O3>
    triple(const triple<O1, O2, O3> &r) : first(r.first), second(r.second), third(r.third) {}

    void swap(MyType &r) {
        std::swap(first, r.first);
        std::swap(second, r.second);
        std::swap(third, r.third);
    }

    auto operator<=>(const triple &) const = default;
    bool operator==(const triple &) const = default;

    first_type first;
    second_type second;
    third_type third;
};

template <class Ty1, class Ty2, class Ty3>
inline triple<Ty1, Ty2, Ty3> make_triple(Ty1 v1, Ty2 v2, Ty3 v3) {
    return triple<Ty1, Ty2, Ty3>(v1, v2, v3);
}

template <class Ty1, class Ty2, class Ty3>
inline void swap(triple<Ty1, Ty2, Ty3> &l, triple<Ty1, Ty2, Ty3> &r) {
    l.swap(r);
}

template <class Ty> struct sortindices {
    uint32 index;
    const Ty *ptr;
    sortindices() : index(~0), ptr(0) {}
    sortindices(uint32 idx, const Ty &val) : index(idx), ptr(&val) {}
    bool operator<(const sortindices<Ty> &r) const { return *ptr < *r.ptr; }
    bool operator==(const sortindices<Ty> &r) const { return *ptr == *r.ptr; }
};

template <class Ty1, class Ty2>
inline void sortbyindex(std::vector<Ty2> &tosort, const std::vector<sortindices<Ty1>> &indices) {
    std::vector<Ty2> tosorttmp(indices.size());
    for (unsigned i = 0; i < indices.size(); ++i)
        std::swap(tosorttmp[i], tosort[indices[i].index]);
    std::swap(tosorttmp, tosort);
}

template <class Ty1, class Ty2>
void friendsort(std::vector<Ty1> &tosort, std::vector<Ty2> &tosortfriend) {
    std::vector<sortindices<Ty1>> indices;
    indices.reserve(tosort.size());
    for (unsigned i = 0; i < tosort.size(); ++i)
        indices.push_back(sortindices<Ty1>(i, tosort[i]));
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    sortbyindex(tosort, indices);
    sortbyindex(tosortfriend, indices);
}

} // namespace hashclash

namespace cereal {

template <class Archive, class Ty1, class Ty2, class Ty3>
void serialize(Archive &ar, hashclash::triple<Ty1, Ty2, Ty3> &t) {
    ar(make_nvp("first", t.first), make_nvp("second", t.second), make_nvp("third", t.third));
}

} // namespace cereal

#endif // HASHCLASH_TYPES_HPP
