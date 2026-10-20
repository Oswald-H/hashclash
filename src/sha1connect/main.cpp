/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <hashclash/cli11_options.hpp>

#include "main.hpp"

#include <hashclash/booleanfunction.hpp>
#include <hashclash/sdr.hpp>
#include <hashclash/timer.hpp>

using namespace hashclash;
namespace po = cli11_options;

std::mutex mut;
std::string workdir;
uint64 pbcount = 0;
int lastlowpathindex = -1;
std::vector<std::vector<unsigned>> badcounts(85, std::vector<unsigned>(32, 0));

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    try {
        path_container container;
        std::vector<std::vector<unsigned>> msgmask(16);
        unsigned msgoffset = 0;
        bool msglcext = false;
        std::string tunnelsfilename;

        // Define program options
        po::options_description desc("Allowed options"),
            msg("Define message differences (as +bitnr and -bitnr, bitnr=1-32)"),
            all("Allowed options");

        desc.add_options()("help,h", "Show options.")(
            "mod,m", po::value<unsigned>(&container.modn)->default_value(1),
            "Do only 1/m of all work.")

            ("index,i", po::value<unsigned>(&container.modi)->default_value(0),
             "Do i-th part of all work (i=0,...,m-1).")

                ("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                 "Set working directory.")

                    ("inputfilelow", po::value<std::string>(&container.inputfilelow),
                     "Use specified inputfile for lower paths.")(
                        "loworder", po::value<unsigned>(&container.loworder)->default_value(0),
                        "Order of processing low paths (0=random, 1=sorted, "
                        "2=w-sorted)")

                        ("inputfilehigh", po::value<std::string>(&container.inputfilehigh),
                         "Use specified inputfile for upper paths.")

                            ("inputfileredo", po::value<std::string>(&container.inputfileredo),
                             "Use specified inputfile to retest\nspecific lower "
                             "and upper paths.")

                                ("showinputpaths,s", po::bool_switch(&container.showinputpaths),
                                 "Show all input paths.\n")

                                    ("tstep,t", po::value<unsigned>(&container.t),
                                     "Do step t (=0,...,15).")

                                        ("Qcondstart",
                                         po::value<int>(&container.Qcondstart)->default_value(64),
                                         "Count conditions from Q_t up.")(
                                            "maxcond",
                                            po::value<unsigned>(&container.bestpathcond)
                                                ->default_value(262144),
                                            "Maximum # conds")(
                                            "keepall", po::bool_switch(&container.keepall),
                                            "Store all paths with cond <= maxcond")(
                                            "determinelowestcond",
                                            po::bool_switch(&container.determinelowestcond),
                                            "Do not generate paths, only determine "
                                            "cond.")("splitmode",
                                                     po::value<unsigned>(&container.splitmode),
                                                     "Set splitmode: 0=upperpath, "
                                                     "2=lowerpath, +1=random")

                                            ("tunnelconditionsfile",
                                             po::value<std::string>(&tunnelsfilename)
                                                 ->default_value(""),
                                             "Force compatibility with tunnels")(
                                                "threads",
                                                po::value<int>(&container.threads)
                                                    ->default_value(-1),
                                                "Number of worker threads")(
                                                "showstats", po::bool_switch(&container.showstats),
                                                "Show internal statistics for "
                                                "every thread every hour");

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
            std::ifstream ifs("sha1diffpathconnect.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);

        // Process program options
        if (vm.count("help") || vm.count("tstep") == 0) {
            std::cout << desc << '\n';
            return 0;
        }
        if (container.inputfilelow.size() == 0) {
            std::cerr << "Error: inputfilelow must be given!" << '\n';
            return 2;
        }
        if (container.inputfilehigh.size() == 0) {
            std::cerr << "Error: inputfilehigh must be given!" << '\n';
            return 2;
        }
        if (container.modi >= container.modn) {
            std::cerr << "Error: i must be strictly less than m!" << '\n';
            return 1;
        }
        std::cout << "Total bitconditions bound: " << container.bestpathcond << '\n';
        if (container.modn != 1)
            std::cout << "Using set " << container.modi << " of " << container.modn << '\n';

        if (container.threads <= 0 ||
            static_cast<unsigned>(container.threads) > std::thread::hardware_concurrency())
            container.threads = std::thread::hardware_concurrency();
        std::cout << "Using " << container.threads << " threads." << '\n';

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

        // Start job with given parameters
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
