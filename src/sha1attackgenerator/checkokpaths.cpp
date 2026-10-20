/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <cmath>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <map>
#include <fstream>
#include <stdexcept>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/booleanfunction.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/progress_display.hpp>
#include <hashclash/sha1messagespace.hpp>
#include <filesystem>
#include <mutex>
#include <thread>
using namespace hashclash;

// #define FORCE_TEND 80
// #define FORCE_TEND 27 // 24
#define FORCE_TEND (20 + 1)

#define BC_PREVR bc_or2
#define BC_PREVRN bc_or2b

std::vector<sha1differentialpath> maincase;
std::vector<sha1differentialpath> okpaths;
std::map<sha1differentialpath, std::vector<sha1differentialpath>> okpaths_index;

void show_path2(const sha1differentialpath &path, std::ostream &o = std::cout) {
    for (int t = path.tbegin(); t < path.tend(); ++t) {
        o << "Q" << t << ":\t" << path[t];
        if (t - 4 >= path.tbegin() && t + 1 < path.tend() && t >= 0 && t < 80) {
            o << path.getme(t);
            std::vector<unsigned> ambiguous, impossible;
            booleanfunction *F = 0;
            if (t < 20)
                F = &SHA1_F1_data;
            else if (t < 40)
                F = &SHA1_F2_data;
            else if (t < 60)
                F = &SHA1_F3_data;
            else
                F = &SHA1_F4_data;
            uint32 dF = 0;
            for (unsigned b = 0; b < 32; ++b) {
                bitcondition qtm3b = path(t - 3, (b + 2) & 31);
                if (qtm3b == BC_PREVR || qtm3b == BC_PREVRN)
                    qtm3b = bc_constant;
                bitcondition qtm2b = path(t - 2, (b + 2) & 31);
                if (qtm2b == BC_PREVR || qtm2b == BC_PREVRN)
                    qtm2b = bc_constant;
                bitcondition qtm1b = path(t - 1, b);
                if (qtm1b == bc_prev || qtm1b == bc_prevn)
                    qtm1b = bc_constant;
                if (qtm1b == BC_PREVR)
                    qtm1b = bc_prev;
                else if (qtm1b == BC_PREVRN)
                    qtm1b = bc_prevn;

                bf_outcome outcome = F->outcome(qtm1b, qtm2b, qtm3b);
                if (outcome.size() == 1) {
                    if (outcome[0] == bc_plus)
                        dF += 1 << b;
                    else if (outcome[0] == bc_minus)
                        dF -= 1 << b;
                } else {
                    if (outcome.size() == 0)
                        impossible.push_back(b);
                    else {
                        if (b == 31 && outcome.size() == 2 && outcome(0, 31) == outcome(1, 31)) {
                            dF += 1 << 31;
                            continue;
                        }
                        ambiguous.push_back(b);
                    }
                }
            }
            uint32 dQtp1 = dF + path.getme(t).adddiff() +
                           path[t].getsdr().rotate_left(5).adddiff() +
                           path[t - 4].getsdr().rotate_left(30).adddiff();
            if (dQtp1 == path[t + 1].diff())
                o << " ok";
            else
                o << " bad(" << naf(dQtp1 - path[t + 1].diff()) << ")";
            if (ambiguous.size() > 0) {
                o << " amb:" << ambiguous[0];
                for (unsigned i = 1; i < ambiguous.size(); ++i)
                    o << "," << ambiguous[i];
            }
            if (impossible.size() > 0) {
                o << " imp:" << impossible[0];
                for (unsigned i = 1; i < impossible.size(); ++i)
                    o << "," << impossible[i];
            }
        } else
            o << sdr();
        o << '\n';
    }
}

