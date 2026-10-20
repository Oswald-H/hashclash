/*

MD5 collision generator
=======================
Source code files:
  block0.cpp
  block1.cpp
  main.cpp
  main.hpp
  md5.cpp
  block1wang.cpp
  block1stevens00.cpp
  block1stevens01.cpp
  block1stevens10.cpp
  block1stevens11.cpp
Win32 executable:
  fastcoll_v1.0.0.5.exe

Version
-----------------------
version 1.0.0.5-1, April 2006.

Copyright
=========
(c) M. Stevens, 2006. All rights reserved.

Disclaimer
==========
This software is provided as is. Use is at the user's risk.
No guarantee whatsoever is given on how it may function or malfunction.
Support cannot be expected.
This software is meant for scientific and educational purposes only.
It is forbidden to use it for other than scientific or educational purposes.
In particular, commercial and malicious use is not allowed.
Further distribution of this software, by whatever means, is not allowed
without our consent.
This includes publication of source code or executables in printed form,
on websites, newsgroups, CD-ROM's, etc.
Changing the (source) code without our consent is not allowed.
In all versions of the source code this disclaimer, the copyright
notice and the version number should be present.

*/

#include <iostream>
#include <fstream>
#include <random>

#include "main.hpp"
#include <hashclash/cli11_options.hpp>

namespace po = cli11_options;

const uint32 MD5IV[] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};

unsigned load_block(std::istream &i, uint32 block[]);
void save_block(std::ostream &o, const uint32 block[]);
void find_collision(const uint32 IV[], uint32 msg1block0[], uint32 msg1block1[],
                    uint32 msg2block0[], uint32 msg2block1[], bool verbose = false);

#include <sstream>
#include <string>
#include <utility>
#include <filesystem>
#include <cstdint>
#include <hashclash/timer.hpp>

using uint64 = std::uint64_t;

namespace fs = std::filesystem;

void test_md5iv(bool single = false);
void test_rndiv(bool single = false);
void test_reciv(bool single = false);
void test_all();

