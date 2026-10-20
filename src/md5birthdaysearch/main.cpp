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

#include <hashclash/md5detail.hpp>
#include <hashclash/timer.hpp>

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

std::string workdir;

int main(int argc, char **argv) {
    int result = 0;
    timer runtime(true);

    std::cout << "Birthday search for MD5 chosen-prefix collisions\n"
                 "Copyright (C) 2009 Marc Stevens\n"
                 "http://homepages.cwi.nl/~stevens/\n"
              << '\n';

    try {
        birthday_parameters parameters;

        // Define program options
        po::options_description desc("Allowed options"), all("Allowed options");

        desc.add_options()("help,h", "Show options.")(
            "mod,m", po::value<unsigned>(&parameters.modn)->default_value(1),
            "Do only 1/m of all work.")

            ("index,i", po::value<unsigned>(&parameters.modi)->default_value(0),
             "Do i-th part of all work (i=0,...,m-1).")

                ("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                 "Set working directory.")

                    ("inputfile1", po::value<std::string>(&parameters.inputfile1),
                     "Use specified inputfile as first message.")

                        ("inputfile2", po::value<std::string>(&parameters.inputfile2),
                         "Use specified inputfile as second message.")

                            ("outputfile1",
                             po::value<std::string>(&parameters.outputfile1)
                                 ->default_value("file1.bin"),
                             "Use specified outputfile for first message.")

                                ("outputfile2",
                                 po::value<std::string>(&parameters.outputfile2)
                                     ->default_value("file2.bin"),
                                 "Use specified outputfile for second message.")

                                    ("distribution", po::bool_switch(&parameters.distribution),
                                     "Show workdistribution.\nDepends on hybridbits and pathtyperange.")

                                        ("hybridbits",
                                         po::value<unsigned>(&parameters.hybridbits)
                                             ->default_value(0),
                                         "Set 0 for 64-bit, 32 for 96-bit birthdaying.")

                                            ("pathtyperange",
                                             po::value<unsigned>(&parameters.pathtyperange)
                                                 ->default_value(2),
                                             "Increases potential # diffs eliminated per n.c.")

                                                ("maxblocks",
                                                 po::value<unsigned>(&parameters.maxblocks)
                                                     ->default_value(9),
                                                 "Upperbound on the amount of near-collisions.")

                                                    ("logtraillength",
                                                     po::value<int>(&parameters.logpathlength)
                                                         ->default_value(-1),
                                                     "Specify avg. trail length in bits.")

                                                        ("maxmemory",
                                                         po::value<unsigned>(&parameters.maxmemory)
                                                             ->default_value(100),
                                                         "Max. memory in MB used for storing trails.")

                                                            ("memhardlimit",
                                                             po::bool_switch(
                                                                 &parameters.memhardlimit),
                                                             "Hard limit max. memory instead of average.")(
                                                                "threads",
                                                                po::value<unsigned>(
                                                                    &parameters.threads)
                                                                    ->default_value(0),
                                                                "Number of computing threads to start.")(
                                                                "generatormode",
                                                                po::bool_switch(
                                                                    &parameters.generatormode),
                                                                "Activate generator mode that saves ALL trails to disk for <mod> controllers to load.\n")(
                                                                "saveloadwait",
                                                                po::value<unsigned>(
                                                                    &parameters.saveloadwait)
                                                                    ->default_value(60),
                                                                "Number of seconds to wait between procedure calls to save/load trails");

#ifdef HAVE_CUDA
        desc.add_options()("cuda_dev_query", "Query CUDA devices")(
            "cuda_enable", po::bool_switch(&parameters.cuda_enabled), "Enable CUDA");
#else
        all.add_options()("cuda_dev_query", "Query CUDA devices")(
            "cuda_enable", po::bool_switch(&parameters.cuda_enabled), "Enable CUDA");
#endif

        all.add(desc);

        // Parse program options
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, all), vm);
        {
            std::ifstream ifs("md5birthdaysearch.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);

        // Process program options
        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }
        if (parameters.distribution) {
            determine_nrblocks_distribution(parameters);
            return 0;
        }

        if (parameters.modi >= parameters.modn) {
            std::cerr << "Error: i must be strictly less than m!" << '\n';
            return 1;
        }
        if (parameters.modn != 1)
            std::cout << "System " << parameters.modi << " of " << parameters.modn << '\n';
        if (parameters.inputfile1.size() == 0) {
            std::cerr << "Error: inputfile1 must be given!" << '\n';
            return 2;
        }
        if (parameters.inputfile2.size() == 0) {
            std::cerr << "Error: inputfile2 must be given!" << '\n';
            return 2;
        }
        if (parameters.logpathlength < 0 && parameters.maxmemory == 0) {
            std::cerr << "Cannot determine logtraillength when maxmemory is unspecified!" << '\n';
            return 3;
        }
        if (parameters.modi == 0) {
            if (parameters.outputfile1.size() == 0) {
                std::cerr << "Error: outputfile1 must be given!" << '\n';
                return 2;
            }
            if (parameters.outputfile2.size() == 0) {
                std::cerr << "Error: outputfile2 must be given!" << '\n';
                return 2;
            }
        }
#ifdef HAVE_CUDA
        if (vm.count("cuda_dev_query")) {
            cuda_device_query();
            return 0;
        }
#endif // CUDA
        result = dostep(parameters);
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
    return result;
}
