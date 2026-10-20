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

int main(int argc, char **argv) {
    hashclash::timer runtime(true);

    std::cout << "MD5 differential path textcollision solver\n"
                 "Copyright (C) 2024 Marc Stevens\n"
                 "http://homepages.cwi.nl/~stevens/\n"
              << '\n';

    try {
        parameters_type parameters;
        parameters.byte_alphabet = std::vector<std::string>(64, "");
        std::vector<std::vector<int>> msgdiff(16);

        // Define program options
        po::options_description cmds("Allowed commands"), desc("Allowed options"),
            msg("Define message differences (as +bitnr and -bitnr, bitnr=1-32)"),
            bba("Byte-specific alphabets"), all("Allowed options");

        cmds.add_options()("help,h", "Show options\n")("prepare,p",
                                                       "Prepare 1st near-collision block search\n")(
            "firstblock,f", "Find 1st near-collision block\n")("secondblock,s",
                                                               "Find 2nd near-collision block\n");
        desc.add_options()("workdir,w", po::value<std::string>(&workdir)->default_value("./data"),
                           "Set working directory.")(
            "pathfile", po::value<std::string>(&parameters.pathfile)->default_value("path.bin.gz"),
            "Special differential path to use")(
            "prefixfile", po::value<std::string>(&parameters.prefixfile)->default_value(""),
            "Use given prefix (must be multiple of 64 bytes)")(
            "alphabet", po::value<std::string>(&parameters.alphabet),
            "Use message alphabet for collisionfinding")(
            "threads", po::value<int>(&parameters.threads)->default_value(-1),
            "Number of worker threads");

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
        bba.add_options()("byte0",
                          po::value<std::string>(&parameters.byte_alphabet[0])->default_value(""),
                          "Byte 0 alphabet (empty=use global alphabet)")(
            "byte1", po::value<std::string>(&parameters.byte_alphabet[1])->default_value(""),
            "Byte 1 alphabet (empty=use global alphabet)")(
            "byte2", po::value<std::string>(&parameters.byte_alphabet[2])->default_value(""),
            "Byte 2 alphabet (empty=use global alphabet)")(
            "byte3", po::value<std::string>(&parameters.byte_alphabet[3])->default_value(""),
            "Byte 3 alphabet (empty=use global alphabet)")(
            "byte4", po::value<std::string>(&parameters.byte_alphabet[4])->default_value(""),
            "Byte 4 alphabet (empty=use global alphabet)")(
            "byte5", po::value<std::string>(&parameters.byte_alphabet[5])->default_value(""),
            "Byte 5 alphabet (empty=use global alphabet)")(
            "byte6", po::value<std::string>(&parameters.byte_alphabet[6])->default_value(""),
            "Byte 6 alphabet (empty=use global alphabet)")(
            "byte7", po::value<std::string>(&parameters.byte_alphabet[7])->default_value(""),
            "Byte 7 alphabet (empty=use global alphabet)")(
            "byte8", po::value<std::string>(&parameters.byte_alphabet[8])->default_value(""),
            "Byte 8 alphabet (empty=use global alphabet)")(
            "byte9", po::value<std::string>(&parameters.byte_alphabet[9])->default_value(""),
            "Byte 9 alphabet (empty=use global alphabet)")(
            "byte10", po::value<std::string>(&parameters.byte_alphabet[10])->default_value(""),
            "Byte 10 alphabet (empty=use global alphabet)")(
            "byte11", po::value<std::string>(&parameters.byte_alphabet[11])->default_value(""),
            "Byte 11 alphabet (empty=use global alphabet)")(
            "byte12", po::value<std::string>(&parameters.byte_alphabet[12])->default_value(""),
            "Byte 12 alphabet (empty=use global alphabet)")(
            "byte13", po::value<std::string>(&parameters.byte_alphabet[13])->default_value(""),
            "Byte 13 alphabet (empty=use global alphabet)")(
            "byte14", po::value<std::string>(&parameters.byte_alphabet[14])->default_value(""),
            "Byte 14 alphabet (empty=use global alphabet)")(
            "byte15", po::value<std::string>(&parameters.byte_alphabet[15])->default_value(""),
            "Byte 15 alphabet (empty=use global alphabet)")(
            "byte16", po::value<std::string>(&parameters.byte_alphabet[16])->default_value(""),
            "Byte 16 alphabet (empty=use global alphabet)")(
            "byte17", po::value<std::string>(&parameters.byte_alphabet[17])->default_value(""),
            "Byte 17 alphabet (empty=use global alphabet)")(
            "byte18", po::value<std::string>(&parameters.byte_alphabet[18])->default_value(""),
            "Byte 18 alphabet (empty=use global alphabet)")(
            "byte19", po::value<std::string>(&parameters.byte_alphabet[19])->default_value(""),
            "Byte 19 alphabet (empty=use global alphabet)")(
            "byte20", po::value<std::string>(&parameters.byte_alphabet[20])->default_value(""),
            "Byte 20 alphabet (empty=use global alphabet)")(
            "byte21", po::value<std::string>(&parameters.byte_alphabet[21])->default_value(""),
            "Byte 21 alphabet (empty=use global alphabet)")(
            "byte22", po::value<std::string>(&parameters.byte_alphabet[22])->default_value(""),
            "Byte 22 alphabet (empty=use global alphabet)")(
            "byte23", po::value<std::string>(&parameters.byte_alphabet[23])->default_value(""),
            "Byte 23 alphabet (empty=use global alphabet)")(
            "byte24", po::value<std::string>(&parameters.byte_alphabet[24])->default_value(""),
            "Byte 24 alphabet (empty=use global alphabet)")(
            "byte25", po::value<std::string>(&parameters.byte_alphabet[25])->default_value(""),
            "Byte 25 alphabet (empty=use global alphabet)")(
            "byte26", po::value<std::string>(&parameters.byte_alphabet[26])->default_value(""),
            "Byte 26 alphabet (empty=use global alphabet)")(
            "byte27", po::value<std::string>(&parameters.byte_alphabet[27])->default_value(""),
            "Byte 27 alphabet (empty=use global alphabet)")(
            "byte28", po::value<std::string>(&parameters.byte_alphabet[28])->default_value(""),
            "Byte 28 alphabet (empty=use global alphabet)")(
            "byte29", po::value<std::string>(&parameters.byte_alphabet[29])->default_value(""),
            "Byte 29 alphabet (empty=use global alphabet)")(
            "byte30", po::value<std::string>(&parameters.byte_alphabet[30])->default_value(""),
            "Byte 30 alphabet (empty=use global alphabet)")(
            "byte31", po::value<std::string>(&parameters.byte_alphabet[31])->default_value(""),
            "Byte 31 alphabet (empty=use global alphabet)")(
            "byte32", po::value<std::string>(&parameters.byte_alphabet[32])->default_value(""),
            "Byte 32 alphabet (empty=use global alphabet)")(
            "byte33", po::value<std::string>(&parameters.byte_alphabet[33])->default_value(""),
            "Byte 33 alphabet (empty=use global alphabet)")(
            "byte34", po::value<std::string>(&parameters.byte_alphabet[34])->default_value(""),
            "Byte 34 alphabet (empty=use global alphabet)")(
            "byte35", po::value<std::string>(&parameters.byte_alphabet[35])->default_value(""),
            "Byte 35 alphabet (empty=use global alphabet)")(
            "byte36", po::value<std::string>(&parameters.byte_alphabet[36])->default_value(""),
            "Byte 36 alphabet (empty=use global alphabet)")(
            "byte37", po::value<std::string>(&parameters.byte_alphabet[37])->default_value(""),
            "Byte 37 alphabet (empty=use global alphabet)")(
            "byte38", po::value<std::string>(&parameters.byte_alphabet[38])->default_value(""),
            "Byte 38 alphabet (empty=use global alphabet)")(
            "byte39", po::value<std::string>(&parameters.byte_alphabet[39])->default_value(""),
            "Byte 39 alphabet (empty=use global alphabet)")(
            "byte40", po::value<std::string>(&parameters.byte_alphabet[40])->default_value(""),
            "Byte 40 alphabet (empty=use global alphabet)")(
            "byte41", po::value<std::string>(&parameters.byte_alphabet[41])->default_value(""),
            "Byte 41 alphabet (empty=use global alphabet)")(
            "byte42", po::value<std::string>(&parameters.byte_alphabet[42])->default_value(""),
            "Byte 42 alphabet (empty=use global alphabet)")(
            "byte43", po::value<std::string>(&parameters.byte_alphabet[43])->default_value(""),
            "Byte 43 alphabet (empty=use global alphabet)")(
            "byte44", po::value<std::string>(&parameters.byte_alphabet[44])->default_value(""),
            "Byte 44 alphabet (empty=use global alphabet)")(
            "byte45", po::value<std::string>(&parameters.byte_alphabet[45])->default_value(""),
            "Byte 45 alphabet (empty=use global alphabet)")(
            "byte46", po::value<std::string>(&parameters.byte_alphabet[46])->default_value(""),
            "Byte 46 alphabet (empty=use global alphabet)")(
            "byte47", po::value<std::string>(&parameters.byte_alphabet[47])->default_value(""),
            "Byte 47 alphabet (empty=use global alphabet)")(
            "byte48", po::value<std::string>(&parameters.byte_alphabet[48])->default_value(""),
            "Byte 48 alphabet (empty=use global alphabet)")(
            "byte49", po::value<std::string>(&parameters.byte_alphabet[49])->default_value(""),
            "Byte 49 alphabet (empty=use global alphabet)")(
            "byte50", po::value<std::string>(&parameters.byte_alphabet[50])->default_value(""),
            "Byte 50 alphabet (empty=use global alphabet)")(
            "byte51", po::value<std::string>(&parameters.byte_alphabet[51])->default_value(""),
            "Byte 51 alphabet (empty=use global alphabet)")(
            "byte52", po::value<std::string>(&parameters.byte_alphabet[52])->default_value(""),
            "Byte 52 alphabet (empty=use global alphabet)")(
            "byte53", po::value<std::string>(&parameters.byte_alphabet[53])->default_value(""),
            "Byte 53 alphabet (empty=use global alphabet)")(
            "byte54", po::value<std::string>(&parameters.byte_alphabet[54])->default_value(""),
            "Byte 54 alphabet (empty=use global alphabet)")(
            "byte55", po::value<std::string>(&parameters.byte_alphabet[55])->default_value(""),
            "Byte 55 alphabet (empty=use global alphabet)")(
            "byte56", po::value<std::string>(&parameters.byte_alphabet[56])->default_value(""),
            "Byte 56 alphabet (empty=use global alphabet)")(
            "byte57", po::value<std::string>(&parameters.byte_alphabet[57])->default_value(""),
            "Byte 57 alphabet (empty=use global alphabet)")(
            "byte58", po::value<std::string>(&parameters.byte_alphabet[58])->default_value(""),
            "Byte 58 alphabet (empty=use global alphabet)")(
            "byte59", po::value<std::string>(&parameters.byte_alphabet[59])->default_value(""),
            "Byte 59 alphabet (empty=use global alphabet)")(
            "byte60", po::value<std::string>(&parameters.byte_alphabet[60])->default_value(""),
            "Byte 60 alphabet (empty=use global alphabet)")(
            "byte61", po::value<std::string>(&parameters.byte_alphabet[61])->default_value(""),
            "Byte 61 alphabet (empty=use global alphabet)")(
            "byte62", po::value<std::string>(&parameters.byte_alphabet[62])->default_value(""),
            "Byte 62 alphabet (empty=use global alphabet)")(
            "byte63", po::value<std::string>(&parameters.byte_alphabet[63])->default_value(""),
            "Byte 63 alphabet (empty=use global alphabet)");
        all.add(cmds).add(desc).add(msg).add(bba);

        // Parse program options
        po::positional_options_description p;
        p.add("pathfile", 1);
        p.add("prefixfile", 1);
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(all).positional(p).run(), vm);
        {
            std::ifstream ifs("md5diffpathhelper.cfg");
            if (ifs)
                po::store(po::parse_config_file(ifs, all), vm);
        }
        po::notify(vm);

        // Process program options
        if (vm.count("help") ||
            1 != vm.count("prepare") + vm.count("firstblock") + vm.count("secondblock")) {
            std::cout << cmds << desc << '\n';
            return 0;
        }

        for (unsigned k = 0; k < 16; ++k) {
            parameters.m_diff[k] = 0;
            for (unsigned j = 0; j < msgdiff[k].size(); ++j)
                if (msgdiff[k][j] > 0)
                    parameters.m_diff[k] += 1 << (msgdiff[k][j] - 1);
                else
                    parameters.m_diff[k] -= 1 << (-msgdiff[k][j] - 1);
        }

        if (parameters.threads <= 0 ||
            static_cast<unsigned>(parameters.threads) > std::thread::hardware_concurrency())
            parameters.threads = std::thread::hardware_concurrency();

        parameters.show_mdiffs();

        textcoll_solver_t solver;
        for (unsigned i = 0; i < 16; ++i)
            solver.m_diff[i] = parameters.m_diff[i];
        solver.threads = parameters.threads;

        solver.fillalphabet(parameters.alphabet, parameters.byte_alphabet);

        if (vm.count("prepare") + vm.count("firstblock")) {
            differentialpath diffpath;
            std::vector<differentialpath> vecpath;
            bool failed = true;
            try {
                load_gz(vecpath, binary_archive, parameters.pathfile);
                failed = false;
            } catch (const std::exception &) {
            }
            if (failed) {
                vecpath.clear();
                try {
                    load_gz(diffpath, binary_archive, parameters.pathfile);
                    vecpath.push_back(diffpath);
                    failed = false;
                } catch (const std::exception &) {
                }
            }
            if (failed || vecpath.size() == 0) {
                std::cerr << "Error: could not load path(s) in '" << parameters.pathfile << "'!"
                          << '\n';
                return 1;
            }
            show_path(vecpath[0], parameters.m_diff);
            solver.filltables(vecpath[0]);
        }

        uint32 ihv1[4] = {md5_iv[0], md5_iv[1], md5_iv[2], md5_iv[3]};
        uint32 ihv2[4] = {md5_iv[0], md5_iv[1], md5_iv[2], md5_iv[3]};
        std::size_t prefixblocks = 0;

        if (parameters.prefixfile.size() > 0 &&
            vm.count("firstblock") + vm.count("secondblock") != 0) {
            uint32 msg1[16];
            uint32 msg2[16];

            std::ifstream if2(parameters.prefixfile.c_str(), std::ios::binary);
            if (!if2) {
                std::cerr << "Error: cannot open inputfile 2 '" << parameters.prefixfile << "'!"
                          << '\n';
                return 1;
            }
            while (load_block(if2, msg1) > 0) {
                ++prefixblocks;
                for (unsigned k = 0; k < 4; ++k)
                    ihv2[k] = ihv1[k];
                for (unsigned k = 0; k < 16; ++k)
                    msg2[k] = msg1[k] + parameters.m_diff[k];
                md5compress(ihv1, msg1);
                md5compress(ihv2, msg2);
            }
            if (vm.count("firstblock")) {
                for (unsigned k = 0; k < 4; ++k)
                    ihv2[k] = ihv1[k];
            }
        }

        solver.prefixblocks = prefixblocks;
        for (unsigned k = 0; k < 4; ++k) {
            solver.ihv1[k] = ihv1[k];
            solver.ihv2[k] = ihv2[k];
        }

        std::cout << "IHV1   = " << std::hex;
        for (unsigned k = 0; k < 4; ++k)
            for (unsigned c = 0; c < 4; ++c) {
                std::cout.width(2);
                std::cout.fill('0');
                std::cout << ((ihv1[k] >> (c * 8)) & 0xFF);
            }
        std::cout << std::dec << '\n';
        if (vm.count("secondblock")) {
            std::cout << "IHV2   = " << std::hex;
            for (unsigned k = 0; k < 4; ++k)
                for (unsigned c = 0; c < 4; ++c) {
                    std::cout.width(2);
                    std::cout.fill('0');
                    std::cout << ((ihv2[k] >> (c * 8)) & 0xFF);
                }
            std::cout << std::dec << '\n';
            std::cout << "dIHV   = " << naf(ihv2[0] - ihv1[0]) << naf(ihv2[1] - ihv1[1])
                      << naf(ihv2[2] - ihv1[2]) << naf(ihv2[3] - ihv1[3]) << '\n';
        }

        if (vm.count("prepare"))
            solver.prepare_block1();
        if (vm.count("firstblock"))
            solver.start_block1();
        if (vm.count("secondblock"))
            solver.start_block2();

        if (solver.solution_found.load())
            return 0;

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
            if (!!i) {
                ++len;
                block[k] += uint32((unsigned char)(uc)) << (c * 8);
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
            o << (unsigned char)((block[k] >> (c * 8)) & 0xFF);
}
