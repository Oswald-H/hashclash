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
#include <hashclash/booleanfunction.hpp>

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

std::mutex mut;
std::string workdir;

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    std::cout << "Connect MD5 differential paths\n"
                 "Copyright (C) 2009 Marc Stevens\n"
                 "http://homepages.cwi.nl/~stevens/\n"
              << '\n';

    try {
        path_container container;
        std::vector<std::vector<int>> msgdiff(16);

        // Define program options
        po::options_description desc("Allowed options"),
            msg("Define message differences (as +bitnr and -bitnr, bitnr=1-32)"),
            all("Allowed options");

        int bestmaxcomp;
        unsigned mintunnel;
        desc.add_options()("help,h", "Show options.")(
            "mod,m", po::value<unsigned>(&container.modn)->default_value(1),
            "Do only 1/m of all work.")

            ("index,i", po::value<unsigned>(&container.modi)->default_value(0),
             "Do i-th part of all work (i=0,...,m-1).")

                ("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                 "Set working directory.")

                    ("inputfilelow", po::value<std::string>(&container.inputfilelow),
                     "Use specified inputfile for lower paths.")

                        ("inputfilehigh", po::value<std::string>(&container.inputfilehigh),
                         "Use specified inputfile for upper paths.")

                            ("waitinputfile", po::bool_switch(&container.waitinputfile),
                             "Wait until input files exist.")

                                ("showinputpaths,s", po::bool_switch(&container.showinputpaths),
                                 "Show all input paths.\n")

                                    ("tstep,t", po::value<unsigned>(&container.t),
                                     "Do step t (=0,...,15).")

                                        ("noverify", po::bool_switch(&container.noverify),
                                         "Do not verify rotations etc.")

                                            ("noenhancepath",
                                             po::bool_switch(&container.noenhancepath),
                                             "Do not enhance path (rotations) for t=0-15.")

                                                ("showstatistics",
                                                 po::bool_switch(&container.showstats),
                                                 "Show intermediate statistics.")

                                                    ("Qcondstart",
                                                     po::value<int>(&container.Qcondstart)
                                                         ->default_value(18),
                                                     "Count conditions from Q_t up.")(
                                                        "maxcomplexity",
                                                        po::value<int>(&bestmaxcomp)
                                                            ->default_value(-1000),
                                                        "Maximum complexity = \n"
                                                        "  #tunnels - #Qconds\n.")

                                                        ("mintunnel",
                                                         po::value<unsigned>(&mintunnel)
                                                             ->default_value(0),
                                                         "Minimum number of tunnels.\n")

                                                            ("threads",
                                                             po::value<int>(&container.threads)
                                                                 ->default_value(-1),
                                                             "Number of worker threads.");

        msg.add_options()("diffm0", po::value<std::vector<int>>(&msgdiff[0]), "delta m0")(
            "diffm1", po::value<std::vector<int>>(&msgdiff[1]),
            "delta m1")("diffm2", po::value<std::vector<int>>(&msgdiff[2]),
                        "delta m2")("diffm3", po::value<std::vector<int>>(&msgdiff[3]), "delta m3")(
            "diffm4", po::value<std::vector<int>>(&msgdiff[4]),
            "delta m4")("diffm5", po::value<std::vector<int>>(&msgdiff[5]),
                        "delta m5")("diffm6", po::value<std::vector<int>>(&msgdiff[6]), "delta m6")(
            "diffm7", po::value<std::vector<int>>(&msgdiff[7]),
            "delta m7")("diffm8", po::value<std::vector<int>>(&msgdiff[8]),
                        "delta m8")("diffm9", po::value<std::vector<int>>(&msgdiff[9]), "delta m9")(
            "diffm10", po::value<std::vector<int>>(&msgdiff[10]),
            "delta m10")("diffm11", po::value<std::vector<int>>(&msgdiff[11]), "delta m11")(
            "diffm12", po::value<std::vector<int>>(&msgdiff[12]),
            "delta m12")("diffm13", po::value<std::vector<int>>(&msgdiff[13]), "delta m13")(
            "diffm14", po::value<std::vector<int>>(&msgdiff[14]),
            "delta m14")("diffm15", po::value<std::vector<int>>(&msgdiff[15]), "delta m15");
        all.add(desc).add(msg);

        // Parse program options
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, all), vm);
        {
            std::ifstream ifs("md5diffpathconnect.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);
        container.bestmaxcomp = bestmaxcomp;
        container.bestmaxtunnel = mintunnel;

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
        if (container.modn != 1)
            std::cout << "Using set " << container.modi << " of " << container.modn << '\n';

        for (unsigned k = 0; k < 16; ++k) {
            container.m_diff[k] = 0;
            for (unsigned j = 0; j < msgdiff[k].size(); ++j)
                if (msgdiff[k][j] > 0)
                    container.m_diff[k] += 1 << (msgdiff[k][j] - 1);
                else
                    container.m_diff[k] -= 1 << (-msgdiff[k][j] - 1);
            if (container.m_diff[k] != 0)
                std::cout << "delta_m[" << k << "] = " << naf(container.m_diff[k]) << '\n';
        }

        if (container.noverify && !container.noenhancepath) {
            container.noenhancepath = true;
            std::cout << "Option noverify implies noenhancepath." << '\n';
        }

        if (container.threads <= 0 ||
            static_cast<unsigned>(container.threads) > std::thread::hardware_concurrency())
            container.threads = std::thread::hardware_concurrency();

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
