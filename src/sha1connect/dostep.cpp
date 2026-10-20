/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <hashclash/progress_display.hpp>
#include <hashclash/console.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/saveload_bz2.hpp>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1differentialpath.hpp>

#include "main.hpp"

unsigned int lowpathindex;

void random_permutation(std::vector<sha1differentialpath> &paths, path_container &container) {
    // use a pseudo-random permutation fixed by inputs
    seed(paths.size());
    addseed(container.t);
    addseed(container.modn);
    for (unsigned i = 0; i < paths.size(); ++i) {
        unsigned k = xrng64() % paths.size();
        paths[i].swap(paths[k]);
    }
    addseed(std::random_device{}());
}

inline std::string pathsstring(const std::string &basepath, unsigned modi, unsigned modn) {
    return workdir + "/" + basepath + "_" + std::to_string(modi) + "of" + std::to_string(modn);
}

unsigned lower_eqbits(const sha1differentialpath &_Left, const sha1differentialpath &_Right) {
    unsigned t = _Left.tend() - 1;
    uint32 LdFt = 0 - _Left[t].getsdr().rotate_left(5).adddiff() -
                  _Left[t - 4].getsdr().rotate_left(30).adddiff();
    uint32 LdFtp1 = 0 - _Left[t - 3].getsdr().rotate_left(30).adddiff();
    uint32 LdFtp2 = 0 - _Left[t - 2].getsdr().rotate_left(30).adddiff();
    uint32 LdFtp3 = 0 - _Left[t - 1].getsdr().rotate_left(30).adddiff();
    uint32 LdFtp4 = 0 - _Left[t].getsdr().rotate_left(30).adddiff();
    uint32 LdQtm1 = _Left[t - 1].diff();
    uint32 LdQt = _Left[t].diff();
    uint32 RdFt = 0 - _Right[t].getsdr().rotate_left(5).adddiff() -
                  _Right[t - 4].getsdr().rotate_left(30).adddiff();
    uint32 RdFtp1 = 0 - _Right[t - 3].getsdr().rotate_left(30).adddiff();
    uint32 RdFtp2 = 0 - _Right[t - 2].getsdr().rotate_left(30).adddiff();
    uint32 RdFtp3 = 0 - _Right[t - 1].getsdr().rotate_left(30).adddiff();
    uint32 RdFtp4 = 0 - _Right[t].getsdr().rotate_left(30).adddiff();
    uint32 RdQtm1 = _Right[t - 1].diff();
    uint32 RdQt = _Right[t].diff();
    unsigned b = 0;
    while (b < 32 + 8) {
        if (b < 32) {
            if (_Left(t - 3, (b + 2) & 31) != _Right(t - 3, (b + 2) & 31))
                break;
            if (_Left(t - 2, (b + 2) & 31) != _Right(t - 2, (b + 2) & 31))
                break;
            if ((LdFt ^ RdFt) & (1 << b))
                break;
            if ((LdQtm1 ^ RdQtm1) & (1 << b))
                break;
        }
        if (b >= 2 && b - 2 < 32) {
            if ((LdFtp1 ^ RdFtp1) & (1 << (b - 2)))
                break;
            if ((LdQt ^ RdQt) & (1 << (b - 2)))
                break;
        }
        if (b >= 4 && b - 4 < 32 && ((LdFtp2 ^ RdFtp2) & (1 << (b - 4))))
            break;
        if (b >= 6 && b - 6 < 32 && ((LdFtp3 ^ RdFtp3) & (1 << (b - 6))))
            break;
        if (b >= 8 && b - 8 < 32 && ((LdFtp4 ^ RdFtp4) & (1 << (b - 8))))
            break;
        ++b;
    }
    return b;
}

