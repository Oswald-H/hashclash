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
#include <stdexcept>
#include <thread>

#include <hashclash/sdr.hpp>
#include <hashclash/saveload_gz.hpp>
#include <hashclash/differentialpath.hpp>
#include <hashclash/rng.hpp>

using namespace hashclash;

extern std::mutex mut;
extern std::string workdir;

unsigned load_block(std::istream &i, uint32 block[]);
void save_block(std::ostream &o, uint32 block[]);

struct parameters_type {
    uint32 m_diff[16];
    std::string infile1, infile2, outfile1, outfile2;
    unsigned split;
    unsigned skipnc;
    unsigned pathtyperange;
    std::vector<std::string> files;
    int threads;

    void show_mdiffs() {
        for (unsigned k = 0; k < 16; ++k)
            if (m_diff[k] != 0)
                std::cout << "delta_m[" << k << "] = " << naf(m_diff[k]) << '\n';
    }
};

int upperpaths(parameters_type &parameters);
int startnearcollision(parameters_type &parameters);
int convert(parameters_type &parameters);
int split(parameters_type &parameters);
int join(parameters_type &parameters);
int combinepaths(parameters_type &parameters);
int negatepaths(parameters_type &parameters);
int pathfromtext(parameters_type &parameters);
int pathfromcollision(parameters_type &parameters);
int collisionfinding(parameters_type &parameters);
int partialpathfromfile(parameters_type &parameters);

struct collision_found : std::runtime_error {
    collision_found() : std::runtime_error("Found collision!") {}
};

struct collisionfinding_thread {
    collisionfinding_thread() : testcounts(1 << 20, 0) {}
    localxrng rng;

    void do_step26();
    void do_step25();
    void do_step24();
    void do_step23();
    // void do_step22();
    void do_step21();
    void do_step20();
    // void do_step19();
    void do_step18();
    void do_step17();
    void do_step16();
    void filltables();
    void findcollision(const std::vector<differentialpath> &diffpaths, bool keeppath = true) {
        if (!keeppath) {
            {
                std::lock_guard<std::mutex> lock(mut);
                diffpath = diffpaths[rng() % diffpaths.size()];
            }
            filltables();
        }
        do_step16();
    }

    uint64 cpu_step_t[64];
    std::vector<uint64> testcounts /*(1<<20,0)*/;

    differentialpath diffpath;
    uint32 m_diff[16];

    std::vector<triple<uint32, uint32, uint32>> Q1Q2m0withm1ok;
    std::vector<triple<uint32, uint32, uint32>>::const_iterator Q1Q2m0it, Q1Q2m0itend;

    static const int offset = 3;
    uint32 m[16];
    uint32 m2[16];
    uint32 Q[68];
    uint32 Q2[68];

    uint32 dQ[68];
    uint32 dT[68];
    uint32 dR[68];

    uint32 Qvaluemask[68];
    uint32 Qvalue[68];
    uint32 Qprev[68];
    uint32 Qprev2[68];

    uint32 Q5m5tunnel, Q4m5tunnel, Q4m4tunnel;
    uint32 Q10m10tunnel, Q9m10tunnel, Q9m9tunnel;
    uint32 Q8Q12m15tunnel, Q14m6Q3m5tunnel, Q14Q3m14tunnel;
    bool isinfinite;
};

#endif // MAIN_HPP
