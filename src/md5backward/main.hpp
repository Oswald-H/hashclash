/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>
#include <vector>
#include <string>

#include <filesystem>
#include <mutex>
#include <thread>

#include <hashclash/differentialpath.hpp>
#include <hashclash/booleanfunction.hpp>

using namespace hashclash;

extern std::mutex mut;
extern std::string workdir;
class path_container_autobalance;
void dostep(path_container_autobalance &container, bool savetocache = false);

struct md5_backward_thread {
    void md5_backward_differential_step(const hashclash::differentialpath &path,
                                        path_container_autobalance &container);
    differentialpath newpath;
    std::vector<sdr> sdrs;
    bitcondition Qtb[32], Qtm1b[32], Qtm2b[32];
    std::vector<unsigned> bval;
    uint32 fdiv[32];
    bf_outcome foutcomes[32];
    std::vector<std::pair<uint32, double>> rotateddiff;
};

class path_container_autobalance {
  public:
    path_container_autobalance()
        : t(0), trange(0), tend(64), maxcond(2176), maxsdrs(1), maxweight(1), minweight(0),
          maxQ26upcond(1024), includenaf(false), nafestweight(0), halfnafweight(false),
          noverify(false), verified(0), verifiedbad(0), modn(1), modi(0), inputfile(),
          showinputpaths(false), newinputpath(false), estimatefactor(0), ubound(0), size(0),
          count(0), count_balanced(0), condcount(0), pathsout(0), fillfraction(0), threads(1),
          uct(-4), ucb(-1), ucc('.') {
        if (uct < -3 || uct > 64 || ucb < 0 || ucb > 32 ||
            (ucc != '0' && ucc != '1' && ucc != '^' && ucc != '!' && ucc != 'm' && ucc != '#')) {
            uct = -4;
            ucb = -1;
            ucc = '.';
        }
        for (unsigned k = 0; k < 16; ++k)
            m_diff[k] = 0;
    }

    ~path_container_autobalance() {
        std::cerr << "Autobalance parameters: maxcond=" << maxcond << '\n';
        if (!noverify)
            std::cerr << "Verified: " << verifiedbad << " bad out of " << verified << '\n';
    }

    void set_parameters() {
        pathsout.clear();
        pathsout.resize(maxcond + 1);
        condcount.clear();
        condcount.resize(maxcond + 1);
    }

    void estimate(unsigned cond, unsigned amount) {
        if (ubound == 0)
            return;
        std::lock_guard<std::mutex> lock(mut);
        if (cond > maxcond)
            return;
        condcount[cond] += amount;
        size += amount;
        unsigned uboundf = unsigned(double(ubound) * estimatefactor);
        if (size > uboundf) {
            unsigned newsize = 0;
            unsigned k = 0;
            while (k < condcount.size() && newsize + condcount[k] <= uboundf) {
                newsize += condcount[k];
                ++k;
            }
            for (unsigned j = k + 1; j <= maxcond && j < condcount.size(); ++j)
                condcount[j] = 0;
            if (newsize != 0) {
                size = newsize;
                maxcond = k - 1;
                condcount[k] = 0;
            } else {
                size = newsize + condcount[k];
                maxcond = k;
            }
        }
    }

    // maxcond is adjusted by estimate()/autobalance() under mut; readers must
    // take a snapshot under the same lock to avoid the data race TSan found.
    unsigned get_maxcond() const {
        std::lock_guard<std::mutex> lock(mut);
        return maxcond;
    }

    void finish_estimate() {
        estimatefactor = 0;
        size = 0;
        count = 0;
        count_balanced = 0;
        if (includenaf) {
            if (halfnafweight)
                maxcond += (nafestweight >> 1);
            else
                maxcond += nafestweight;
        }
        pathsout.clear();
        pathsout.resize(maxcond + 1);
        condcount.clear();
        condcount.resize(maxcond + 1);
    }

    void push_back(const differentialpath &path, unsigned cond = 0) {
        if (!test_uc(path))
            return;

        if (cond == 0) {
            for (int k = int(t) - 2; k <= std::min(tend, path.tend() - 1) && k <= 64; ++k)
                cond += path[k].hw();
            if (includenaf) {
                if (halfnafweight)
                    cond += (path[int(t) - 3].hw() >> 1);
                else
                    cond += path[int(t) - 3].hw();
            }
        }
        {
            std::lock_guard<std::mutex> lock(mut);
            if (cond > maxcond)
                return;
        }
        if (maxQ26upcond > 0) {
            unsigned cond2 = 0;
            for (int k = int(t) - 2; k < path.tend() && k <= 64; ++k)
                if (k >= 26)
                    cond2 += path[k].hw();
            if (cond2 > maxQ26upcond)
                return;
        }

        if (!noverify) {
            if (!test_path_fast(path, m_diff, t - 3, t + 1)) {
                std::lock_guard<std::mutex> lock(mut);
                ++verified;
                ++verifiedbad;
                return;
            }
        }
        std::lock_guard<std::mutex> lock(mut);
        if (!noverify)
            ++verified;
        if (ubound == 0) {
            pathsout[cond].push_back(path);
            return;
        }
        if (pathsout[cond].size() < ubound) {
            pathsout[cond].push_back(path);

            ++size, ++count;
            autobalance();
        }
    }

    void autobalance() {
        if (size > ubound) {
            ++count_balanced;
            unsigned newsize = 0;
            unsigned k = 0;
            while (k < pathsout.size() && k <= maxcond && newsize + pathsout[k].size() <= ubound) {
                newsize += unsigned(pathsout[k].size());
                ++k;
            }
            if (newsize != 0) {
                size = newsize;
                maxcond = k - 1;
            } else {
                size = newsize + pathsout[k].size();
                maxcond = k;
            }
            for (unsigned j = maxcond + 2; j < pathsout.size(); ++j)
                pathsout[j].clear();
        }
    }