std::unique_ptr<progress_display> dostep_progress;
unsigned dostep_index = 0;
struct dostep_thread {
    dostep_thread(std::vector<sha1differentialpath> &inlow,
                  std::vector<sha1differentialpath> &inhigh, path_container &out, bool randomize)
        : pathsinlow(inlow), pathsinhigh(inhigh), container(out), random(randomize) {}
    std::vector<sha1differentialpath> &pathsinlow;
    std::vector<sha1differentialpath> &pathsinhigh;
    path_container &container;
    std::unique_ptr<sha1_connect_thread> worker;
    sha1differentialpath prelowpath;
    std::vector<sha1differentialpath> lowpaths;
    bool random;
    void operator()() {
        try {
            worker = std::make_unique<sha1_connect_thread>();
            while (true) {
                unsigned count = 0;
                {
                    std::scoped_lock lock(mut);
                    if (dostep_index >= pathsinlow.size())
                        break;
                    // obtain a stripe of consecutive lowpaths
                    static constexpr unsigned stripe = 1u << 5; // must be a power of 2
                    count = stripe;
                    if (dostep_index + count >= pathsinlow.size())
                        count = unsigned(pathsinlow.size()) - dostep_index;
                    dostep_index += count;
                    (*dostep_progress) += count;
                    lowpaths.clear();
                    if (random) {
                        unsigned kr;
                        do {
                            kr = (xrng128() % pathsinlow.size());
                            kr &= ~(stripe - 1); // set kr to beginning of stripe (set necessary
                                                 // low bits to zero)
                        } while (pathsinlow[kr].path.size() == 0);
                        for (unsigned i = 0; i < count; ++i) {
                            lowpaths.push_back(pathsinlow[kr + i]);
                            pathsinlow[kr + i].clear();
                            worker->lowpathindex = kr;
                        }
                    } else {
                        unsigned kr = dostep_index - count;
                        for (unsigned i = 0; i < count; ++i) {
                            lowpaths.push_back(pathsinlow[kr + i]);
                            pathsinlow[kr + i].clear();
                            worker->lowpathindex = kr;
                        }
                    }
                }

                for (unsigned i = 0; i < lowpaths.size(); ++i, ++worker->lowpathindex) {
                    if (prelowpath.path.size())
                        worker->sha1_connect(lowpaths[i], pathsinhigh, container,
                                             lower_eqbits(lowpaths[i], prelowpath));
                    else
                        worker->sha1_connect(lowpaths[i], pathsinhigh, container, 0);
                    prelowpath.swap(lowpaths[i]);
                }
            }
        } catch (std::exception &e) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cerr << "Worker thread: caught exception:" << '\n' << e.what() << '\n';
        } catch (...) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cerr << "Worker thread: caught unknown exception:" << '\n';
        }
    }
};
void dostep_threaded(std::vector<sha1differentialpath> &inlow,
                     std::vector<sha1differentialpath> &inhigh, path_container &out,
                     bool randomize = true) {
    std::vector<std::jthread> mythreads;
    {
        std::scoped_lock lock(mut);
        dostep_index = 0;
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Starting threads..." << std::flush;
        }
        for (unsigned i = 0; i < static_cast<unsigned>(out.threads); ++i) {
            {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                std::cout << i << std::flush;
            }
            mythreads.emplace_back(dostep_thread(inlow, inhigh, out, randomize));
            {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                std::cout << i << std::flush;
            }
        }
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << '\n';
        }
        std::string tstring = "t=" + std::to_string(out.t) + ": ";
        if (tstring.size() == 5)
            tstring += " ";
        dostep_progress = std::make_unique<progress_display>(inlow.size(), true, std::cout, tstring,
                                                             "      ", "      ");
    }
    for (auto &thread : mythreads)
        thread.join();
}

void load_upper_paths(std::vector<sha1differentialpath> &pathsinhigh, path_container &container) {
    const unsigned modn = container.modn;
    const unsigned modi = container.modi;
    const unsigned mode = container.splitmode;
    // 0 = split upperpath sorted, 1 = split upperpath random, 2 = split lowerpath
    // sorted, 3 = split lowerpath random
    try {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Loading " << container.inputfilehigh << "..." << std::flush;
        }
        if (modn > 1 && mode <= 1) {
            std::vector<sha1differentialpath> tmp;
            load_bz2(tmp, binary_archive, container.inputfilehigh);
            if (mode == 1) {
                random_permutation(tmp, container);
                pathsinhigh.reserve((tmp.size() + modn) / modn);
                for (unsigned j = modi; j < tmp.size(); j += modn) {
                    pathsinhigh.push_back(tmp[j]);
                }
                std::sort(pathsinhigh.begin(), pathsinhigh.end(), diffpathupper_less());
            } else {
                std::sort(tmp.begin(), tmp.end(), diffpathupper_less());
                unsigned blockstart = (modi * tmp.size()) / modn;
                unsigned blockend = ((modi + 1) * tmp.size()) / modn;
                pathsinhigh.reserve(blockend - blockstart);
                for (unsigned k = blockstart; k < blockend; ++k)
                    pathsinhigh.push_back(tmp[k]);
            }
        } else {
            load_bz2(pathsinhigh, binary_archive, container.inputfilehigh);
            std::sort(pathsinhigh.begin(), pathsinhigh.end(), diffpathupper_less());
        }
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "done: " << pathsinhigh.size() << "." << '\n';
        }
    } catch (std::exception &e) {
        std::lock_guard<std::mutex> lock(hashclash::console_mutex);
        std::cout << "failed.\n(" << e.what() << ")" << '\n';
    } catch (...) {
        std::lock_guard<std::mutex> lock(hashclash::console_mutex);
        std::cout << "failed." << '\n';
    }
}

