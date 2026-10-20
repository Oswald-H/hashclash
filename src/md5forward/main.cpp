/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include <hashclash/cli11_options.hpp>

#include <hashclash/sdr.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/booleanfunction.hpp>

#include "main.hpp"

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

std::mutex mut;
std::string workdir;

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    std::cout << "Extend MD5 differential paths forward\n"
                 "Copyright (C) 2009 Marc Stevens\n"
                 "http://homepages.cwi.nl/~stevens/\n"
              << '\n';

    try {
        path_container_autobalance container;
        std::vector<std::vector<int>> msgdiff(16);

        // Define program options
        po::options_description desc("Allowed options"),
            msg("Define message differences (as +bitnr and -bitnr, bitnr=1-32)"),
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

                        ("showinputpaths,s", po::bool_switch(&container.showinputpaths),
                         "Show all input paths.")

                            ("splitsave",
                             po::value<unsigned>(&container.splitsave)->default_value(1),
                             "Split set of output paths and save into N files.\n")

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
                                                                         ->default_value(1),
                                                                     "Fill up to fraction of target amount\n\twith heavier paths than maxcond.")

                                                                        ("estimate,e",
                                                                         po::value<double>(
                                                                             &container
                                                                                  .estimatefactor)
                                                                             ->default_value(2),
                                                                         "Do an estimate for maxcond.")

                                                                            ("nafestimate",
                                                                             po::value<unsigned>(
                                                                                 &container
                                                                                      .nafestweight)
                                                                                 ->default_value(8),
                                                                             "Estimate of naf(Q_t+1) in # conditions.")

                                                                                ("noverify",
                                                                                 po::bool_switch(
                                                                                     &container
                                                                                          .noverify),
                                                                                 "Do not verify rotations etc.")

                                                                                    ("normalt01",
                                                                                     po::bool_switch(
                                                                                         &container
                                                                                              .normalt01),
                                                                                     "Do not use special methods for t=0,1")

                                                                                        ("minQ456tunnel",
                                                                                         po::value<
                                                                                             unsigned>(
                                                                                             &container
                                                                                                  .minQ456tunnel),
                                                                                         "Minimum tunnel strength over Q4, Q5, Q6.")

                                                                                            ("minQ91011tunnel",
                                                                                             po::value<
                                                                                                 unsigned>(
                                                                                                 &container
                                                                                                      .minQ91011tunnel),
                                                                                             "Minimum tunnel strength over Q9, Q10, Q11.")

                                                                                                ("minQ314tunnel",
                                                                                                 po::value<
                                                                                                     unsigned>(
                                                                                                     &container
                                                                                                          .minQ314tunnel),
                                                                                                 "Minimum tunnel strength of Q3, Q14.")

                                                                                                    ("threads",
                                                                                                     po::value<
                                                                                                         int>(
                                                                                                         &container
                                                                                                              .threads)
                                                                                                         ->default_value(
                                                                                                             -1),
                                                                                                     "Number of worker threads")(
                                                                                                        "uct",
                                                                                                        po::value<
                                                                                                            int>(
                                                                                                            &container
                                                                                                                 .uct)
                                                                                                            ->default_value(
                                                                                                                -4),
                                                                                                        "Disallow condition: Q_t[b] = c")(
                                                                                                        "ucb",
                                                                                                        po::value<
                                                                                                            int>(
                                                                                                            &container
                                                                                                                 .ucb)
                                                                                                            ->default_value(
                                                                                                                -1),
                                                                                                        "Disallow condition: Q_t[b] = c")(
                                                                                                        "ucc",
                                                                                                        po::value<
                                                                                                            char>(
                                                                                                            &container
                                                                                                                 .ucc)
                                                                                                            ->default_value(
                                                                                                                '.'),
                                                                                                        "Disallow condition: Q_t[b] = c");

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
        ihv.add_options()("ihv0a", po::value<uint32>(&container.IHV1[0])->default_value(0),
                          "ihv0a")("ihv1a", po::value<uint32>(&container.IHV1[1])->default_value(0),
                                   "ihv1a")(
            "ihv2a", po::value<uint32>(&container.IHV1[2])->default_value(0),
            "ihv2a")("ihv3a", po::value<uint32>(&container.IHV1[3])->default_value(0), "ihv3a")(
            "ihv0b", po::value<uint32>(&container.IHV2[0])->default_value(0),
            "ihv0b")("ihv1b", po::value<uint32>(&container.IHV2[1])->default_value(0), "ihv1b")(
            "ihv2b", po::value<uint32>(&container.IHV2[2])->default_value(0),
            "ihv2b")("ihv3b", po::value<uint32>(&container.IHV2[3])->default_value(0), "ihv3b");
        all.add(desc).add(msg).add(ihv);

        // Parse program options
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, all), vm);
        {
            std::ifstream ifs("md5diffpathforward.cfg");
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
            container.splitsave = 0;
        }

        if (false == container.normalt01 && container.t <= 1) {
            uint32 *IHV1 = container.IHV1;
            uint32 *IHV2 = container.IHV2;
            std::cout << "IHV1= " << IHV1[0] << " " << IHV1[1] << " " << IHV1[2] << " " << IHV1[3]
                      << '\n'
                      << "    = 0x" << std::hex << IHV1[0] << " 0x" << IHV1[1] << " 0x" << IHV1[2]
                      << " 0x" << IHV1[3] << std::dec << '\n';
            std::cout << "IHV2= " << IHV2[0] << " " << IHV2[1] << " " << IHV2[2] << " " << IHV2[3]
                      << '\n'
                      << "    = 0x" << std::hex << IHV2[0] << " 0x" << IHV2[1] << " 0x" << IHV2[2]
                      << " 0x" << IHV2[3] << std::dec << '\n';
            std::cout << "dIHV= " << (IHV2[0] - IHV1[0]) << " " << (IHV2[1] - IHV1[1]) << " "
                      << (IHV2[2] - IHV1[2]) << " " << (IHV2[3] - IHV1[3]) << " = 0x" << std::hex
                      << (IHV2[0] - IHV1[0]) << " " << (IHV2[1] - IHV1[1]) << " "
                      << (IHV2[2] - IHV1[2]) << " " << (IHV2[3] - IHV1[3]) << std::dec << '\n';
            std::cout << "dIHV= " << naf(IHV2[0] - IHV1[0]) << " " << naf(IHV2[1] - IHV1[1]) << " "
                      << naf(IHV2[2] - IHV1[2]) << " " << naf(IHV2[3] - IHV1[3]) << std::dec
                      << '\n';
        }

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

        if (container.threads <= 0 ||
            static_cast<unsigned>(container.threads) > std::thread::hardware_concurrency())
            container.threads = std::thread::hardware_concurrency();

        // Start job with given parameters
        container.set_parameters();
        for (unsigned tt = container.t; tt < container.t + container.trange; ++tt) {
            path_container_autobalance containertmp = container;
            containertmp.t = tt;

            if (tt == 1 && container.ubound > 0) {
                containertmp.ubound >>= 2;
                //				containertmp.maxcond = 157;
            }
            if (tt == 2 && container.ubound > 0) {
                containertmp.ubound >>= 1;
                //				containertmp.maxcond = 177;
            }
            if (tt == 3 && container.ubound > 0)
                containertmp.ubound >>= 0;

            dostep(containertmp, true);
        }
        container.t += container.trange;
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