int main(int argc, char **argv) {
    seed32_1 = uint32(std::random_device{}());
    seed32_2 = 0x12345678;

    uint32 IV[4] = {MD5IV[0], MD5IV[1], MD5IV[2], MD5IV[3]};

    std::string outfn1 = "msg1.bin";
    std::string outfn2 = "msg2.bin";
    std::string ihv;
    std::string prefixfn;
    std::vector<std::string> outfns;
    bool verbose = true;

    std::cout << "MD5 collision generator v1.5\n"
                 "by Marc Stevens (http://www.win.tue.nl/hashclash/)\n"
              << '\n';

    try {
        hashclash::timer runtime(true);

        po::options_description desc("Allowed options");
        desc.add_options()("help,h", "Show options.")("quiet,q", "Be less verbose.")(
            "ihv,i", po::value<std::string>(&ihv),
            "Use specified initial value. Default is MD5 initial value.")(
            "seed1", po::value<uint32_t>(&seed32_1), "Specify a SEED1 value.  Default is random.")(
            "seed2", po::value<uint32_t>(&seed32_2),
            "Specify a SEED2 value.  Default is a constant value.")(
            "prefixfile,p", po::value<std::string>(&prefixfn),
            "Calculate initial value using given prefixfile. Also copies data to output files.")(
            "out,o", po::value<std::vector<std::string>>(&outfns)->multitoken(),
            "Set output filenames. This must be the last option and exactly 2 filenames must be specified. \nDefault: -o msg1.bin msg2.bin");

        po::options_description hidden;
        hidden.add_options()("testmd5iv",
                             "Endlessly time collision generation using MD5 initial value.")(
            "testrndiv",
            "Endlessly time collision generation using arbitrary random initial values.")(
            "testreciv",
            "Endlessly time collision generation using recommended random initial values.")(
            "testall", "Endlessly time collision generation for each case.");

        po::options_description cmdline;
        cmdline.add(desc).add(hidden);
        po::positional_options_description p;
        p.add("prefixfile", 1);
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(cmdline).positional(p).run(), vm);
        po::notify(vm);

        if (vm.count("quiet"))
            verbose = false;

        if (vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }
        if (argc == 1) {
            std::cout << desc << '\n';
            return 1;
        }

        if (vm.count("testmd5iv"))
            test_md5iv();
        if (vm.count("testrndiv"))
            test_rndiv();
        if (vm.count("testreciv"))
            test_reciv();
        if (vm.count("testall"))
            test_all();

        if (vm.count("prefixfile")) {
            unsigned l = prefixfn.size();
            if (l >= 4 && prefixfn[l - 4] == '.' && prefixfn[l - 3] != '.' &&
                prefixfn[l - 2] != '.' && prefixfn[l - 1] != '.') {
                outfn1 = prefixfn.substr(0, l - 4) + "_msg1" + prefixfn.substr(l - 4);
                outfn2 = prefixfn.substr(0, l - 4) + "_msg2" + prefixfn.substr(l - 4);
                unsigned i = 1;
                while (fs::exists(fs::path(outfn1)) || fs::exists(fs::path(outfn2))) {
                    outfn1 = prefixfn.substr(0, l - 4) + "_msg1_" + std::to_string(i) +
                             prefixfn.substr(l - 4);
                    outfn2 = prefixfn.substr(0, l - 4) + "_msg2_" + std::to_string(i) +
                             prefixfn.substr(l - 4);
                    ++i;
                }
            }
        }

        if (vm.count("out")) {
            if (outfns.size() != 2) {
                std::cerr << "Error: exactly two output filenames should be specified." << '\n';
                return 1;
            }
            outfn1 = outfns[0];
            outfn2 = outfns[1];
        }

        if (verbose)
            std::cout << "Using output filenames: '" << outfn1 << "' and '" << outfn2 << "'"
                      << '\n';
        std::ofstream ofs1(outfn1.c_str(), std::ios::binary);
        if (!ofs1) {
            std::cerr << "Error: cannot open outfile: '" << outfn1 << "'" << '\n';
            return 1;
        }
        std::ofstream ofs2(outfn2.c_str(), std::ios::binary);
        if (!ofs2) {
            std::cerr << "Error: cannot open outfile: '" << outfn2 << "'" << '\n';
            return 1;
        }

        if (vm.count("prefixfile")) {
            if (verbose)
                std::cout << "Using prefixfile: '" << prefixfn << "'" << '\n';
            std::ifstream ifs(prefixfn.c_str(), std::ios::binary);
            if (!ifs) {
                std::cerr << "Error: cannot open inputfile: '" << prefixfn << "'" << '\n';
                return 1;
            }
            uint32 block[16];
            while (true) {
                unsigned len = load_block(ifs, block);
                if (len) {
                    save_block(ofs1, block);
                    save_block(ofs2, block);
                    md5_compress(IV, block);
                } else
                    break;
            }
        } else {
            if (vm.count("ihv") == 0)
                ihv = "0123456789abcdeffedcba9876543210";
            if (ihv.size() != 32) {
                std::cerr
                    << "Error: an initial value must be specified as a hash value of 32 hexadecimal characters."
                    << '\n';
                return 1;
            } else {
                uint32 c;
                for (unsigned i = 0; i < 4; ++i) {
                    IV[i] = 0;
                    for (unsigned b = 0; b < 4; ++b) {
                        std::stringstream ss;
                        ss << ihv.substr(i * 8 + b * 2, 2);
                        ss >> std::hex >> c;
                        IV[i] += c << (b * 8);
                    }
                }
            }
        }
        if (verbose) {
            std::cout << "Using initial value: " << std::hex;
            unsigned oldwidth = std::cout.width(2);
            char oldfill = std::cout.fill('0');

            for (unsigned i = 0; i < 4; ++i) {
                for (unsigned b = 0; b < 4; ++b) {
                    std::cout.width(2);
                    std::cout.fill('0');
                    std::cout << ((IV[i] >> (b * 8)) & 0xFF);
                }
            }
            std::cout.width(oldwidth);
            std::cout.fill(oldfill);
            std::cout << std::dec << '\n';
        }

        if (verbose)
            std::cout << '\n';

        uint32 msg1block0[16];
        uint32 msg1block1[16];
        uint32 msg2block0[16];
        uint32 msg2block1[16];
        find_collision(IV, msg1block0, msg1block1, msg2block0, msg2block1, true);

        save_block(ofs1, msg1block0);
        save_block(ofs1, msg1block1);
        save_block(ofs2, msg2block0);
        save_block(ofs2, msg2block1);
        if (verbose)
            std::cout << "Running time: " << runtime.time() << " s" << '\n';
        return 0;
    } catch (std::exception &e) {
        std::cerr << "\nException caught:\n" << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "\nUnknown exception caught!" << '\n';
        return 1;
    }
}