void cleanuppath2(sha1differentialpath &path) {
    sha1differentialpath newpath;
    for (int t = path.tbegin(); t < path.tend(); ++t) {
        if (t - 4 >= path.tbegin() && t + 1 < path.tend()) {
            newpath.getme(t) = path.getme(t);
            if (newpath.getme(t).get(31) != 0)
                newpath.getme(t).sign |= 1 << 31;
        }
        newpath[t] = path[t].getsdr();
    }
    for (int t = path.tend() - 2; t - 4 >= path.tbegin(); --t) {
        booleanfunction *F = 0;
        if (t < 20)
            F = &SHA1_F1_data;
        else if (t < 40)
            F = &SHA1_F2_data;
        else if (t < 60)
            F = &SHA1_F3_data;
        else
            F = &SHA1_F4_data;
        for (unsigned b = 0; b < 32; ++b) {
            bitcondition qtm3bo = path(t - 3, (b + 2) & 31);
            bitcondition qtm2bo = path(t - 2, (b + 2) & 31);
            bitcondition qtm1bo = path(t - 1, b);
            bitcondition qtm3b = newpath(t - 3, (b + 2) & 31);
            if (qtm3b == BC_PREVR || qtm3b == BC_PREVRN)
                qtm3b = bc_constant;
            bitcondition qtm2b = newpath(t - 2, (b + 2) & 31);
            if (qtm2b == BC_PREVR || qtm2b == BC_PREVRN)
                qtm2b = bc_constant;
            bitcondition qtm1b = newpath(t - 1, b);
            if (qtm1b == bc_prev || qtm1b == bc_prevn)
                qtm1b = bc_constant;
            if (qtm1b == BC_PREVR)
                qtm1b = bc_prev;
            else if (qtm1b == BC_PREVRN)
                qtm1b = bc_prevn;
            bf_outcome outcome_path = F->outcome(qtm1bo, qtm2bo, qtm3bo);
            if (outcome_path.size() != 1 && b < 31) {
                std::cerr << "multiple outcome " << t << " " << b << '\n';
                show_path2(path);
                throw std::runtime_error("multiple outcome t=" + std::to_string(t) +
                                         " b=" + std::to_string(b));
            }
            bf_outcome outcome = F->outcome(qtm1b, qtm2b, qtm3b);
            if (outcome.size() > 1 && b < 31) {
                bf_conditions newcond = F->backwardconditions(qtm1b, qtm2b, qtm3b, outcome_path[0]);
                if (newcond.first == bc_prev)
                    newcond.first = BC_PREVR;
                if (newcond.first == bc_prevn)
                    newcond.first = BC_PREVRN;
                newpath.setbitcondition(t - 1, b, newcond.first);
                newpath.setbitcondition(t - 2, (b + 2) & 31, newcond.second);
                newpath.setbitcondition(t - 3, (b + 2) & 31, newcond.third);
            } else if (b == 31 && outcome.size() > 1 && outcome_path[0] == bc_constant) {
                bf_conditions newcond = F->backwardconditions(qtm1b, qtm2b, qtm3b, bc_constant);
                if (newcond.first == bc_prev)
                    newcond.first = BC_PREVR;
                if (newcond.first == bc_prevn)
                    newcond.first = BC_PREVRN;
                newpath.setbitcondition(t - 1, b, newcond.first);
                newpath.setbitcondition(t - 2, (b + 2) & 31, newcond.second);
                newpath.setbitcondition(t - 3, (b + 2) & 31, newcond.third);
            } else if (b == 31 && outcome.size() > 1 && outcome_path[0] != bc_constant) {
                if (outcome.size() == 3) {
                    // request dF[b] = 0
                    bf_conditions newcond = F->backwardconditions(qtm1b, qtm2b, qtm3b, bc_constant);
                    // invert addition bitconditions
                    if (newcond.second == bc_prev)
                        newcond.second = bc_prevn;
                    if (newcond.first == bc_prev)
                        newcond.first = BC_PREVR;
                    if (newcond.first == bc_prevn)
                        newcond.first = BC_PREVRN;
                    newpath.setbitcondition(t - 1, b, newcond.first);
                    newpath.setbitcondition(t - 2, (b + 2) & 31, newcond.second);
                    newpath.setbitcondition(t - 3, (b + 2) & 31, newcond.third);
                }
            }
        }
    }
    path = newpath;
}