    void export_results(std::vector<differentialpath> &outpaths) const {
        outpaths.clear();
        outpaths.reserve(ubound);
        for (unsigned k = 0; k < pathsout.size() && k <= maxcond; ++k) {
            unsigned index = unsigned(outpaths.size());
            outpaths.resize(index + pathsout[k].size());
            std::copy(pathsout[k].begin(), pathsout[k].end(), outpaths.begin() + index);
        }
        unsigned hbound = unsigned(double(ubound) * fillfraction);
        unsigned k = maxcond + 1;
        while (outpaths.size() < hbound && k < pathsout.size()) {
            unsigned index = unsigned(outpaths.size());
            unsigned length = hbound - outpaths.size();
            if (length > pathsout[k].size())
                length = pathsout[k].size();
            outpaths.resize(index + length);
            std::copy(pathsout[k].begin(), pathsout[k].begin() + length, outpaths.begin() + index);
            ++k;
        }
    }

    bool test_uc(const differentialpath &path) {
        if (ucb != -1 && uct >= path.tbegin() + 1 && uct < path.tend() - 1) {
            switch (ucc) {
            default:
                throw std::runtime_error("test_uc: value ucc not allowed");
            case '0':
                return !(path(uct, ucb) == bc_zero || path(uct, ucb) == bc_plus);
            case '1':
                return !(path(uct, ucb) == bc_one || path(uct, ucb) == bc_minus);
            case '^':
                if (uct - 1 < path.tbegin() + 1)
                    return true;
                if (path(uct, ucb) == bc_prev)
                    return false;
                if (path(uct - 1, ucb) == bc_next)
                    return false;
                if ((path(uct - 1, ucb) == bc_zero || path(uct - 1, ucb) == bc_plus) &&
                    (path(uct, ucb) == bc_zero || path(uct, ucb) == bc_plus))
                    return false;
                if ((path(uct - 1, ucb) == bc_one || path(uct - 1, ucb) == bc_minus) &&
                    (path(uct, ucb) == bc_one || path(uct, ucb) == bc_minus))
                    return false;
                return true;
            case '!':
                if (uct - 1 < path.tbegin() + 1)
                    return true;
                if (path(uct, ucb) == bc_prevn)
                    return false;
                if (path(uct - 1, ucb) == bc_nextn)
                    return false;
                if ((path(uct - 1, ucb) == bc_zero || path(uct - 1, ucb) == bc_plus) &&
                    (path(uct, ucb) == bc_one || path(uct, ucb) == bc_minus))
                    return false;
                if ((path(uct - 1, ucb) == bc_one || path(uct - 1, ucb) == bc_minus) &&
                    (path(uct, ucb) == bc_zero || path(uct, ucb) == bc_plus))
                    return false;
                return true;
            case 'm':
                if (uct - 2 < path.tbegin() + 1)
                    return true;
                if (path(uct, ucb) == bc_prev2)
                    return false;
                if (path(uct - 2, ucb) == bc_next2)
                    return false;
                if ((path(uct, ucb) == bc_prev || path(uct - 1, ucb) == bc_next) &&
                    (path(uct - 2, ucb) == bc_next || path(uct - 1, ucb) == bc_prev))
                    return false;
                if ((path(uct - 2, ucb) == bc_zero || path(uct - 2, ucb) == bc_plus) &&
                    (path(uct, ucb) == bc_zero || path(uct, ucb) == bc_plus))
                    return false;
                if ((path(uct - 2, ucb) == bc_one || path(uct - 2, ucb) == bc_minus) &&
                    (path(uct, ucb) == bc_one || path(uct, ucb) == bc_minus))
                    return false;
                return true;
            case '#':
                if (uct - 2 < path.tbegin() + 1)
                    return true;
                if (path(uct, ucb) == bc_prev2n)
                    return false;
                if (path(uct - 2, ucb) == bc_next2n)
                    return false;
                if ((path(uct, ucb) == bc_prev || path(uct - 1, ucb) == bc_next) &&
                    (path(uct - 2, ucb) == bc_nextn || path(uct - 1, ucb) == bc_prevn))
                    return false;
                if ((path(uct, ucb) == bc_prevn || path(uct - 1, ucb) == bc_nextn) &&
                    (path(uct - 2, ucb) == bc_nextn || path(uct - 1, ucb) == bc_prevn))
                    return false;
                if ((path(uct - 2, ucb) == bc_zero || path(uct - 2, ucb) == bc_plus) &&
                    (path(uct, ucb) == bc_one || path(uct, ucb) == bc_minus))
                    return false;
                if ((path(uct - 2, ucb) == bc_one || path(uct - 2, ucb) == bc_minus) &&
                    (path(uct, ucb) == bc_zero || path(uct, ucb) == bc_plus))
                    return false;
                return true;
            }
        }
        return true;
    }

    uint32 m_diff[16];

    unsigned t, trange;
    int tend;
    unsigned maxcond;
    unsigned maxsdrs;
    unsigned maxweight, minweight;
    unsigned maxQ26upcond;

    bool includenaf;
    unsigned nafestweight;
    bool halfnafweight;

    bool noverify;
    unsigned verified, verifiedbad;

    unsigned modn;
    unsigned modi;
    std::string inputfile;
    bool showinputpaths;
    bool newinputpath;

    double estimatefactor;
    unsigned ubound;
    unsigned size;
    unsigned count, count_balanced;
    std::vector<unsigned> condcount;
    std::vector<std::vector<differentialpath>> pathsout;

    double fillfraction;

    int threads;

    int uct, ucb;
    char ucc;
};

#endif // MAIN_HPP
