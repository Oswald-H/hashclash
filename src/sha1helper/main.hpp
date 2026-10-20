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

using namespace hashclash;

extern std::string workdir;

unsigned load_block(std::istream &i, uint32 block[]);
void save_block(std::ostream &o, uint32 block[]);

struct parameters_type {
    unsigned mod, index;
    uint32 m_mask[80];
    std::string infile1, infile2, outfile1, outfile2;
    int cpuaffinity;
    unsigned split;
    std::vector<std::string> files;
    bool unique;
    int filtert;
    uint64 maxstates;
    int seli;
    bool invert;

    void show_mdiffs() {}
};

int convert(parameters_type &parameters);
int split(parameters_type &parameters);
int join(parameters_type &parameters);
int pathfromtext(parameters_type &parameters);
int showmespaceconditions(parameters_type &parameters);

int filterfeasible(parameters_type &parameters);

int selectpath(parameters_type &parameters);

#endif // MAIN_HPP
