/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <cmath>
#include <iomanip>
#include <algorithm>
#include <map>
#include <fstream>
#include <stdexcept>

#include <hashclash/saveload_bz2.hpp>
#include <hashclash/sha1detail.hpp>
#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/booleanfunction.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/progress_display.hpp>
#include <hashclash/sha1messagespace.hpp>

#include "main.hpp"

sha1differentialpath maindiffpath;
sha1messagespace mainmespace;

uint32 m_diff[80];

bool throw_at_t16 = false;
uint32 firstmsg[16];

std::vector<uint32> Qtp1valsvec[30];

std::vector<std::vector<std::vector<uint32>>> bitrelations;
std::vector<std::vector<uint32>> pathbitrelations;
std::vector<std::vector<std::vector<uint32>>> pathbitrelationsmatrix;

std::vector<std::vector<uint32>> firstmsg_vec;

void filter_bitconditions(std::vector<std::vector<uint32>> &bitrels, unsigned tbegin,
                          unsigned tend) {
    sweep_matrix(bitrels, 80 * 32);
    std::vector<uint32> triv(81, 0), impos(81, 0);
    impos[80] = ~uint32(0);
    unsigned i = 0;
    while (i < bitrels.size()) {
        if (bitrels[i] == triv) {
            std::swap(bitrels[i], bitrels.back());
            bitrels.pop_back();
            continue;
        }
        if (bitrels[i] == impos)
            throw std::runtime_error("filter_bitconditions: contradicting bitrelations");

        int lastt = -1;
        for (int j = 0; static_cast<std::size_t>(j) < bitrels[i].size() && j < 80; ++j)
            if (bitrels[i][j])
                lastt = j;
        if (lastt == -1)
            throw std::runtime_error("filter_bitconditions: huh?!?");
        if (static_cast<unsigned>(lastt) < tbegin || static_cast<unsigned>(lastt) >= tend) {
            std::swap(bitrels[i], bitrels.back());
            bitrels.pop_back();
        } else
            ++i;
    }
}

