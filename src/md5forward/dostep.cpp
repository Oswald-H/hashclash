/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <utility>
#include <algorithm>
#include <string>
#include <iostream>
#include <memory>
#include <random>

#include <hashclash/saveload_gz.hpp>
#include <hashclash/md5detail.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/differentialpath.hpp>
#include <hashclash/progress_display.hpp>
#include <hashclash/timer.hpp>

#include "main.hpp"

using namespace hashclash;

void random_permutation(std::vector<differentialpath> &paths) {
    // use a pseudo-random permutation fixed by the number of paths
    seed(paths.size());
    for (unsigned i = 0; i < paths.size(); ++i) {
        unsigned k = xrng64() % paths.size();
        paths[i].swap(paths[k]);
    }
    addseed(std::random_device{}());
}

inline std::string pathsstring(const std::string &basepath, unsigned modi, unsigned modn) {
    return workdir + "/" + basepath + "_" + std::to_string(modi) + "of" + std::to_string(modn);
}

// map: (dQ1, dF1) => (count, Q1_set0, Q1_set1)
using step1diffs_type =
    std::map<std::pair<sdr, sdr>, std::pair<unsigned, std::pair<uint32, uint32>>>;
void generate_step1diffs(step1diffs_type &step1diffs, const path_container_autobalance &container) {
    const unsigned m = container.modn;
    const unsigned i = container.modi;
    const uint32 *m_diff = container.m_diff;

    uint32 Q1[4], Q2[4];
    Q1[0] = container.IHV1[0];
    Q1[1] = container.IHV1[3];
    Q1[2] = container.IHV1[2];
    Q1[3] = container.IHV1[1];
    Q2[0] = container.IHV2[0];
    Q2[1] = container.IHV2[3];
    Q2[2] = container.IHV2[2];
    Q2[3] = container.IHV2[1];

    step1diffs_type::iterator step1diffs_it, step1diffs_it2;
    uint32 Ta = md5_ff(Q1[3], Q1[2], Q1[1]) + Q1[0] + md5_ac[0];
    uint32 Tb = md5_ff(Q2[3], Q2[2], Q2[1]) + Q2[0] + md5_ac[0] + m_diff[0];

    uint64 endcount = uint64(1) << 32;
    unsigned k = 0;
    progress_display show_progress(endcount, true, std::cout, "t=0:  ", "      ", "      ");
    uint32 Q1a, Fa, Q1b, Fb;
    sdr dQ1, dF;
    std::pair<sdr, sdr> index;
    for (uint64 count = 0; count < endcount; ++count, ++show_progress, ++Ta, ++Tb) {
        ++k;
        if (k == m)
            k = 0;
        if (k != i)
            continue;

        Q1a = rotate_left(Ta, 7) + Q1[Qoff + 0];
        Fa = md5_ff(Q1a, Q1[Qoff + 0], Q1[Qoff - 1]);
        Q1b = rotate_left(Tb, 7) + Q2[Qoff + 0];
        Fb = md5_ff(Q1b, Q2[Qoff + 0], Q2[Qoff - 1]);
        dQ1.set(Q1a, Q1b);
        dF.set(Fa, Fb);

        index.first = dQ1;
        index.second = dF;
        step1diffs_it = step1diffs.find(index);
        if (step1diffs_it == step1diffs.end()) {
            std::pair<unsigned, std::pair<uint32, uint32>> tmp;
            tmp.first = 1;
            tmp.second.first = Q1a;
            tmp.second.second = Q1a;
            step1diffs[index] = tmp;
        } else {
            ++step1diffs_it->second.first;
            step1diffs_it->second.second.first |= Q1a;
            step1diffs_it->second.second.second &= Q1a;
        }
    }
}

void combine_step1diffs(step1diffs_type &global_step1diffs, const step1diffs_type &step1diffs) {
    step1diffs_type::const_iterator step1diffs_it;
    step1diffs_type::iterator step1diffs_it2;
    for (const auto &[key, value] : step1diffs) {
        step1diffs_it2 = global_step1diffs.find(key);
        if (step1diffs_it2 == global_step1diffs.end())
            global_step1diffs.insert({key, value});
        else {
            step1diffs_it2->second.first += value.first;
            step1diffs_it2->second.second.first |= value.second.first;
            step1diffs_it2->second.second.second &= value.second.second;
        }
    }
}

void dostep0(const path_container_autobalance &container) {
    // map: (dQ1, dF1) => (count, Q1_set0, Q1_set1)
    std::map<std::pair<sdr, sdr>, std::pair<unsigned, std::pair<uint32, uint32>>> step1diffs;
    std::map<std::pair<sdr, sdr>, std::pair<unsigned, std::pair<uint32, uint32>>>::iterator
        step1diffs_it,
        step1diffs_it2;

    std::cout << "Searching first step differentials: " << '\n';
    generate_step1diffs(step1diffs, container);
    hashclash::save_gz(step1diffs, pathsstring("paths0", container.modi, container.modn),
                       binary_archive);
    std::cout << "Saved " << step1diffs.size() << " first step differentials." << '\n';
}

