/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <hashclash/cli11_options.hpp>

#include <hashclash/rng.hpp>
#include <hashclash/sdr.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/booleanfunction.hpp>

#include "main.hpp"

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

std::string workdir;

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    try {
        path_container_autobalance container;
        std::vector<std::vector<unsigned>> msgmask(16);
        unsigned msgoffset = 0;
        bool msglcext = false;
        std::string tunnelsfilename;

        // Define program options
        po::options_description desc("Allowed options"),
            msg("Define message differences (as bitnr, bitnr=0-31)"),
            ihv("Define start IV (necessary for t=0 and t=1)"), all("Allowed options");

        desc.add_options()("help,h", "Show options.")(
            "mod,m", po::value<unsigned>(&container.modn)->default_value(1),
            "Do only 1/m of all work.")

            ("index,i", po::value<unsigned>(&container.modi)->default_value(0),
             "Do i-th part of all work (i=0,...,m-1).")

                ("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                 "Set working directory.")

                    ("inputfile,f", po::value<std::string>(&container.inputfile)->default_value(""),
                     "Use specified inputfile.")

                        ("newinputpath,n", po::bool_switch(&container.newinputpath),
                         "Start with empty path.")

                            ("showinputpaths,s", po::bool_switch(&container.showinputpaths),
                             "Show all input paths.\n")

                                ("tstep,t", po::value<unsigned>(&container.t),
                                 "Do step t (=0,...,15).")

                                    ("trange",
                                     po::value<unsigned>(&container.trange)->default_value(0),
                                     "Number of additional steps to perform.")

                                        ("maxconditions,c",
                                         po::value<unsigned>(&container.maxcond)
                                             ->default_value(2176),
                                         "Limit total amount of bitconditions.")

                                            ("condtbegin,b",
                                             po::value<int>(&container.tbegin)->default_value(-3),
                                             "Set starting Q_t for maxconditions.")

                                                ("includenaf",
                                                 po::bool_switch(&container.includenaf),
                                                 "Include naf(Q_t+1) in # conditions.")

                                                    ("halfnafweight",
                                                     po::bool_switch(&container.halfnafweight),
                                                     "naf(Q_t+1) only weighs half in # cond.")

                                                        ("maxweight,p",
                                                         po::value<unsigned>(&container.maxweight)
                                                             ->default_value(32),
                                                         "Limit carry propagation.")(
                                                            "minweight",
                                                            po::value<unsigned>(
                                                                &container.minweight)
                                                                ->default_value(0),
                                                            "Limit carry propagation.")

                                                            ("maxsdrs,q",
                                                             po::value<unsigned>(&container.maxsdrs)
                                                                 ->default_value(2000),
                                                             "Limit total number of sdrs.")

                                                                ("autobalance,a",
                                                                 po::value<unsigned>(
                                                                     &container.ubound)
                                                                     ->default_value(0),
                                                                 "Do autobalancing using target amount.")

                                                                    ("fillfraction",
                                                                     po::value<double>(
                                                                         &container.fillfraction)
                                                                         ->default_value(0),
                                                                     "Fill up to fraction of target amount\n\twith heavier paths than maxcond.")

                                                                        ("estimate,e",
                                                                         po::value<double>(
                                                                             &container
                                                                                  .estimatefactor)
                                                                             ->default_value(0),
                                                                         "Do an estimate for maxcond.")

                                                                            ("nafestimate",
                                                                             po::value<unsigned>(
                                                                                 &container
                                                                                      .nafestweight)
                                                                                 ->default_value(8),
                                                                             "Estimate of naf(Q_t+1) in # conditions.")

                                                                                ("onemessagediff",
                                                                                 po::bool_switch(
                                                                                     &container
                                                                                          .onemessagediff),
                                                                                 "Use only one messagediff dmt")(
                                                                                    "expandprevmessagediff",
                                                                                    po::bool_switch(
                                                                                        &container
                                                                                             .expandprevmessagediff),
                                                                                    "Expand previous messagediff")

                                                                                    ("tunnelconditionsfile",
                                                                                     po::value<
                                                                                         std::
                                                                                             string>(
                                                                                         &tunnelsfilename)
                                                                                         ->default_value(
                                                                                             ""),
                                                                                     "Force compatibility with tunnels");

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
        all.add(desc).add(msg);

        // Parse program options
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, all), vm);
        {
            std::ifstream ifs("sha1diffpathforward.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);

        // Process program options
        if (vm.count("help") || vm.count("tstep") == 0) {
            std::cout << desc << '\n';
            return 0;
        }
        if (container.modi >= container.modn) {
            std::cerr << "Error: i must be strictly less than m!" << '\n';
            return 1;
        }
        if (container.modn != 1) {
            std::cout << "Using set " << container.modi << " of " << container.modn << '\n';
            container.trange = 0;
        }

        if (tunnelsfilename.size()) {
            std::cout << "Loading tunnel conditions: " << tunnelsfilename << "..." << std::flush;
            bool loadedtunnels = false;
            try {
                std::vector<sha1differentialpath> tmptunnel;
                load_bz2(tmptunnel, text_archive, tunnelsfilename);
                container.tunnelconditions = tmptunnel[0];
                loadedtunnels = true;
            } catch (std::exception &e) {
                std::cout << "error: " << '\n' << e.what() << '\n';
            } catch (...) {
                std::cout << "unknown error!" << '\n';
            }
            if (loadedtunnels) {
                std::cout << "done." << '\n';
                show_path(container.tunnelconditions);
            }
        }

        uint32 me[16];
        for (unsigned k = 0; k < 16; ++k) {
            me[k] = 0;
            for (unsigned j = 0; j < msgmask[k].size(); ++j)
                me[k] |= 1 << msgmask[k][j];
        }
        sha1_me_generalised(container.m_mask, me, msgoffset);
        if (msglcext) {
            uint32 met[16];
            uint32 mex[80];
            for (unsigned i = 0; i < 16; ++i)
                met[i] = rotate_left(me[i], 5);
            sha1_me_generalised(mex, met, msgoffset + 1);
            for (unsigned i = 0; i < 80; ++i)
                container.m_mask[i] ^= mex[i];

            sha1_me_generalised(mex, me, msgoffset + 2);
            for (unsigned i = 0; i < 80; ++i)
                container.m_mask[i] ^= mex[i];

            for (unsigned i = 0; i < 16; ++i)
                met[i] = rotate_left(me[i], 30);
            for (unsigned j = 3; j < 6; ++j) {
                sha1_me_generalised(mex, met, msgoffset + j);
                for (unsigned i = 0; i < 80; ++i)
                    container.m_mask[i] ^= mex[i];
            }
        }

        // do not estimate if not autobalancing
        if (container.ubound == 0)
            container.estimatefactor = 0;

        // only estimate for larger ubound, not lower
        if (container.estimatefactor < 1)
            container.estimatefactor = 0;

        if (container.fillfraction < 0)
            container.fillfraction = 0;
        if (container.fillfraction > 1)
            container.fillfraction = 1;

        // Start job with given parameters
        container.set_parameters();
        for (unsigned tt = container.t; tt < container.t + container.trange; ++tt) {
            std::cout << "delta_m[" << tt << "] = " << sdr(0, container.m_mask[tt]) << '\n';
            path_container_autobalance containertmp = container;
            containertmp.t = tt;
            dostep(containertmp, true);
        }
        container.t += container.trange;
        std::cout << "delta_m[" << container.t << "] = " << sdr(0, container.m_mask[container.t])
                  << '\n';

        dostep(container);
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