int collisionfinding(parameters_type &parameters) {
    bool usetunnelbitconditions = parameters.usetunnelbitconditions;

    sha1messagespace tmpspace;
    std::vector<std::vector<uint32>> bitrels, tmpbitrel, tmpbitrel2;
    for (unsigned i = 0; i < parameters.rnd234_m_bitrelationfiles.size(); ++i) {
        try {
            std::cout << "Loading '" << parameters.rnd234_m_bitrelationfiles[i] << "'..."
                      << std::flush;
            load_bz2(tmpspace, text_archive, parameters.rnd234_m_bitrelationfiles[i]);
            std::cout << "done" << std::flush;
            tmpspace.tobitrelations_80(tmpbitrel);
            bitrels.insert(bitrels.end(), tmpbitrel.begin(), tmpbitrel.end());
            std::cout << " (" << tmpbitrel.size() << " bitrels, new total: " << bitrels.size()
                      << ")" << '\n';
        } catch (std::exception &e) {
            std::cerr << "Exception:" << '\n' << e.what() << '\n';
            return 1;
        }
    }
    std::vector<sha1differentialpath> paths;

    std::cout << "Loading round 1 paths from '" << parameters.rnd1_pathsfile << "'..."
              << std::flush;
    try {
        load_bz2(paths, text_archive, parameters.rnd1_pathsfile);
        std::cout << "done (" << paths.size() << " paths)." << '\n';
    } catch (std::exception &e) {
        std::cerr << "Exception:" << '\n' << e.what() << '\n';
        return 1;
    }
    if (paths.size() == 0)
        return 0;

    std::vector<std::vector<uint32>> bitrelsbu = bitrels;
    for (unsigned pi = 0; pi < paths.size(); ++pi) {
        try {
            bitrels = bitrelsbu;
            sha1differentialpath upperpath;
            std::cout << "Loading round 2,3,4 path from '" << parameters.rnd234_pathfile << "'..."
                      << std::flush;
            try {
                load_bz2(upperpath, text_archive, parameters.rnd234_pathfile);
                std::cout << "done." << '\n';
            } catch (std::exception &e) {
                std::cerr << "Exception:" << '\n' << e.what() << '\n';
                return 1;
            }
            for (int t = paths[pi].tbegin(); t < paths[pi].tend(); ++t)
                upperpath[t] = paths[pi][t];
            for (unsigned t = 0; t < static_cast<unsigned>(paths[pi].tend() - 1); ++t)
                upperpath.getme(t) = paths[pi].getme(t);
            cleanup_path(upperpath);
            maindiffpath = upperpath;

            bool bad = false;
            for (int t = maindiffpath.tbegin() + 4; t < maindiffpath.tend() - 1; ++t) {
                if (maindiffpath.getme(t).mask != parameters.m_mask[t]) {
                    uint32 dm = maindiffpath.getme(t).adddiff();
                    uint32 mcur = 0;
                    uint32 mmask = parameters.m_mask[t];
                    uint32 madd = (~mmask) + 1;
                    sdr msdr;
                    msdr.mask = mmask;
                    do {
                        mcur += madd;
                        mcur &= mmask;
                        msdr.sign = mcur;
                        if (msdr.adddiff() == dm) {
                            std::cout << "corrected: \t" << t << ":\t" << msdr
                                      << " == " << sdr(parameters.m_mask[t]) << '\n';
                            break;
                        }
                    } while (mcur != 0);
                    if (msdr.adddiff() == dm) {
                        maindiffpath.getme(t) = msdr;
                    } else {
                        bad = true;
                        std::cout << "failed: \t" << t << ":\t" << maindiffpath.getme(t)
                                  << " != " << sdr(parameters.m_mask[t]) << '\n';
                    }
                }
            }
            if (bad)
                return 0;
            show_path(maindiffpath);

            // remove bitrelations possibly limiting me[0],...,me[19]
            const unsigned tend_fix_me = parameters.tend_rnd1_me;
            std::cout << "Fixed me diffs for t=[0," << tend_fix_me << "): (" << bitrels.size()
                      << "=>" << std::flush;
            filter_bitconditions(bitrels, tend_fix_me, 80);
            std::cout << bitrels.size() << ")" << '\n';
            for (unsigned i = 0; i < bitrels.size(); ++i) {
                std::cout << " - ";
                bool firstone = true;
                for (unsigned t = 0; t < 80; ++t)
                    for (unsigned b = 0; b < 32; ++b)
                        if (bitrels[i][t] & (1 << b)) {
                            if (firstone)
                                firstone = false;
                            else
                                std::cout << " + ";
                            std::cout << "M[" << t << "," << b << "]";
                        }
                std::cout << " = " << (bitrels[i][80] & 1) << '\n';
            }

            tmpspace.clear();
            for (unsigned t = 0; t < 80; ++t) {
                if (t < tend_fix_me) {
                    for (unsigned b = 0; b < 31; ++b) {
                        int bit = maindiffpath.getme(t).get(b);
                        if (bit == 0)
                            tmpspace.buildbasis_addfreebit(t, b);
                        else
                            tmpspace.buildbasis_setbit(t, b, bit == -1);
                    }
                    tmpspace.buildbasis_addfreebit(t, 31);
                } else {
                    for (unsigned b = 0; b < 32; ++b)
                        tmpspace.buildbasis_addfreebit(t, b);
                }
            }
            tmpspace.tobitrelations_80(tmpbitrel);
            bitrels.insert(bitrels.end(), tmpbitrel.begin(), tmpbitrel.end());
            std::cout << "Extra me [0," << tend_fix_me << ") bitrelations: " << tmpbitrel.size()
                      << '\n';
            {
                std::cout << "Extra tunnel me bitrelations: " << '\n';
                std::ifstream ifs("tunnel_bitconditions.txt");
                read_message_bitconditions(ifs, tmpbitrel);
                for (unsigned i = 0; i < tmpbitrel.size(); ++i) {
                    bool firstone = true;
                    for (unsigned t = 0; t < 80; ++t)
                        if (tmpbitrel[i][t]) {
                            if (firstone)
                                firstone = false;
                            else
                                std::cout << "+ ";
                            std::cout << "m" << t << sdr(tmpbitrel[i][t]) << " ";
                        }
                    std::cout << "= " << (tmpbitrel[i][80] & 1) << '\n';
                }
                bitrels.insert(bitrels.end(), tmpbitrel.begin(), tmpbitrel.end());
            }
            tmpspace.frombitrelations_80(bitrels);
            mainmespace = tmpspace;
            tmpspace.tobitrelations_16(bitrels);
            std::cout << "Total bitrelations: " << bitrels.size() << '\n';
            pathbitrelations = bitrels;
            pathbitrelationsmatrix.clear();
            pathbitrelationsmatrix.resize(16);
            for (unsigned i = 0; i < 16; ++i)
                pathbitrelationsmatrix[i].resize(32);
            for (unsigned i = 0; i < pathbitrelations.size(); ++i) {
                int lastcol = -1;
                for (int col = 16 * 32 - 1; col >= 0; --col)
                    if (pathbitrelations[i][col >> 5] & (1 << (col & 31))) {
                        lastcol = col;
                        unsigned t = lastcol >> 5;
                        unsigned b = lastcol & 31;
                        pathbitrelationsmatrix[t][b] = pathbitrelations[i];
                        break;
                    }
                if (lastcol == -1)
                    throw std::logic_error(
                        "collisionfinding.cpp: collisionfinding: empty bitrelation row");
            }
            if (parameters.tunnelfile.size()) {
                std::vector<sha1differentialpath> tunnels;
                std::cout << "Loading tunnels from '" << parameters.tunnelfile << "'..."
                          << std::flush;
                try {
                    load_bz2(tunnels, text_archive, parameters.tunnelfile);
                } catch (std::exception &e) {
                    tunnels.clear();
                    std::cout << "failed:" << '\n' << e.what() << '\n';
                } catch (...) {
                    tunnels.clear();
                    std::cout << "failed." << '\n';
                }
                if (tunnels.size())
                    analyze_tunnels_diffpath(maindiffpath, bitrels, tunnels);
                return 0;
            }
            if (!usetunnelbitconditions) {
                // if not using tunnel bit conditions then we'll assume we want to analyze tunnels
                analyze_tunnels_diffpath(maindiffpath, bitrels);
                return 0;
            }
            // if we're using tunnel bit conditions we'll assume we want to generate the collision
            // finding program

            if (!generate_program())
                return 1;
            return 0;
        } catch (std::exception &e) {
            std::cerr << "c: " << e.what() << '\n';
        } catch (...) {
            std::cerr << "c: unknown exception" << '\n';
        }
    } // for (unsigned pi = 0; pi < paths.size(); ++pi)
    return 1;
}