void dostep1(const path_container_autobalance &container) {
    const unsigned modn = container.modn;
    const unsigned modi = container.modi;

    std::map<std::pair<sdr, sdr>, std::pair<unsigned, std::pair<uint32, uint32>>> step1diffs,
        step1diffs2;
    std::map<std::pair<sdr, sdr>, std::pair<unsigned, std::pair<uint32, uint32>>>::iterator
        step1diffs_it,
        step1diffs_it2;

    if (modi != 0)
        return;
    for (unsigned j = 0; j < modn; ++j) {
        std::cout << "Loading " << pathsstring("paths0", j, modn) << "..." << std::flush;
        hashclash::load_gz(step1diffs2, pathsstring("paths0", j, modn), binary_archive);
        std::cout << "done." << '\n';
        combine_step1diffs(step1diffs, step1diffs2);
    }

    unsigned count_max = 0;

    step1diffs_it2 = step1diffs.begin();
    std::vector<unsigned> countvec;
    for (const auto &[key, value] : step1diffs) {
        countvec.push_back(value.first);
        if (value.first > count_max)
            count_max = value.first;
    }
    std::sort(countvec.begin(), countvec.end());
    int countindex = int(countvec.size()) - int(65536);
    if (countindex < 0)
        countindex = 0;
    unsigned count_min = countvec[countindex];

    uint32 Q1[4], Q2[4];
    Q1[0] = container.IHV1[0];
    Q1[1] = container.IHV1[3];
    Q1[2] = container.IHV1[2];
    Q1[3] = container.IHV1[1];
    Q2[0] = container.IHV2[0];
    Q2[1] = container.IHV2[3];
    Q2[2] = container.IHV2[2];
    Q2[3] = container.IHV2[1];
    differentialpath tmp;
    for (int i = -3; i <= 0; ++i)
        tmp[i].set(Q2[3 + i] - Q1[3 + i], Q1[3 + i], Q1[3 + i]);

    std::vector<differentialpath> goodpaths;
    for (const auto &[key, value] : step1diffs) {
        if (value.first >= count_min) {
            uint32 dQ1 = key.first.adddiff();
            uint32 dF1 = key.second.adddiff();
            uint32 dT1 = dF1 + (Q2[Qoff - 2] - Q1[Qoff - 2]) + container.m_diff[1];
            uint32 dR1 = best_rotated_difference(dT1, 12);
            uint32 dQ2 = dQ1 + dR1;

            tmp[1].set(dQ1, value.second.first, value.second.second);
            tmp[2] = naf(dQ2);
            goodpaths.push_back(tmp);
        }
    }
    std::cout << "Saving " << goodpaths.size() << " paths..." << std::flush;
    save_gz(goodpaths, pathsstring("paths1", modi, modn), binary_archive);
    std::cout << "done." << '\n';
    if (goodpaths.size() > 0)
        show_path(goodpaths[0], container.m_diff);
}

std::unique_ptr<progress_display> dostep_progress;
unsigned dostep_index = 0;
struct dostep_thread {
    dostep_thread(std::vector<differentialpath> &in, path_container_autobalance &out)
        : pathsin(in), container(out) {}
    std::vector<differentialpath> &pathsin;
    path_container_autobalance &container;
    md5_forward_thread worker;
    void operator()() {
        try {
            while (true) {
                unsigned i, iend;
                {
                    std::lock_guard<std::mutex> lock(mut);
                    i = dostep_index;
                    if (i >= pathsin.size())
                        break;
                    unsigned cnt = std::clamp<unsigned>((pathsin.size() - i) / 128, 1u, 16u);
                    iend = i + cnt;

                    if (cnt != 0)
                        (*dostep_progress) += cnt;
                    dostep_index = iend;
                }

                for (; i < iend; ++i)
                    worker.md5_forward_differential_step(pathsin[i], container);
            }
            if (container.main_container != nullptr) {
                if (container.estimatefactor != 0)
                    container.estimate_flush_main();
                else
                    container.push_back_flush_main();
            }
        } catch (std::exception &e) {
            std::cerr << "Worker thread: caught exception:" << '\n' << e.what() << '\n';
        } catch (...) {
            std::cerr << "Worker thread: caught unknown exception!" << '\n';
        }
    }
};
void dostep_threaded(std::vector<differentialpath> &in, path_container_autobalance &out) {
    dostep_index = 0;
    std::string tstring = "t=" + std::to_string(out.t) + ": ";
    if (tstring.size() == 5)
        tstring += " ";
    if (out.estimatefactor)
        dostep_progress = std::make_unique<progress_display>(in.size(), true, std::cout, tstring,
                                                             "      ", "e     ");
    else
        dostep_progress = std::make_unique<progress_display>(in.size(), true, std::cout, tstring,
                                                             "      ", "      ");

    std::vector<path_container_autobalance> helpcontainers(out.threads, out);
    std::vector<std::jthread> mythreads;
    for (int i = 0; i < out.threads; ++i) {
        helpcontainers[i].main_container = &out;
        mythreads.emplace_back(dostep_thread(in, helpcontainers[i]));
    }
    for (auto &thread : mythreads)
        thread.join();
    if (dostep_progress->expected_count() != dostep_progress->count())
        *dostep_progress += dostep_progress->expected_count() - dostep_progress->count();
}