void test_md5iv(bool single) {
    uint32 IV[4] = {MD5IV[0], MD5IV[1], MD5IV[2], MD5IV[3]};
    uint32 msg1block0[16];
    uint32 msg1block1[16];
    uint32 msg2block0[16];
    uint32 msg2block1[16];

    hashclash::timer runtime(true);
    while (true) {
        runtime.start();
        find_collision(IV, msg1block0, msg1block1, msg2block0, msg2block1);
        double time = runtime.time();
        std::cout << '\n' << "Running time: " << time << " s" << '\n';
        std::ofstream of_timings("timings_md5iv.txt", std::ios::app);
        of_timings << time << '\n';
        if (single)
            return;
    }
}

void test_rndiv(bool single) {
    uint32 IV[4];
    uint32 msg1block0[16];
    uint32 msg1block1[16];
    uint32 msg2block0[16];
    uint32 msg2block1[16];

    hashclash::timer runtime(true);
    while (true) {
        runtime.start();
        IV[0] = xrng64();
        IV[1] = xrng64();
        IV[2] = xrng64();
        IV[3] = xrng64();
        find_collision(IV, msg1block0, msg1block1, msg2block0, msg2block1);
        double time = runtime.time();
        std::cout << '\n' << "Running time: " << time << " s" << '\n';
        std::ofstream of_timings("timings_rndiv.txt", std::ios::app);
        of_timings << time << '\n';
        if (single)
            return;
    }
}

void test_reciv(bool single) {
    uint32 IV[4];
    uint32 msg1block0[16];
    uint32 msg1block1[16];
    uint32 msg2block0[16];
    uint32 msg2block1[16];

    hashclash::timer runtime(true);
    while (true) {
        runtime.start();
        IV[0] = xrng64();
        IV[1] = xrng64();
        IV[2] = xrng64();
        IV[3] = xrng64();
        IV[2] |= 1 << 25;
        IV[2] ^= ((IV[2] & (1 << 24)) << 1);
        IV[3] &= ~(1 << 25);
        IV[3] ^= ((IV[3] & (1 << 24)) << 1);

        find_collision(IV, msg1block0, msg1block1, msg2block0, msg2block1);
        double time = runtime.time();
        std::cout << '\n' << "Running time: " << time << " s" << '\n';
        std::ofstream of_timings("timings_reciv.txt", std::ios::app);
        of_timings << time << '\n';
        if (single)
            return;
    }
}

void test_all() {
    while (true) {
        test_md5iv(true);
        test_reciv(true);
        test_rndiv(true);
    }
}

unsigned load_block(std::istream &i, uint32 block[]) {
    unsigned len = 0;
    char uc;
    for (unsigned k = 0; k < 16; ++k) {
        block[k] = 0;
        for (unsigned c = 0; c < 4; ++c) {
            i.get(uc);
            if (i)
                ++len;
            else
                uc = 0;
            block[k] += uint32((unsigned char)(uc)) << (c * 8);
        }
    }
    return len;
}

void save_block(std::ostream &o, const uint32 block[]) {
    for (unsigned k = 0; k < 16; ++k)
        for (unsigned c = 0; c < 4; ++c)
            o << (unsigned char)((block[k] >> (c * 8)) & 0xFF);
}

void find_collision(const uint32 IV[], uint32 msg1block0[], uint32 msg1block1[],
                    uint32 msg2block0[], uint32 msg2block1[], bool verbose) {
    if (verbose)
        std::cout << "Generating first block: " << std::flush;
    find_block0(msg1block0, IV);

    uint32 IHV[4] = {IV[0], IV[1], IV[2], IV[3]};
    md5_compress(IHV, msg1block0);

    if (verbose)
        std::cout << '\n' << "Generating second block: " << std::flush;
    find_block1(msg1block1, IHV);

    for (int t = 0; t < 16; ++t) {
        msg2block0[t] = msg1block0[t];
        msg2block1[t] = msg1block1[t];
    }
    msg2block0[4] += 1 << 31;
    msg2block0[11] += 1 << 15;
    msg2block0[14] += 1 << 31;
    msg2block1[4] += 1 << 31;
    msg2block1[11] -= 1 << 15;
    msg2block1[14] += 1 << 31;
    if (verbose)
        std::cout << '\n';
}
