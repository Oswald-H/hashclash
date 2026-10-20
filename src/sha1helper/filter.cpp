/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include <hashclash/saveload_bz2.hpp>
#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1messagespace.hpp>
#include <hashclash/booleanfunction.hpp>
#include <hashclash/progress_display.hpp>
#include <hashclash/rng.hpp>

#include "main.hpp"

inline uint32 sha1_step_round1_bw(int t, uint32 Q[]) {
    const int offset = 4;
    uint32 Ft = sha1_f1(Q[offset + t - 1], rotate_left(Q[offset + t - 2], 30),
                        rotate_left(Q[offset + t - 3], 30));
    return Q[offset + t + 1] -
           (Ft + sha1_ac[0] + rotate_left(Q[offset + t], 5) + rotate_left(Q[offset + t - 4], 30));
}

struct filter_t {
    const int Qoffset = 4;

    uint32 Q[85];

    uint32 Qcondmask[85];
    uint32 Qset1mask[85];
    uint32 Qprevmask[85];

    uint32 mcondmask[80];
    uint32 mset1mask[80];

    sha1differentialpath &path;
    explicit filter_t(sha1differentialpath &_path) : path(_path) {
        for (int i = 0; i < 80; ++i)
            mcondmask[i] = 0;
        for (int i = 0; i < 80; ++i)
            mset1mask[i] = 0;
        for (int i = 0; i < 85; ++i)
            Q[i] = 0;
        for (int i = 0; i < 85; ++i)
            Qcondmask[i] = 0;
        for (int i = 0; i < 85; ++i)
            Qset1mask[i] = 0;
        for (int i = 0; i < 85; ++i)
            Qprevmask[i] = 0;

        //		show_path(path);
        for (int t = -4; t <= 20 && t < path.tend(); ++t) {
            //			cout << "Q" << t << "\t";
            for (unsigned b = 0; b < 32; ++b) {
                bitcondition bc = path(t, b);

                if (bc != bc_constant)
                    switch (bc) {
                    case bc_one:
                    case bc_minus:
                        Qset1mask[Qoffset + t] |= uint32(1) << b;
                    case bc_zero:
                    case bc_plus:
                        Qcondmask[Qoffset + t] |= uint32(1) << b;
                        break;
                    case bc_prevn:
                        Qset1mask[Qoffset + t] |= uint32(1) << b;
                    case bc_prev:
                        Qprevmask[Qoffset + t] |= uint32(1) << b;
                        Qcondmask[Qoffset + t] |= uint32(1) << b;
                        break;
                    default:
                        std::cout << "========= " << bc << " =======" << '\n';
                        throw std::logic_error("filter.cpp: filter_t: unsupported bitcondition in "
                                               "the differential path");
                    }
                //				cout << bc;
            }

            if (t >= 0 && t < 20) {
                sdr mt = path.getme(t);
                mcondmask[t] = mt.mask;
                mset1mask[t] = mt.set1conditions();

                //				cout << mt;
            }

            //			cout << endl;
        }
    }

    // Feasibility needs one assignment, so search a single Q prefix without storing every solution.
    bool has_solution(int t, int last, uint64 &checked, uint64 maxstates) {
        if (t > last)
            return true;

        uint32 set1 = Qset1mask[Qoffset + t];
        if (t > -4)
            set1 ^= Qprevmask[Qoffset + t] & Q[Qoffset + t - 1];

        const uint32 mask = ~Qcondmask[Qoffset + t];
        uint32 v = 0;
        do {
            if (maxstates != 0 && checked == maxstates)
                throw std::runtime_error("filterfeasible reached --maxstates=" +
                                         std::to_string(maxstates) + "; no output was saved");
            ++checked;
            if (checked % 10000 == 0)
                std::cerr << "Checked " << checked << " candidate states." << '\n';

            --v;
            v &= mask;
            Q[Qoffset + t] = v ^ set1;

            if (t > 0) {
                const uint32 message = sha1_step_round1_bw(t - 1, Q);
                if (((message ^ mset1mask[t - 1]) & mcondmask[t - 1]) != 0)
                    continue;
            }
            if (has_solution(t + 1, last, checked, maxstates))
                return true;
        } while (v != 0);
        return false;
    }
};

bool filterpath(sha1differentialpath &path, parameters_type &parameters, uint64 &checked) {
    cleanup_path(path);

    filter_t filter(path);
    return filter.has_solution(-4, parameters.filtert, checked, parameters.maxstates);
}

int filterfeasible(parameters_type &parameters) {
    if (parameters.filtert < -4 || parameters.filtert > 20) {
        std::cerr << "--filtert must be between -4 and 20." << '\n';
        return 2;
    }
    std::vector<sha1differentialpath> vecpath;
    std::cout << "Loading " << parameters.infile1 << "..." << std::flush;
    try {
        load_bz2(vecpath, binary_archive, parameters.infile1);
        std::cout << "done (loaded " << vecpath.size() << " paths)." << '\n';
    } catch (...) {
        std::cout << "failed." << '\n';
        return 1;
    }
    if (parameters.unique) {
        std::sort(vecpath.begin(), vecpath.end());
        vecpath.erase(unique(vecpath.begin(), vecpath.end()), vecpath.end());
        std::cout << "Reduced to " << vecpath.size() << " unique paths." << '\n';
    }

    uint64 cnt = 0, cnt2 = 0, checked = 0;
    for (std::size_t i = 0; i < vecpath.size();) {
        if (hw(++cnt) == 1)
            std::cout << "cnt = " << cnt << '\n';
        if (filterpath(vecpath[i], parameters, checked)) {
            ++i;
            if (hw(++cnt2) == 1)
                std::cout << "cnt2 = " << cnt2 << '\n';

        } else {
            if (i != vecpath.size() - 1)
                std::swap(vecpath[i], vecpath.back());
            vecpath.pop_back();
        }
    }
    std::cout << "Differential paths remaining: " << vecpath.size() << '\n';
    if (!parameters.outfile1.empty()) {
        std::cout << "Saving " << parameters.outfile1 << "..." << std::flush;
        save_bz2(vecpath, binary_archive, parameters.outfile1);
        std::cout << "done." << '\n';
    }
    return 0;
}

int selectpath(parameters_type &parameters) {
    std::vector<sha1differentialpath> vecpath;
    std::cout << "Loading " << parameters.infile1 << "..." << std::flush;
    try {
        load_bz2(vecpath, binary_archive, parameters.infile1);
        std::cout << "done (loaded " << vecpath.size() << " paths)." << '\n';
    } catch (...) {
        std::cout << "failed." << '\n';
        return 1;
    }
    if (static_cast<std::size_t>(parameters.seli) < vecpath.size()) {
        sha1differentialpath path = vecpath[parameters.seli];
        if (parameters.invert) {
            for (int t = path.tbegin(); t < path.tend(); ++t) {
                if (t - 4 >= path.tbegin() && t + 1 < path.tend())
                    path.getme(t) = -path.getme(t);
                for (unsigned b = 0; b < 32; ++b) {
                    bitcondition bc = path(t, b);
                    if (bc == bc_plus)
                        path.setbitcondition(t, b, bc_minus);
                    else if (bc == bc_minus)
                        path.setbitcondition(t, b, bc_plus);
                }
            }
        }
        show_path(path);
        std::cout << "Saving " << parameters.outfile1 << "..." << std::flush;
        save_bz2(path, binary_archive, parameters.outfile1);
        std::cout << "done." << '\n';
        return 0;
    }
    std::cout << "Index is out of bounds." << '\n';
    return 1;
}