void loadokpaths() {
    std::cout << "Searching for files 'okpaths.*.bz2'..." << '\n';
    for (std::filesystem::directory_iterator dit("."); dit != std::filesystem::directory_iterator();
         ++dit)
        if (dit->path().filename().string().starts_with("okpaths.") &&
            dit->path().extension() == ".bz2") {
            std::string file = dit->path().filename().string();
            std::cout << "Loading '" << file << "'..." << std::flush;
            std::vector<sha1differentialpath> tmp;
            for (unsigned trycnt = 0; tmp.size() == 0 && trycnt < 2; ++trycnt) {
                try {
                    load_bz2(tmp, text_archive, dit->path());
                } catch (std::exception &e) {
                    std::cout << e.what() << '\n';
                    tmp.clear();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            okpaths.insert(okpaths.end(), tmp.begin(), tmp.end());
            std::cout << "done: " << tmp.size() << " paths (new total: " << okpaths.size() << ")"
                      << '\n';
        }
}
void index_okpaths() {
#if 1
    for (unsigned i = 0; i < okpaths.size(); ++i) {
        cleanuppath2(okpaths[i]);
        if (okpaths[i].tend() > FORCE_TEND) {
            okpaths[i].path.resize(okpaths[i].path.size() + FORCE_TEND - okpaths[i].tend());
            okpaths[i].me.resize(okpaths[i].path.size());
        }
        okpaths_index[okpaths[i]].push_back(okpaths[i]);
    }
#else
    for (unsigned i = 0; i < okpaths.size(); ++i) {
        sha1differentialpath tmp;
        for (int t = okpaths[i].tbegin(); t < okpaths[i].tend() && t < FORCE_TEND; ++t) {
            tmp[t] = okpaths[i][t].getsdr();

            if (t - 4 >= okpaths[i].tbegin() && t + 1 < okpaths[i].tend() && t + 1 < FORCE_TEND) {
                tmp.getme(t) = okpaths[i].getme(t);
                sdr me = tmp.getme(t);
                if (me.mask & (1 << 31))
                    me.sign |= 1 << 31;
                tmp.getme(t) = me;
            }
        }
        if (okpaths[i].tend() > FORCE_TEND) {
            okpaths[i].path.resize(okpaths[i].path.size() + FORCE_TEND - okpaths[i].tend());
            okpaths[i].me.resize(okpaths[i].path.size());
        }
        okpaths_index[tmp].push_back(okpaths[i]);
    }
#endif
    std::cout << "Total # cases: " << okpaths_index.size() << '\n';
    unsigned totalcasecnts = 0;
    std::vector<unsigned> casecnts;
    for (std::map<sha1differentialpath, std::vector<sha1differentialpath>>::const_iterator cit =
             okpaths_index.begin();
         cit != okpaths_index.end(); ++cit) {
        totalcasecnts += cit->second.size();
        casecnts.push_back(cit->second.size());
        if (cit->second.size() > maincase.size())
            maincase = cit->second;
    }
    std::vector<unsigned> casecntsbu = casecnts;
    unsigned cumcasecnts = 0;
    while (double(cumcasecnts) < double(totalcasecnts) * 1.0) {
        int highind = -1, highval = 0;
        for (unsigned i = 0; i < casecntsbu.size(); ++i)
            if (casecntsbu[i] > static_cast<unsigned>(highval)) {
                highind = i;
                highval = casecntsbu[i];
            }
        if (highind == -1)
            throw std::runtime_error("checkokpaths.cpp: index_okpaths: no remaining case with a "
                                     "non-zero path count");
        cumcasecnts += highval;
        casecntsbu[highind] = 0;
    }
    unsigned i = 0;
    for (std::map<sha1differentialpath, std::vector<sha1differentialpath>>::const_iterator cit =
             okpaths_index.begin();
         cit != okpaths_index.end(); ++cit, ++i) {
        if (casecntsbu[i] == 0) {
            std::cout << "Case " << i << ": # paths = " << casecnts[i] << " ("
                      << double(100 * casecnts[i]) / double(totalcasecnts) << "%)" << '\n';
            show_path2(cit->second[0]);
        }
    }
}

void checkokpaths() {
    loadokpaths();
    index_okpaths();
    return;
}
