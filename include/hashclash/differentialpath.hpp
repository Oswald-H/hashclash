/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_DIFFERENTIALPATH_HPP
#define HASHCLASH_DIFFERENTIALPATH_HPP

#include <iostream>
#include <vector>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include <cereal/cereal.hpp>

#include "types.hpp"
#include "sdr.hpp"
#include "conditions.hpp"

namespace hashclash {

class differentialpath;
void show_path(const differentialpath &path, const uint32 blockdiff[], std::ostream &o = std::cout);
double test_path(const differentialpath &path, const uint32 blockdiff[]);
double check_rotation(uint32 dR, uint32 dT, unsigned n, const wordconditions &Qt,
                      const wordconditions &Qtp1, unsigned loopcount = (1 << 10));

bool test_path_fast(const differentialpath &path, const uint32 blockdiff[], int tbegin = 0,
                    int tend = 64);
bool check_rotation_fast(uint32 dR, uint32 dT, unsigned n, const wordconditions &Qt,
                         const wordconditions &Qtp1, unsigned loopcount = (1 << 10));

// cleans up differential path to backward conditions only
void cleanup(differentialpath &path);

// requires differential path consisting of backward conditions only!
unsigned totaltunnelstrength(const differentialpath &path);

// enhance differential path with rotational bitconditions for t=0 up to t=16
void enhancepath(differentialpath &path, const uint32 blockdiff[]);

class differentialpath {
  public:
    void clear() { path.clear(); }
    int tbegin() const { return -offset; }
    int tend() const { return int(path.size()) - offset; }
    unsigned nrcond() const {
        unsigned cond = 0;
        for (int t = tbegin(); t < tend(); ++t)
            cond += path[offset + t].hw();
        return cond;
    }
    void compress() {
        // erase trivial wordconditions at the end
        while (path.size() > 0 && path[path.size() - 1] == wordconditions())
            path.pop_back();
        // erase trivial wordconditions at the begin
        while (path.size() > 0 && path[0] == wordconditions()) {
            path.erase(path.begin());
            --offset;
        }
    }

    const wordconditions &get(int t) const {
        if (offset + t < 0 || offset + t >= int(path.size()))
            throw std::out_of_range(
                "differentialpath::operator[] (int t) const: t is out of bounds");
        return path[offset + t];
    }
    wordconditions &get(int t) {
        if (path.size() == 0)
            offset = -t;
        if (offset + t < 0) {
            int toadd = -(offset + t);
            path.resize(path.size() + unsigned(toadd));
            for (unsigned k = unsigned(path.size()) - 1; k >= unsigned(toadd); --k)
                path[k] = path[k - toadd];
            for (unsigned k = 0; k < unsigned(toadd); ++k)
                path[k].clear();
            offset = -t;
        } else if (offset + t >= int(path.size()))
            path.resize(offset + t + 1);
        return path[offset + t];
    }
    wordconditions &operator[](int t) { return get(t); }
    const wordconditions &operator[](int t) const {
        if (offset + t < 0 || offset + t >= int(path.size()))
            throw std::out_of_range(
                "differentialpath::operator[] (int t) const: t is out of bounds");
        return path[offset + t];
    }
    bitcondition operator()(int t, unsigned b) const {
        if (offset + t < 0 || offset + t >= int(path.size()))
            return bc_constant;
        return path[offset + t].get(b);
    }

    void setbitcondition(int t, unsigned b, bitcondition cond) { get(t).set(b, cond); }

    void swap(differentialpath &r) {
        std::swap(offset, r.offset);
        std::swap(path, r.path);
    }

    bool operator==(const differentialpath &r) const {
        const differentialpath &l = *this;
        int tbegin = l.tbegin() < r.tbegin() ? l.tbegin() : r.tbegin();
        for (int t = tbegin; t < l.tend() || t < r.tend(); ++t) {
            if (t >= l.tbegin() && t < l.tend()) {
                if (t >= r.tbegin() && t < r.tend()) {
                    if (l[t] != r[t])
                        return false;
                } else if (l[t] != wordconditions())
                    return false;
            } else {
                if (t >= r.tbegin() && t < r.tend())
                    if (wordconditions() != r[t])
                        return false;
            }
        }
        return true;
    }
    bool operator!=(const differentialpath &r) const { return !(*this == r); }

    bool operator<(const differentialpath &r) const {
        const differentialpath &l = *this;
        int tbegin = l.tbegin() < r.tbegin() ? l.tbegin() : r.tbegin();
        for (int t = tbegin; t < l.tend() || t < r.tend(); ++t) {
            if (t >= l.tbegin() && t < l.tend()) {
                if (t >= r.tbegin() && t < r.tend()) {
                    if (l[t] < r[t])
                        return true;
                    if (l[t] > r[t])
                        return false;
                } else {
                    if (l[t] < wordconditions())
                        return true;
                    if (l[t] > wordconditions())
                        return false;
                }
            } else {
                if (t >= r.tbegin() && t < r.tend()) {
                    if (wordconditions() < r[t])
                        return true;
                    if (wordconditions() > r[t])
                        return false;
                }
            }
        }
        return false;
    }

    // private:
    int offset = 0;
    std::vector<wordconditions> path;
};

} // namespace hashclash

namespace cereal {

template <class Archive> void serialize(Archive &ar, hashclash::differentialpath &p) {
    ar(make_nvp("offset", p.offset), make_nvp("path", p.path));
}

} // namespace cereal

#endif // HASHCLASH_DIFFERENTIALPATH_HPP
