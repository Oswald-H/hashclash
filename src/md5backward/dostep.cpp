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

std::unique_ptr<progress_display> dostep_progress;
unsigned dostep_index = 0;
struct dostep_thread {
    dostep_thread(std::vector<differentialpath> &in, path_container_autobalance &out)
        : pathsin(in), container(out) {}
    std::vector<differentialpath> &pathsin;
    path_container_autobalance &container;
    md5_backward_thread worker;
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
                    worker.md5_backward_differential_step(pathsin[i], container);
            }
        } catch (std::exception &e) {
            std::cerr << "Worker thread: caught exception:" << '\n' << e.what() << '\n';
        } catch (...) {
            std::cerr << "Worker thread: caught unknown exception" << '\n';
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
    std::vector<std::jthread> mythreads;
    for (int i = 0; i < out.threads; ++i)
        mythreads.emplace_back(dostep_thread(in, out));
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

    std::vector<differentialpath> pathsin, pathstmp, pathsout;
    if (pathscache.size() != 0) {
        pathsin.swap(pathscache);
        random_permutation(pathsin);
    } else if (container.newinputpath) {
        differentialpath path;
        path.offset = -int(t) + 3;
        path.path.resize(4);
        pathsin.push_back(path);
        std::cout << "Generated 1 new path." << '\n';
    } else if (container.inputfile.size() == 0) {
        for (unsigned k = 0; k < modn; ++k) {
            try {
                std::string filename = pathsstring("paths" + std::to_string(t + 1), k, modn);
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
        //		progress_display show_progress(pathsin.size(), true, cout, tstring, "      ", "e ");
        //		for (unsigned k = 0; k < pathsin.size(); ++k,++show_progress)
        //			md5_backward_differential_step(pathsin[k], container);
        container.finish_estimate();
        std::cout << "Found maxcond = " << container.maxcond << '\n';
    }

    dostep_threaded(pathsin, container);
    //	progress_display show_progress(pathsin.size(), true, cout, tstring, "      ", "      ");
    //	for (unsigned k = 0; k < pathsin.size(); ++k,++show_progress)
    //		md5_backward_differential_step(pathsin[k], container);

    pathstmp.swap(pathsout);
    container.export_results(pathsout);

    if (pathsout.size() > 0)
        show_path(pathsout[0], container.m_diff);
    else
        throw std::runtime_error("No valid differential paths found!");
    std::string filenameout = pathsstring("paths" + std::to_string(t), modi, modn);
    hashclash::timer savetime(true);
    std::cout << "Saving " << pathsout.size() << " paths..." << std::flush;
    if (savetocache)
        pathsout.swap(pathscache);
    else
        save_gz(pathsout, filenameout, binary_archive);
    std::cout << "done. (" << savetime.time() << "s)" << '\n';
}