std::vector<differentialpath> pathscache;
void dostep(path_container_autobalance &container, bool savetocache) {
    const unsigned t = container.t;
    const unsigned modn = container.modn;
    const unsigned modi = container.modi;

    if (t == 0 && !container.normalt01) {
        dostep0(container);
        return;
    }
    if (t == 1 && !container.normalt01) {
        dostep1(container);
        return;
    }

    std::vector<differentialpath> pathsin, pathstmp, pathsout;
    if (pathscache.size() != 0) {
        pathsin.swap(pathscache);
        random_permutation(pathsin);
    } else if (container.inputfile.size() == 0) {
        for (unsigned k = 0; k < modn; ++k) {
            try {
                std::string filename = pathsstring("paths" + std::to_string(t - 1), k, modn);
                hashclash::timer loadtime(true);
                std::cout << "Loading " << filename << "..." << std::flush;
                load_gz(pathstmp, filename, binary_archive);
                random_permutation(pathstmp);
                for (unsigned j = modi; j < pathstmp.size(); j += modn)
                    pathsin.push_back(pathstmp[j]);
                std::cout << "done: " << pathstmp.size() << " (work:" << pathsin.size() << "). ("
                          << loadtime.time() << "s)" << '\n';
            } catch (...) {
                std::cout << "failed." << '\n';
            }
        }
    } else {
        bool failed = false;
        try {
            hashclash::timer loadtime(true);
            std::cout << "Loading " << container.inputfile << "..." << std::flush;
            load_gz(pathstmp, binary_archive, container.inputfile);
            random_permutation(pathstmp);
            for (unsigned j = modi; j < pathstmp.size(); j += modn)
                pathsin.push_back(pathstmp[j]);
            std::cout << "done: " << pathsin.size() << ". (" << loadtime.time() << "s)" << '\n';
        } catch (...) {
            failed = true;
            std::cout << "failed." << '\n';
        }
        if (failed) {
            try {
                hashclash::timer loadtime(true);
                std::cout << "Loading (text) " << container.inputfile << "..." << std::flush;
                load_gz(pathstmp, text_archive, container.inputfile);
                random_permutation(pathstmp);
                for (unsigned j = modi; j < pathstmp.size(); j += modn)
                    pathsin.push_back(pathstmp[j]);
                std::cout << "done: " << pathsin.size() << ". (" << loadtime.time() << "s)" << '\n';
            } catch (...) {
                std::cout << "failed." << '\n';
            }
        }
    }
    if (container.showinputpaths)
        for (unsigned r = 0; r < pathsin.size(); ++r)
            show_path(pathsin[r], container.m_diff);

    std::string tstring = "t=" + std::to_string(t) + ": ";
    if (tstring.size() == 5)
        tstring += " ";

    if (container.estimatefactor != 0) {
        std::cout << "Estimating maxcond for upper bound "
                  << unsigned(double(container.ubound) * container.estimatefactor)
                  << " (=" << container.ubound << " * " << container.estimatefactor << ")..."
                  << '\n';
        dostep_threaded(pathsin, container);
        container.finish_estimate();
        std::cout << "Found maxcond = " << container.maxcond << '\n';
    }

    dostep_threaded(pathsin, container);

    pathstmp.swap(pathsout);
    container.export_results(pathsout);

    if (pathsout.size() > 0)
        show_path(pathsout[0], container.m_diff);
    else
        throw std::runtime_error("No valid differential paths found!");

    std::cout << "Saving " << pathsout.size() << " paths..." << std::flush;
    if (savetocache) {
        pathscache.swap(pathsout);
        std::cout << "cached." << '\n';
        return;
    }
    unsigned splitsave = container.splitsave;
    if (splitsave <= 1) {
        hashclash::timer savetime(true);
        std::string filenameout = pathsstring("paths" + std::to_string(t), modi, modn);
        save_gz(pathsout, filenameout, binary_archive);
        std::cout << "done. (" << savetime.time() << "s)" << '\n';
        return;
    }
    std::vector<std::jthread> mythreads;
    unsigned threads = std::min<unsigned>(container.threads, splitsave);
    for (unsigned j = 0; j < threads; ++j) {
        mythreads.emplace_back([t, j, splitsave, threads, &pathsout]() {
            std::vector<differentialpath> pathsouti;
            pathsouti.reserve(std::size_t(double(pathsout.size()) / double(splitsave) + 2));
            for (unsigned i = j; i < splitsave; i += threads) {
                std::string filenameout = pathsstring("paths" + std::to_string(t), i, splitsave);
                pathsouti.clear();
                for (std::size_t k = i; k < pathsout.size(); k += splitsave)
                    pathsouti.emplace_back(std::move(pathsout[k]));
                save_gz(pathsouti, filenameout, binary_archive);
                std::cout << " " << i;
            }
        });
    }
    for (auto &thread : mythreads)
        thread.join();
    std::cout << "done." << '\n';
}