void load_lower_paths(std::vector<sha1differentialpath> &pathsinlow, path_container &container) {
    const unsigned modn = container.modn;
    const unsigned modi = container.modi;
    const unsigned mode = container.splitmode;
    try {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Loading " << container.inputfilelow << "..." << std::flush;
        }
        if (modn > 1 && mode >= 2) {
            std::vector<sha1differentialpath> pathstmp2;
            load_bz2(pathstmp2, binary_archive, container.inputfilelow);
            if (mode == 3) {
                random_permutation(pathstmp2, container);
                pathsinlow.reserve((pathstmp2.size() / modn) + 1);
                for (unsigned j = modi; j < pathstmp2.size(); j += modn)
                    pathsinlow.push_back(pathstmp2[j]);
                std::sort(pathsinlow.begin(), pathsinlow.end(), diffpathlower_less());
            } else {
                std::sort(pathstmp2.begin(), pathstmp2.end(), diffpathlower_less());
                unsigned blockstart = (modi * pathstmp2.size()) / modn;
                unsigned blockend = ((modi + 1) * pathstmp2.size()) / modn;
                pathsinlow.reserve(blockend - blockstart);
                for (unsigned k = blockstart; k < blockend; ++k)
                    pathsinlow.push_back(pathstmp2[k]);
            }
        } else {
            load_bz2(pathsinlow, binary_archive, container.inputfilelow);
            std::sort(pathsinlow.begin(), pathsinlow.end(), diffpathlower_less());
        }
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "done: " << pathsinlow.size() << "." << '\n';
        }
    } catch (std::exception &e) {
        std::lock_guard<std::mutex> lock(hashclash::console_mutex);
        std::cout << "failed.\n(" << e.what() << ")" << '\n';
    } catch (...) {
        std::lock_guard<std::mutex> lock(hashclash::console_mutex);
        std::cout << "failed." << '\n';
    }
}

