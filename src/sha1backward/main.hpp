/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>
#include <vector>
#include <string>

#include <filesystem>

#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/booleanfunction.hpp>

using namespace hashclash;

extern std::string workdir;
class path_container_autobalance;
void dostep(path_container_autobalance &container, bool savetocache = false);
void sha1_backward_differential_step(const hashclash::sha1differentialpath &path,
                                     path_container_autobalance &container);

class path_container_autobalance {
  public:
    path_container_autobalance()
        : t(0), trange(0), tend(64), maxcond(2176), maxsdrs(1), maxweight(1), minweight(0),
          maxQ26upcond(1024), includenaf(false), nafestweight(0), halfnafweight(false), modn(1),
          modi(0), inputfile(), showinputpaths(false), newinputpath(false), estimatefactor(0),
          ubound(0), size(0), count(0), count_balanced(0), condcount(0), pathsout(0),
          fillfraction(0), onemessagediff(false), expandprevmessagediff(false) {
        for (unsigned k = 0; k < 80; ++k)
            m_mask[k] = 0;
    }

    ~path_container_autobalance() {
        std::cerr << "Autobalance parameters: maxcond=" << maxcond << '\n';
    }

    void set_parameters() {
        pathsout.clear();
        pathsout.resize(maxcond + 1);
        condcount.clear();
        condcount.resize(maxcond + 1);
    }

    void estimate(unsigned cond, unsigned amount) {
        if (cond > maxcond)
            return;
        if (ubound == 0)
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

    void push_back(const sha1differentialpath &path, unsigned cond = 0) {
        if (cond == 0) {
            for (int k = int(t) - 3; k <= tend && k <= 80; ++k)
                if (k < path.tend())
                    cond += path[k].hw();
            if (includenaf) {
                if (halfnafweight)
                    cond += (path[int(t) - 4].hw() >> 1);
                else
                    cond += path[int(t) - 4].hw();
            }
        }
        if (cond > maxcond)
            return;
        if (maxQ26upcond > 0) {
            unsigned cond2 = 0;
            for (int k = int(t) - 3; k < path.tend() && k <= 80; ++k)
                if (k >= 26)
                    cond2 += path[k].hw();
            if (cond2 > maxQ26upcond)
                return;
        }

        for (int k = tunnelconditions.tbegin(); k < tunnelconditions.tend(); ++k) {
            if (k >= path.tbegin() && k < path.tend())
                for (unsigned b = 0; b < 32; ++b) {
                    bitcondition bc = tunnelconditions(k, b), bcpath = path(k, b);
                    if (bcpath == bc_constant && bc != bc_prev && bc != bc_prevn)
                        continue;
                    if (bcpath == bc_plus)
                        bcpath = bc_zero;
                    else if (bcpath == bc_minus)
                        bcpath = bc_one;
                    switch (bc) {
                    case bc_constant:
                        break;
                    case bc_plus:
                        return;
                    case bc_minus:
                        return;
                    case bc_one:
                    case bc_zero:
                        if (bcpath == bc)
                            break;
                        if (bcpath == bc_one || bcpath == bc_zero)
                            return;
                        if (bcpath == bc_plus || bcpath == bc_minus)
                            return;
                        break;
                    case bc_prev: {
                        bitcondition bcpathm1 = path(k - 1, b);
                        if (bcpathm1 == bc_plus)
                            bcpathm1 = bc_zero;
                        else if (bcpathm1 == bc_minus)
                            bcpathm1 = bc_one;
                        if (bcpath == bc_prev)
                            break;
                        if (bcpath == bc_prevn)
                            return;
                        //                                              if (bcpath == bc_plus ||
                        //                                              bcpath == bc_minus ||
                        //                                              bcpathm1 == bc_plus ||
                        //                                              bcpathm1 == bc_minus)
                        //                                              return;
                        if (bcpathm1 == bc_constant)
                            break;
                        if (bcpathm1 == bc_one && bcpath == bc_zero)
                            return;
                        if (bcpathm1 == bc_zero && bcpath == bc_one)
                            return;
                        break;
                    }
                    case bc_prevn: {
                        bitcondition bcpathm1 = path(k - 1, b);
                        if (bcpathm1 == bc_plus)
                            bcpathm1 = bc_zero;
                        else if (bcpathm1 == bc_minus)
                            bcpathm1 = bc_one;
                        if (bcpath == bc_prevn)
                            break;
                        if (bcpath == bc_prev)
                            return;
                        //                                                if (bcpath == bc_plus ||
                        //                                                bcpath == bc_minus ||
                        //                                                bcpathm1 == bc_plus ||
                        //                                                bcpathm1 == bc_minus)
                        //                                                return;
                        if (bcpathm1 == bc_constant)
                            break;
                        if (bcpathm1 == bc_one && bcpath == bc_one)
                            return;
                        if (bcpathm1 == bc_zero && bcpath == bc_zero)
                            return;
                        break;
                    }
                    case bc_prev2:
                    case bc_prev2n:
                    case bc_next:
                    case bc_nextn:
                    case bc_next2:
                    case bc_next2n:
                    case bc_or2:
                    case bc_or2b:
                    case bc_max:
                        break;
                    }
                }
        }

        if (!test_path(path))
            std::cout << "!" << std::flush;

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
        if (ubound != 0 && maxcond + 1 < pathsout.size() &&
            size + pathsout[maxcond + 1].size() > ubound)
            pathsout[maxcond + 1].pop_back();
    }

    void export_results(std::vector<sha1differentialpath> &outpaths) {
        outpaths.clear();
        outpaths.reserve(ubound);
        for (unsigned k = 0; k < pathsout.size() && k <= maxcond; ++k) {
            unsigned index = unsigned(outpaths.size());
            outpaths.resize(index + pathsout[k].size());
            for (std::size_t i = 0; i < pathsout[k].size(); ++i)
                outpaths[index + i] = std::move(pathsout[k][i]);
            pathsout[k].clear();
        }
        unsigned hbound = unsigned(double(ubound) * fillfraction);
        unsigned k = maxcond + 1;
        while (outpaths.size() < hbound && k < pathsout.size()) {
            unsigned index = unsigned(outpaths.size());
            unsigned length = hbound - outpaths.size();
            if (length > pathsout[k].size())
                length = pathsout[k].size();
            outpaths.resize(index + length);
            for (std::size_t i = 0; i < length; ++i)
                outpaths[index + i] = std::move(pathsout[k][i]);
            pathsout[k].clear();
            ++k;
        }
        pathsout.clear();
    }

    uint32 m_mask[80];

    unsigned t, trange;
    int tend;
    unsigned maxcond;
    unsigned maxsdrs;
    unsigned maxweight, minweight;
    unsigned maxQ26upcond;

    bool includenaf;
    unsigned nafestweight;
    bool halfnafweight;

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
    std::vector<std::vector<sha1differentialpath>> pathsout;

    double fillfraction;

    bool onemessagediff;
    bool expandprevmessagediff;

    sha1differentialpath tunnelconditions;
};

#endif // MAIN_HPP
