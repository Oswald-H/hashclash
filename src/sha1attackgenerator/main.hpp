/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>
#include <vector>
#include <string>

#include <filesystem>

#include <hashclash/sdr.hpp>
#include <hashclash/saveload_bz2.hpp>
#include <hashclash/sha1differentialpath.hpp>
#include <hashclash/sha1messagespace.hpp>

using namespace hashclash;

struct parameters_type {
    parameters_type() {
        usetunnelbitconditions = false;
        tunneldeepanalysis = false;
        changebitstat = false;
        mecarry = 0;
        threebittunnels = true;
        simpletunnels = false;
        mintunnelpov = -1;
    }

    std::vector<std::string> rnd234_m_bitrelationfiles;
    std::string rnd234_pathfile; // single path
    std::string rnd1_pathsfile;  // possibly multiple paths allowing choice without m bitrelation
                                 // contradictions
    unsigned tend_rnd1_me;

    unsigned mod, index;
    uint32 m_mask[80];
    bool usetunnelbitconditions;
    std::string tunnelfile;
    unsigned mecarry;
    bool tunneldeepanalysis;
    bool changebitstat;
    bool tunnelfilterbitcondition;
    bool threebittunnels;
    bool simpletunnels;
    int mintunnelpov;
    void show_mdiffs() {}
};
extern std::string workdir;
extern sha1differentialpath maindiffpath;
extern sha1messagespace mainmespace;
extern parameters_type mainparameters;
const int offset = 4;

unsigned load_block(std::istream &i, uint32 block[]);
void save_block(std::ostream &o, uint32 block[]);

int collisionfinding(parameters_type &parameters);

// filters.cpp
void mespace_to_pathbitrelationsmatrix(
    const sha1messagespace &mespace,
    std::vector<std::vector<std::vector<uint32>>> &pathbitrelationsmatrix);
void random_me(uint32 me[80],
               const std::vector<std::vector<std::vector<uint32>>> &pathbitrelationsmatrix);

void filter_tunnels_bitconditions(std::vector<sha1differentialpath> &tunnels,
                                  const sha1differentialpath &path);
bool filter_tunnel_bitrelations(const sha1differentialpath &tunnel,
                                const std::vector<std::vector<uint32>> &bitrels);
void filter_tunnels_bitrelations(std::vector<sha1differentialpath> &tunnels,
                                 const std::vector<std::vector<uint32>> &bitrels);

// program_generator.cpp
std::string mask2bit_to_string(const std::string &variable, const uint32 mask, const unsigned bit);
bool generate_program();

// tunnel_analysis.cpp
unsigned analyze_bc_tunnel(const sha1differentialpath &tunnel);
unsigned analyze_tunnel(const sha1differentialpath &tunnel, bool verbose = false,
                        bool deepanalyze = false);
void analyze_tunnels_diffpath(
    const sha1differentialpath &mypath, const std::vector<std::vector<uint32>> &bitrels,
    std::vector<sha1differentialpath> tunnels = std::vector<sha1differentialpath>());

// checkoppaths.cpp
void checkokpaths();

#endif // MAIN_HPP