void dostep(path_container &container) {
    const unsigned mode = container.splitmode;
    // 0 = split upperpath sorted, 1 = split upperpath random, 2 = split lowerpath
    // sorted, 3 = split lowerpath random
    std::vector<sha1differentialpath> pathsinhigh, pathsinlow, pathsout;
    if (mode <= 1) {
        load_upper_paths(pathsinhigh, container);
        load_lower_paths(pathsinlow, container);
    } else {
        load_lower_paths(pathsinlow, container);
        load_upper_paths(pathsinhigh, container);
    }
    if (container.showinputpaths) {
        for (unsigned r = 0; r < pathsinlow.size(); ++r) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            show_path(pathsinlow[r]);
        }
        for (unsigned r = 0; r < pathsinhigh.size(); ++r) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            show_path(pathsinhigh[r]);
        }
    } else {
        if (pathsinlow.size()) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            show_path(pathsinlow[0]);
        }
        if (pathsinhigh.size()) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            show_path(pathsinhigh[0]);
        }
    }
    unsigned badcount = 0;
    for (unsigned i = 0; i < pathsinlow.size(); ++i)
        if (!test_path(pathsinlow[i]))
            ++badcount;
    if (badcount) {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Bad lowerpaths: " << badcount << " out of " << pathsinlow.size() << '\n';
        }
        for (unsigned i = 0; i < pathsinlow.size(); ++i)
            if (!test_path(pathsinlow[i]) || pathsinlow[i].path.size() == 0) {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                show_path(pathsinlow[i]);
                break;
            }
    }
    badcount = 0;
    for (unsigned i = 0; i < pathsinhigh.size(); ++i)
        if (!test_path(pathsinhigh[i]))
            ++badcount;
    if (badcount) {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Bad upperpaths: " << badcount << " out of " << pathsinhigh.size() << '\n';
        }
        for (unsigned i = 0; i < pathsinhigh.size(); ++i)
            if (!test_path(pathsinhigh[i]) || pathsinhigh[i].path.size() == 0) {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                show_path(pathsinhigh[i]);
                break;
            }
    }

    std::vector<sha1differentialpath> pathsredo, pathsredohigh, pathsredolow;
    if (container.inputfileredo.size()) {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Loading redo inputs: " << std::flush;
        }
        try {
            load_bz2(pathsredo, binary_archive, container.inputfileredo);
            {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                std::cout << "done: " << pathsredo.size() << "." << '\n';
            }
        } catch (std::exception &e) {
            {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                std::cout << "failed.\n(" << e.what() << ")" << '\n';
            }
            pathsredo.clear();
        } catch (...) {
            {
                std::lock_guard<std::mutex> lock(hashclash::console_mutex);
                std::cout << "failed." << '\n';
            }
            pathsredo.clear();
        }
    }
    if (pathsredo.size() > 0) {
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Filtering..." << '\n';
        }
        progress_display pd(pathsinlow.size() + pathsinhigh.size());
        int lowtend = pathsinlow[0].tend() - 1, hightbegin = pathsinhigh[0].tbegin();
        std::vector<uint32> redolowtend(pathsredo.size()), redohightbegin(pathsredo.size());
        if (lowtend + 1 != hightbegin) {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "lowtend+1 != hightend: " << lowtend << " " << hightbegin << '\n' << '\n';
        }
        for (unsigned i = 0; i < pathsredo.size(); ++i) {
            redolowtend[i] = pathsredo[i][lowtend].diff();
            redohightbegin[i] = pathsredo[i][hightbegin].diff();
        }
        for (unsigned k = 0; k < pathsinlow.size(); ++k, ++pd) {
            bool redolow = false;
            uint32 tmp = pathsinlow[k][pathsinlow[k].tend() - 1].diff();
            for (unsigned j = 0; j < pathsredo.size(); ++j) {
                if (pathsinlow[k].tend() - 1 != lowtend)
                    throw std::runtime_error("tend()-1 != lowtend");
                if (tmp != redolowtend[j])
                    continue;
                //				if (pathsredo[j].tbegin() >
                // pathsinlow[k].tbegin() || pathsredo[j].tend() < pathsinlow[k].tend())
                //					continue;
                bool ok = true;
                /*				for (int t = pathsinlow[k].tend()-2; t
                   >= pathsinlow[k].tbegin(); --t) if (pathsinlow[k][t].diff() !=
                   pathsredo[j][t].diff()) { ok = false; break;
                                                        }
                */
                if (ok) {
                    redolow = true;
                    break;
                }
            }
            if (redolow) {
                pathsredolow.push_back(pathsinlow[k]);
            }
        }
        for (unsigned k = 0; k < pathsinhigh.size(); ++k, ++pd) {
            bool redohigh = false;
            uint32 tmp = pathsinhigh[k][pathsinhigh[k].tbegin()].diff();
            for (unsigned j = 0; j < pathsredo.size(); ++j) {
                if (pathsinhigh[k].tbegin() != hightbegin)
                    throw std::runtime_error("pathsinhigh[k].tbegin() != hightbegin");
                if (tmp != redohightbegin[j])
                    continue;
                //				if (pathsredo[j].tbegin() >
                // pathsinhigh[k].tbegin() || pathsredo[j].tend() <
                // pathsinhigh[k].tend()) 					throw
                // std::runtime_error("blaaat");
                // continue;
                bool ok = true;
                /*				for (int t = pathsinhigh[k].tbegin()+1;
                   t < pathsinhigh[k].tend(); ++t) if (pathsinhigh[k][t].diff() !=
                   pathsredo[j][t].diff()) { ok = false; break;
                                                        }
                */
                if (ok) {
                    redohigh = true;
                    break;
                }
            }
            if (redohigh) {
                pathsredohigh.push_back(pathsinhigh[k]);
            }
        }
        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "# Lowerpaths to redo: " << pathsredolow.size() << '\n';
            std::cout << "# Upperpaths to redo: " << pathsredohigh.size() << '\n';
            std::cout << "Processing selected lowerpaths..." << '\n';
        }
        dostep_threaded(pathsredolow, pathsinhigh, container, false);
        container.save_bestpaths();

        {
            std::lock_guard<std::mutex> lock(hashclash::console_mutex);
            std::cout << "Processing selected upperpaths..." << '\n';
        }
        dostep_threaded(pathsinlow, pathsredohigh, container, false);
        container.save_bestpaths();
        return;
    }
    if (container.loworder == 0)
        dostep_threaded(pathsinlow, pathsinhigh, container, true);
    else {
        if (container.loworder == 2)
            std::sort(pathsinlow.begin(), pathsinlow.end(), diffpathlower_less_weight());
        dostep_threaded(pathsinlow, pathsinhigh, container, false);
    }
    container.save_bestpaths();
}
