/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <hashclash/cli11_options.hpp>

#include "main.hpp"

#include <hashclash/sdr.hpp>
#include <hashclash/timer.hpp>

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

std::string workdir;
parameters_type mainparameters;

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    try {
        parameters_type parameters;
        std::vector<std::vector<unsigned>> msgmask(16);
        unsigned msgoffset = 0;
        bool msglcext = false;
        bool docheckokpaths = false;

        // Define program options
        po::options_description cmds("Allowed commands"), desc("Allowed options"),
            msg("Define message differences (as +bitnr and -bitnr, bitnr=1-32)"),
            all("Allowed options");

        cmds.add_options()("help,h", "Show options\n");
        desc.add_options()("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                           "Set working directory.")(
            "rnd234merel",
            po::value<std::vector<std::string>>(&parameters.rnd234_m_bitrelationfiles),
            "File with round 2,3,4 msg.exp. bitrelations.\n(Can be specified multiple times)")(
            "rnd234path", po::value<std::string>(&parameters.rnd234_pathfile),
            "File with upper path.")("rnd1paths",
                                     po::value<std::string>(&parameters.rnd1_pathsfile),
                                     "File with round 1 paths.")(
            "tend_rnd1_me", po::value<unsigned>(&parameters.tend_rnd1_me)->default_value(22),
            "Apply round 1 path msg.exp. values for t=[0,tend).\n")

            ("checkokpaths", po::bool_switch(&docheckokpaths), "Analyse ok paths from collfind.")

                ("analyzetunnels,a", po::bool_switch(&parameters.usetunnelbitconditions),
                 "Analyze tunnels instead of creating program.")(
                    "tunnel,t", po::value<std::string>(&parameters.tunnelfile), "Analyze tunnel.")(
                    "deepanalysis", po::bool_switch(&parameters.tunneldeepanalysis),
                    "Analyze tunnel strengthening.")(
                    "tunnelmecarry", po::value<unsigned>(&parameters.mecarry)->default_value(0),
                    "Tunnel analysis message carries.")("tunnelshowbitchangestat",
                                                        po::bool_switch(&parameters.changebitstat),
                                                        "Show bit change statistics.")(
                    "mintunnelpov", po::value<int>(&parameters.mintunnelpov)->default_value(-1),
                    "Only  show bitchangestats when POV>=bound.")(
                    "tunnelfilterbitcondition",
                    po::bool_switch(&parameters.tunnelfilterbitcondition),
                    "Only analyze tunnels with compatible b.c.")(
                    "threebittunnels", po::bool_switch(&parameters.threebittunnels),
                    "Beside 1,2 bit tunnels do 3 bit tunnels.")(
                    "simpletunnels", po::bool_switch(&parameters.simpletunnels),
                    "Only consider simple ^+01 tunnels.");

        msg.add_options()("diffm0", po::value<std::vector<unsigned>>(&msgmask[0]), "mask m0")(
            "diffm1", po::value<std::vector<unsigned>>(&msgmask[1]),
            "mask m1")("diffm2", po::value<std::vector<unsigned>>(&msgmask[2]), "mask m2")(
            "diffm3", po::value<std::vector<unsigned>>(&msgmask[3]),
            "mask m3")("diffm4", po::value<std::vector<unsigned>>(&msgmask[4]), "mask m4")(
            "diffm5", po::value<std::vector<unsigned>>(&msgmask[5]),
            "mask m5")("diffm6", po::value<std::vector<unsigned>>(&msgmask[6]), "mask m6")(
            "diffm7", po::value<std::vector<unsigned>>(&msgmask[7]),
            "mask m7")("diffm8", po::value<std::vector<unsigned>>(&msgmask[8]), "mask m8")(
            "diffm9", po::value<std::vector<unsigned>>(&msgmask[9]),
            "mask m9")("diffm10", po::value<std::vector<unsigned>>(&msgmask[10]), "mask m10")(
            "diffm11", po::value<std::vector<unsigned>>(&msgmask[11]),
            "mask m11")("diffm12", po::value<std::vector<unsigned>>(&msgmask[12]), "mask m12")(
            "diffm13", po::value<std::vector<unsigned>>(&msgmask[13]),
            "mask m13")("diffm14", po::value<std::vector<unsigned>>(&msgmask[14]), "mask m14")(
            "diffm15", po::value<std::vector<unsigned>>(&msgmask[15]),
            "mask m15")("diffoffset", po::value<unsigned>(&msgoffset), "mask offset")(
            "difflcext", po::bool_switch(&msglcext), "extend to local coll.");
        all.add(cmds).add(desc).add(msg);

        // Parse program options
        po::positional_options_description p;
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);
        {
            std::ifstream ifs("sha1diffpathcollfind.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);

        // Process program options
        if (vm.count("help")) {
            std::cout << cmds << desc << '\n';
            return 0;
        }
        parameters.usetunnelbitconditions = !parameters.usetunnelbitconditions;

        uint32 me[16];
        for (unsigned k = 0; k < 16; ++k) {
            me[k] = 0;
            for (unsigned j = 0; j < msgmask[k].size(); ++j)
                me[k] |= 1 << msgmask[k][j];
        }
        sha1_me_generalised(parameters.m_mask, me, msgoffset);
        if (msglcext) {
            uint32 met[16];
            uint32 mex[80];
            for (unsigned i = 0; i < 16; ++i)
                met[i] = rotate_left(me[i], 5);
            sha1_me_generalised(mex, met, msgoffset + 1);
            for (unsigned i = 0; i < 80; ++i)
                parameters.m_mask[i] ^= mex[i];

            sha1_me_generalised(mex, me, msgoffset + 2);
            for (unsigned i = 0; i < 80; ++i)
                parameters.m_mask[i] ^= mex[i];

            for (unsigned i = 0; i < 16; ++i)
                met[i] = rotate_left(me[i], 30);
            for (unsigned j = 3; j < 6; ++j) {
                sha1_me_generalised(mex, met, msgoffset + j);
                for (unsigned i = 0; i < 80; ++i)
                    parameters.m_mask[i] ^= mex[i];
            }
        }

        mainparameters = parameters;
        if (docheckokpaths) {
            checkokpaths();
            return 0;
        }
        return collisionfinding(parameters);
        // Start job with given parameters
    } catch (std::exception &e) {
        std::cout << "Runtime: " << runtime.time() << '\n';
        std::cerr << "Caught exception!!:" << '\n' << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cout << "Runtime: " << runtime.time() << '\n';
        std::cerr << "Unknown exception caught!!" << '\n';
        return 1;
    }
    std::cout << "Runtime: " << runtime.time() << '\n';
    return 0;
}

unsigned load_block(std::istream &i, uint32 block[]) {
    for (unsigned k = 0; k < 16; ++k)
        block[k] = 0;

    unsigned len = 0;
    char uc;
    for (unsigned k = 0; k < 16; ++k)
        for (unsigned c = 0; c < 4; ++c) {
            i.get(uc);
            if (i) {
                ++len;
                block[k] += uint32((unsigned char)(uc)) << ((3 - c) * 8);
            } else {
                i.putback(uc);
                i.setstate(std::ios::failbit);
                return len;
            }
        }
    return len;
}

void save_block(std::ostream &o, uint32 block[]) {
    for (unsigned k = 0; k < 16; ++k)
        for (unsigned c = 0; c < 4; ++c)
            o << (unsigned char)((block[k] >> ((3 - c) * 8)) & 0xFF);
}
