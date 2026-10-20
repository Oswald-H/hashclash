/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <stdexcept>
#include <iostream>
#include <memory>
#include <fstream>
#include <vector>
#include <string>

#include <hashclash/cli11_options.hpp>

#include "main.hpp"

#include <hashclash/md5detail.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/rng.hpp>

using namespace hashclash;
namespace po = cli11_options;
namespace fs = std::filesystem;

unsigned load_block_fillrandom(std::istream &i, uint32 block[]) {
    unsigned bytes = load_block(i, block);
    unsigned len = 0;
    for (unsigned k = 0; k < 16; ++k)
        for (unsigned c = 0; c < 4; ++c) {
            if (len >= bytes)
                block[k] ^= (xrng128() & 0xFF) << (c * 8);
            ++len;
        }
    return bytes;
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

int dostep(birthday_parameters &parameters) {
    uint32 ihv1[4];
    uint32 ihv2[4];
    uint32 msg1[16];
    uint32 msg2[16];
    {
        std::ifstream if1(parameters.inputfile1.c_str(), std::ios::binary);
        if (!if1) {
            std::cerr << "Error: cannot open inputfile 1 '" << parameters.inputfile1 << "'!"
                      << '\n';
            return 1;
        }
        std::ifstream if2(parameters.inputfile2.c_str(), std::ios::binary);
        if (!if2) {
            std::cerr << "Error: cannot open inputfile 2 '" << parameters.inputfile2 << "'!"
                      << '\n';
            return 1;
        }
        std::unique_ptr<std::ofstream> of1;
        std::unique_ptr<std::ofstream> of2;
        if (parameters.modi == 0) {
            of1 = std::make_unique<std::ofstream>(parameters.outputfile1.c_str(), std::ios::binary);
            if (!(*of1)) {
                std::cerr << "Error: cannot open outputfile 1 '" << parameters.outputfile1 << "'!"
                          << '\n';
                return 1;
            }
            of2 = std::make_unique<std::ofstream>(parameters.outputfile2.c_str(), std::ios::binary);
            if (!(*of2)) {
                std::cerr << "Error: cannot open outputfile 2 '" << parameters.outputfile1 << "'!"
                          << '\n';
                return 1;
            }
        }

        for (unsigned k = 0; k < 4; ++k)
            ihv1[k] = ihv2[k] = md5_iv[k];

        seed(0x12345678);
        // load, md5 and save inputfile1
        unsigned file1blocks = 0;
        while (load_block_fillrandom(if1, msg1) >
               64 - 12) { // stop when at least 12 bytes are not used
            md5compress(ihv1, msg1);
            if (of1)
                save_block(*of1, msg1);
            ++file1blocks;
            addseed(ihv1[0]);
        }
        // load, md5 and save inputfile2
        unsigned file2blocks = 0;
        while (load_block_fillrandom(if2, msg2) >
               64 - 12) { // stop when at least 12 bytes are not used
            md5compress(ihv2, msg2);
            if (of2)
                save_block(*of2, msg2);
            ++file2blocks;
            addseed(ihv2[0]);
        }
        // flush last partial message block when padding blocks are required
        if (file1blocks < file2blocks) {
            md5compress(ihv1, msg1);
            if (of1)
                save_block(*of1, msg1);
            ++file1blocks;
        }
        if (file2blocks < file1blocks) {
            md5compress(ihv2, msg2);
            if (of2)
                save_block(*of2, msg2);
            ++file2blocks;
        }
        // expand length outputfile1 to length outputfile2, by saving and md5'ing
        while (file1blocks < file2blocks) {
            for (unsigned k = 0; k < 16; ++k)
                msg1[k] = xrng128() + xrng128();
            md5compress(ihv1, msg1);
            if (of1)
                save_block(*of1, msg1);
            ++file1blocks;
        }
        // expand length outputfile2 to length outputfile1, by saving and md5'ing
        while (file2blocks < file1blocks) {
            for (unsigned k = 0; k < 16; ++k)
                msg2[k] = xrng128() + xrng128();
            md5compress(ihv2, msg2);
            if (of2)
                save_block(*of2, msg2);
            ++file2blocks;
        }

    } // close if1, if2, of1, of2
    addseed(uint32(time(0)));
    addseed(parameters.modi);

    for (unsigned k = 0; k < 4; ++k) {
        parameters.ihv1[k] = ihv1[k];
        parameters.ihv2[k] = ihv2[k];
    }
    for (unsigned k = 0; k < 16; ++k) {
        parameters.msg1[k] = msg1[k];
        parameters.msg2[k] = msg2[k];
    }

    std::cout << "IHV1 = {" << ihv1[0] << "," << ihv1[1] << "," << ihv1[2] << "," << ihv1[3] << "}"
              << '\n';
    std::cout << "IHV1 = " << std::hex;
    for (unsigned k = 0; k < 4; ++k)
        for (unsigned c = 0; c < 4; ++c) {
            std::cout.width(2);
            std::cout.fill('0');
            std::cout << ((ihv1[k] >> (c * 8)) & 0xFF);
        }
    std::cout << std::dec << '\n' << '\n';

    std::cout << "IHV2 = {" << ihv2[0] << "," << ihv2[1] << "," << ihv2[2] << "," << ihv2[3] << "}"
              << '\n';
    std::cout << "IHV2 = " << std::hex;
    for (unsigned k = 0; k < 4; ++k)
        for (unsigned c = 0; c < 4; ++c) {
            std::cout.width(2);
            std::cout.fill('0');
            std::cout << ((ihv2[k] >> (c * 8)) & 0xFF);
        }
    std::cout << std::dec << '\n' << '\n';

    // Start job with given parameters
    birthday(parameters);

    return 0;
}
