/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <numbers>
#include <sstream>

#include <hashclash/saveload_bz2.hpp>

#include "main.hpp"

#include <filesystem>
#include <thread>
#include <mutex>

#include <hashclash/sdr.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/md5detail.hpp>
#include <hashclash/cpu_avx2.hpp>

#include "distribution.hpp" // distribution tables
#include "storage.hpp"

void determine_nrblocks_distribution(birthday_parameters &parameters);

std::mutex global_mutex;
#define LOCK_GLOBAL_MUTEX std::lock_guard<std::mutex> lock(global_mutex);

using namespace hashclash;

/* LOCK_GLOBAL_MUTEX not needed */
uint32 ihv1[4], ihv2[4], ihv2mod[4], msg1[16], msg2[16], precomp1[4], precomp2[4];
uint32 hybridmask = 0, distinguishedpointmask = 0, maximumpathlength = 1;
unsigned maxblocks = 64;
bool memhardlimit = false;
unsigned parameterspathtyperange = 0;
unsigned procmodn = 0, procmodi = 0;
bool generatormode = false;
std::vector<std::pair<uint32, uint32>> singleblockdata;
/**/

/* LOCK_GLOBAL_MUTEX required */
uint32 colla1, collb1, collc1, colla2, collb2, collc2;
uint64 totwork = 0, totworkallproc = 0;
unsigned bestnrblocks = 64;
std::atomic<bool> quit = false;
unsigned collrobinhoods = 0, collequalihvs = 0, collusefull = 0;
std::vector<std::vector<trail_type>> trail_distribution(0);
// vector< pair<trail_type, trail_type> > collisions_queue(0);
/**/

// LOCK_GLOBAL_MUTEX required
void status_line() {
    unsigned totcoll = main_storage.get_totcoll();
    unsigned collqueue = main_storage.get_collqueuesize();
    if (procmodn > 1)
        std::cout << "Work: mine=2^(" << log(double(totwork)) / std::numbers::ln2 << ") all=2^("
                  << log(double(totworkallproc)) / std::numbers::ln2 << ")";
    else
        std::cout << "Work: 2^(" << log(double(totwork)) / std::numbers::ln2 << ")";
    std::cout << ", Coll.: " << main_storage.get_totcoll() << "(uf=" << collusefull
              << ",nuf=" << collequalihvs
              << ",?=" << (totcoll - collusefull - collequalihvs - collrobinhoods - collqueue)
              << ",q=" << collqueue << ",rh=" << collrobinhoods << "), Blocks: " << bestnrblocks
              << '\n'
              << std::flush; //"     \r" << flush;
}

// LOCK_GLOBAL_MUTEX not needed
unsigned nrblocks(uint32 dihv[4]) {
    uint32 ptrmaskt2 = 0;
    for (unsigned j = 0; j < parameterspathtyperange; ++j)
        ptrmaskt2 |= 2 << j;
    sdr p1naf = naf(dihv[3]);
    uint32 p1nafmask = p1naf.mask;
    uint32 p2nafmask = naf(dihv[1] - dihv[3]).mask;
    p2nafmask = rotate_right(p2nafmask, 21);
    uint32 p1mask = p1nafmask;
    for (unsigned j = 0; j < parameterspathtyperange; ++j)
        p1mask |= p1nafmask << (j + 1);
    p2nafmask &= ~p1mask;
    for (unsigned b = 0; b < 32; ++b)
        if (p2nafmask & (1 << b))
            p2nafmask &= ~(ptrmaskt2 << b);
    p1nafmask &= ~(p2nafmask << 1);
    p1nafmask &= ~(p1nafmask >> 31);
    return hw(p1nafmask) + 2 * hw(p2nafmask);
}

// LOCK_GLOBAL_MUTEX not needed
void precomputestate(uint32 ihv[4], uint32 block[16]) {
#define HASHCLASH_MD5COMPRESS_STEP(f, a, b, c, d, m, ac, rc)                                       \
    a += f(b, c, d) + m + ac;                                                                      \
    a = rotate_left(a, rc);                                                                        \
    a += b;

    uint32 a = ihv[0];
    uint32 b = ihv[1];
    uint32 c = ihv[2];
    uint32 d = ihv[3];

    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[0], 0xd76aa478, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[1], 0xe8c7b756, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[2], 0x242070db, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[3], 0xc1bdceee, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[4], 0xf57c0faf, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[5], 0x4787c62a, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[6], 0xa8304613, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[7], 0xfd469501, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[8], 0x698098d8, 7);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, block[9], 0x8b44f7af, 12);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, block[10], 0xffff5bb1, 17);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, block[11], 0x895cd7be, 22);
    HASHCLASH_MD5COMPRESS_STEP(md5_ff, a, b, c, d, block[12], 0x6b901122, 7);

    ihv[0] = a;
    ihv[1] = b;
    ihv[2] = c;
    ihv[3] = d;
}

// LOCK_GLOBAL_MUTEX not needed
void birthday_step(uint32 &x, uint32 &y, uint32 &z) {
    uint32 *block;
    uint32 *precomp;
    uint32 *ihv;
    if (x <= y) {
        precomp = precomp1;
        block = msg1;
        ihv = ihv1;
    } else {
        precomp = precomp2;
        block = msg2;
        ihv = ihv2mod;
    }
    {
        uint32 a = precomp[0], b = precomp[1], c = precomp[2], d = precomp[3];
        HASHCLASH_MD5COMPRESS_STEP(md5_ff, d, a, b, c, z, 0xfd987193, 12);
        HASHCLASH_MD5COMPRESS_STEP(md5_ff, c, d, a, b, x, 0xa679438e, 17);
        HASHCLASH_MD5COMPRESS_STEP(md5_ff, b, c, d, a, y, 0x49b40821, 22);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[1], 0xf61e2562, 5);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[6], 0xc040b340, 9);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[11], 0x265e5a51, 14);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[0], 0xe9b6c7aa, 20);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[5], 0xd62f105d, 5);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[10], 0x02441453, 9);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, y, 0xd8a1e681, 14);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[4], 0xe7d3fbc8, 20);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, block[9], 0x21e1cde6, 5);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, x, 0xc33707d6, 9);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[3], 0xf4d50d87, 14);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[8], 0x455a14ed, 20);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, a, b, c, d, z, 0xa9e3e905, 5);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, d, a, b, c, block[2], 0xfcefa3f8, 9);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, c, d, a, b, block[7], 0x676f02d9, 14);
        HASHCLASH_MD5COMPRESS_STEP(md5_gg, b, c, d, a, block[12], 0x8d2a4c8a, 20);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[5], 0xfffa3942, 4);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[8], 0x8771f681, 11);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[11], 0x6d9d6122, 16);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, x, 0xfde5380c, 23);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[1], 0xa4beea44, 4);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[4], 0x4bdecfa9, 11);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[7], 0xf6bb4b60, 16);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[10], 0xbebfbc70, 23);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, z, 0x289b7ec6, 4);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[0], 0xeaa127fa, 11);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, block[3], 0xd4ef3085, 16);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[6], 0x04881d05, 23);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, a, b, c, d, block[9], 0xd9d4d039, 4);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, d, a, b, c, block[12], 0xe6db99e5, 11);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, c, d, a, b, y, 0x1fa27cf8, 16);
        HASHCLASH_MD5COMPRESS_STEP(md5_hh, b, c, d, a, block[2], 0xc4ac5665, 23);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[0], 0xf4292244, 6);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[7], 0x432aff97, 10);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, x, 0xab9423a7, 15);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[5], 0xfc93a039, 21);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[12], 0x655b59c3, 6);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[3], 0x8f0ccc92, 10);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[10], 0xffeff47d, 15);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[1], 0x85845dd1, 21);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[8], 0x6fa87e4f, 6);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, y, 0xfe2ce6e0, 10);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[6], 0xa3014314, 15);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, z, 0x4e0811a1, 21);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, a, b, c, d, block[4], 0xf7537e82, 6);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, d, a, b, c, block[11], 0xbd3af235, 10);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, c, d, a, b, block[2], 0x2ad7d2bb, 15);
        HASHCLASH_MD5COMPRESS_STEP(md5_ii, b, c, d, a, block[9], 0xeb86d391, 21);
        a += ihv[0];
        b += ihv[1];
        c += ihv[2];
        d += ihv[3];
        if (maxblocks != 1) {
            x = a;
            y = d - c;
            z = (d - b) & hybridmask;
        } else {
            x = a;
            y = d;
            z = c & hybridmask;
        }
    }
}

// LOCK_GLOBAL_MUTEX not needed
void generate_trail(trail_type &trail) {
    uint32 x = trail.start[0], y = trail.start[1], z = trail.start[2];
    trail.len = 1;
    birthday_step(x, y, z);
    while (trail.len <= maximumpathlength && 0 != (x & distinguishedpointmask)) {
        birthday_step(x, y, z);
        ++trail.len;
    }
    trail.end[0] = x;
    trail.end[1] = y;
    trail.end[2] = z;
}

/*
bool operator<(const trail_type& l, const trail_type& r)
{
    for (unsigned i=0; i<3; ++i)
        if (l.start[i]!=r.start[i])
            return l.start[i]<r.start[i];
    for (unsigned i=0; i<3; ++i)
        if (l.end[i]!=r.end[i])
            return l.end[i]<r.end[i];
    return false;
}
std::set<trail_type> check;
size_t doubles = 0;
*/

// do not use LOCK_GLOBAL_MUTEX
// function possibly calls LOCK_GLOBAL_MUTEX
void find_collision(const trail_type &trail1, const trail_type &trail2) {
    uint32 a1 = trail1.start[0];
    uint32 b1 = trail1.start[1];
    uint32 c1 = trail1.start[2];
    uint32 a2 = trail2.start[0];
    uint32 b2 = trail2.start[1];
    uint32 c2 = trail2.start[2];
    uint32 len1 = trail1.len;
    uint32 len2 = trail2.len;
    /*
        if (len1 == 1 && len2 ==1)
        {
            LOCK_GLOBAL_MUTEX;
            auto old = doubles;
            if (!check.insert(trail1).second)
                ++doubles;
            if (!check.insert(trail2).second)
                ++doubles;
            if (doubles>0 && old != doubles)
                cout << "D" << doubles << " " << flush;
        }
    */
    /*
        // sanity check for non-preprocessed inputs
        if (len1 != 1 || len2 != 1)
        {
            if (trail1.len > maximumpathlength || 0!=(trail1.end[0]&distinguishedpointmask)
                || trail2.len > maximumpathlength || 0!=(trail2.end[0]&distinguishedpointmask))
            {
                LOCK_GLOBAL_MUTEX;
                cout << "X" << flush;
                ++collrobinhoods; // have to categorize this collision
                return;
            }
        }
    */
    while (len1 > len2) {
        birthday_step(a1, b1, c1);
        --len1;
    }
    while (len2 > len1) {
        birthday_step(a2, b2, c2);
        --len2;
    }

    // check for robin hood
    if (a1 == a2 && b1 == b2 && c1 == c2) {
        LOCK_GLOBAL_MUTEX;
        ++collrobinhoods;
        return;
    }

    uint32 oa1 = a1, oa2 = a2, ob1 = b1, ob2 = b2, oc1 = c1, oc2 = c2;
    while ((a1 != a2 || b1 != b2 || c1 != c2) && len1 > 0) {
        oa1 = a1;
        oa2 = a2;
        ob1 = b1;
        ob2 = b2;
        oc1 = c1;
        oc2 = c2;
        birthday_step(a1, b1, c1);
        birthday_step(a2, b2, c2);
        --len1;
    }
    if (len1 == 0 && (a1 != a2 || b1 != b2 || c1 != c2)) {
        LOCK_GLOBAL_MUTEX;
        std::cerr << "find_collision(): len=0 (len1=" << trail1.len << ",len2=" << trail2.len
                  << ")           " << '\n';
        ++collrobinhoods;
        return;
    }

    // check for same ihv birthday collision
    if ((oa1 <= ob1) == (oa2 <= ob2)) {
        LOCK_GLOBAL_MUTEX;
        ++collequalihvs;
        return;
    }

    uint32 ihvmsg1[4];
    uint32 ihvmsg2[4];
    if (oa2 <= ob2) {
        std::swap(oa2, oa1);
        std::swap(ob2, ob1);
        std::swap(oc2, oc1);
    }
    uint32 lmsg1[16];
    uint32 lmsg2[16];
    for (unsigned i = 0; i < 13; ++i) {
        lmsg1[i] = msg1[i];
        lmsg2[i] = msg2[i];
    }
    lmsg1[13] = oc1;
    lmsg1[14] = oa1;
    lmsg1[15] = ob1;
    ihvmsg1[0] = ihv1[0];
    ihvmsg1[1] = ihv1[1];
    ihvmsg1[2] = ihv1[2];
    ihvmsg1[3] = ihv1[3];
    md5compress(ihvmsg1, lmsg1);
    lmsg2[13] = oc2;
    lmsg2[14] = oa2;
    lmsg2[15] = ob2;
    ihvmsg2[0] = ihv2[0];
    ihvmsg2[1] = ihv2[1];
    ihvmsg2[2] = ihv2[2];
    ihvmsg2[3] = ihv2[3];
    md5compress(ihvmsg2, lmsg2);
    uint32 dihv[4] = {ihvmsg2[0] - ihvmsg1[0], ihvmsg2[1] - ihvmsg1[1], ihvmsg2[2] - ihvmsg1[2],
                      ihvmsg2[3] - ihvmsg1[3]};

    unsigned n = 64;
    if (maxblocks == 1) {
        if (dihv[0] == uint32(-(1 << 5)) && dihv[3] == uint32(-(1 << 5) + (1 << 25)) &&
            (dihv[2] & hybridmask) == (uint32(-(1 << 5) + (1 << 25) + (1 << 23)) & hybridmask)) {
            std::pair<uint32, uint32> bc(dihv[1], dihv[2]);
            if (std::binary_search(singleblockdata.begin(), singleblockdata.end(), bc))
                n = 1;
        } else if (dihv[0] == uint32(1 << 5) && dihv[3] == uint32((1 << 5) - (1 << 25)) &&
                   (dihv[2] & hybridmask) ==
                       (uint32((1 << 5) - (1 << 25) - (1 << 23)) & hybridmask)) {
            std::pair<uint32, uint32> bc(-dihv[1], -dihv[2]);
            if (std::binary_search(singleblockdata.begin(), singleblockdata.end(), bc))
                n = 1;
        } else
            std::cout << "bad!!" << '\n';
    } else {
        if (dihv[0] != 0 && dihv[2] != dihv[3]) {
            std::cerr << "dihv fails assumptions" << '\n';
            return;
            //			throw;
        }
        n = nrblocks(dihv);
    }
    LOCK_GLOBAL_MUTEX;
    ++collusefull;
    if (quit == true)
        return;

    if (n < bestnrblocks) {
        bestnrblocks = n;
        std::cout << '\n';
        status_line();
    }

    if (n <= maxblocks) {
        if (n >= bestnrblocks) {
            std::cout << '\n';
            status_line();
        }
        colla1 = oa1;
        collb1 = ob1;
        collc1 = oc1;
        colla2 = oa2;
        collb2 = ob2;
        collc2 = oc2;
        std::cout << '\n';

        std::cout << "IHV1   = {" << ihvmsg1[0] << "," << ihvmsg1[1] << "," << ihvmsg1[2] << ","
                  << ihvmsg1[3] << "}" << '\n';
        std::cout << "IHV2   = {" << ihvmsg2[0] << "," << ihvmsg2[1] << "," << ihvmsg2[2] << ","
                  << ihvmsg2[3] << "}" << '\n';
        std::cout << "dIHV   = {" << dihv[0] << "," << dihv[1] << "," << dihv[2] << "," << dihv[3]
                  << "}" << '\n';
        sdr v = naf(dihv[3]);
        sdr w = naf(dihv[1] - dihv[3]);
        std::cout << "Dv     = " << v << '\n';
        std::cout << "Dw     = " << w << '\n';
        std::cout << "Blocks = " << n << '\n' << '\n';
        std::cout << "Msg1   = ";
        for (unsigned i = 0; i < 13; ++i)
            std::cout << msg1[i] << " ";
        std::cout << oc1 << " " << oa1 << " " << ob1 << '\n';
        std::cout << "Msg2   = ";
        for (unsigned i = 0; i < 13; ++i)
            std::cout << msg2[i] << " ";
        std::cout << oc2 << " " << oa2 << " " << ob2 << '\n';
#if 1
        quit = true;
        std::ostringstream vstream;
        std::ostringstream wstream;
        vstream << v;
        wstream << w;
        std::string filename1 =
            workdir + "/birthdayblock1_" + vstream.str() + "_" + wstream.str() + ".bin";
        std::string filename2 =
            workdir + "/birthdayblock2_" + vstream.str() + "_" + wstream.str() + ".bin";
        std::ofstream of1(filename1.c_str(), std::ios::binary | std::ios::app);
        std::ofstream of2(filename2.c_str(), std::ios::binary | std::ios::app);
        save_block(of1, lmsg1);
        save_block(of2, lmsg2);
        std::cout << "Wrote birthdaycollision block to: " << '\n';
        std::cout << "\t" << filename1 << '\n' << "\t" << filename2 << '\n';
#endif
    }
}

// LOCK_GLOBAL_MUTEX required
std::vector<trail_type> main_storage_buffer;
void distribute_trail(const trail_type &newtrail) {
    totwork += newtrail.len;
    totworkallproc += newtrail.len;
    uint32 procindex = newtrail.end[1] % procmodn;
    if (procindex == procmodi && !generatormode)
        //		main_storage.insert_trail(newtrail);
        main_storage_buffer.emplace_back(newtrail);
    else
        trail_distribution[procindex].push_back(newtrail);
}
// LOCK_GLOBAL_MUTEX required
void distribute_trail_flush() {
    main_storage.insert_trails(main_storage_buffer);
    main_storage_buffer.clear();
}

// LOCK_GLOBAL_MUTEX required
struct trail_distribute_type {
    uint32 proci;
    uint64 totwork;
    std::vector<trail_type> trails;
    uint32 check;

    template <class Archive> void serialize(Archive &ar) {
        ar(cereal::make_nvp("proci", proci), cereal::make_nvp("totwork", totwork),
           cereal::make_nvp("trails", trails), cereal::make_nvp("check", check));
    }
};

// do not use LOCK_GLOBAL_MUTEX
// function possibly calls LOCK_GLOBAL_MUTEX
void load_save_trails(bool dosave = true) {
    static std::vector<uint64> workothers(procmodn, 0);
    static std::vector<unsigned> fileserialothers(procmodn, 0);
    static unsigned savepart = 0;
    try {
        std::filesystem::directory_iterator dit(workdir + "/" + std::to_string(procmodi)), ditend;
        for (; dit != ditend; ++dit) {
            std::filesystem::path filepath = *dit;
            if (!std::filesystem::exists(*dit) || std::filesystem::is_symlink(*dit) ||
                std::filesystem::is_directory(*dit))
                continue;
            std::string filename = dit->path().filename().string();
            if (filename.size() < 16)
                continue;
            if (filename.substr(0, 12) != "birthdaydata")
                continue;
            if (filename.substr(filename.size() - 4) != ".bin")
                continue;

            trail_distribute_type traildata;
            traildata.check = 0;
            try {
                load(traildata, binary_archive, *dit);
            } catch (const std::exception &) {
            }
            if (traildata.check != 0x56139078)
                continue; // incomplete file
            try {
                std::filesystem::remove(*dit);
            } catch (const std::exception &) {
            }
            if (traildata.proci >= procmodn)
                continue;
            if (workothers[traildata.proci] < traildata.totwork)
                workothers[traildata.proci] = traildata.totwork;
            LOCK_GLOBAL_MUTEX;
            for (unsigned i = 0; i < traildata.trails.size(); ++i)
                if (traildata.trails[i].end[1] % procmodn != procmodi)
                    std::cerr << "False trail loaded!!" << '\n';
            main_storage.insert_trails(traildata.trails);
        }
    } catch (const std::exception &) {
    }
    if (dosave) {
        try {
            trail_distribute_type traildata;
            traildata.proci = procmodi;
            traildata.totwork = totwork;
            traildata.check = 0x56139078;
            savepart = (savepart + 1) % 64;
            for (unsigned i = 0; i < procmodn; ++i) {
                traildata.trails.clear();
                {
                    LOCK_GLOBAL_MUTEX;
                    std::swap(traildata.trails, trail_distribution[i]);
                }
                if (traildata.trails.size()) {
                    try {
                        ++fileserialothers[i];
                        // The workers only reach the global generator while holding
                        // global_mutex, so drawing these two values needs the same lock.
                        uint32 basenamernd, tmpnamernd;
                        {
                            LOCK_GLOBAL_MUTEX;
                            basenamernd = xrng128();
                            tmpnamernd = xrng128();
                        }
                        std::string basefilename = workdir + "/" + std::to_string(i) +
                                                   "/birthdaydata_" + std::to_string(procmodi) +
                                                   "_" + std::to_string(basenamernd) + "_";
                        std::string tmpfilename = "/tmp/birthdaydata_" + std::to_string(i) + "_" +
                                                  std::to_string(procmodi) + "_" +
                                                  std::to_string(tmpnamernd) + "_";
                        save(traildata, binary_archive,
                             tmpfilename + std::to_string(fileserialothers[i]) + ".tmp");
                        std::filesystem::copy_file(
                            tmpfilename + std::to_string(fileserialothers[i]) + ".tmp",
                            basefilename + std::to_string(fileserialothers[i]) + ".bin");
                        std::filesystem::remove(tmpfilename + std::to_string(fileserialothers[i]) +
                                                ".tmp");
                    } catch (const std::exception &e) {
                        std::cerr << e.what() << '\n';
                    }
                }
            }
        } catch (const std::exception &) {
        }
    }
    LOCK_GLOBAL_MUTEX;
    workothers[procmodi] = totwork;
    totworkallproc = 0;
    for (unsigned i = 0; i < workothers.size(); ++i)
        totworkallproc += workothers[i];
}

struct coll_less {
    bool operator()(const std::pair<trail_type, trail_type> &_Left,
                    const std::pair<trail_type, trail_type> &_Right) const {
        return _Left.first.len < _Right.first.len;
    }
};

struct birthday_thread {
    static uint32 id_counter; // = 1 (below class definition)
    uint32 id;
    int _cuda_device_nr;
    cuda_device _cuda_device;
    simd_device_avx256 _simd_device;
    bool _nosimd;

    birthday_thread(int cuda_device_nr = -1)
        : id(id_counter++), _cuda_device_nr(cuda_device_nr), _nosimd(false) {}

    void loop_cuda(bool single = false) {
        std::vector<trail_type> work;
        std::vector<std::pair<trail_type, trail_type>> collisions;
        while (true) {
            [[maybe_unused]] uint64 seed;
            {
                LOCK_GLOBAL_MUTEX;
                seed = uint64(xrng128()) + (uint64(xrng128()) << 32) + 1111 * procmodi;
                xrng128();
                xrng128();

                /*
                                if (_cuda_device_nr == 0 && (haveenoughcoll ||
                   main_storage.get_collqueuesize() >= 2048))
                                {
                                    if (!haveenoughcoll)
                                        std::cout << "Thread " << id << " (CUDA): started processing
                   colliding trails on GPU" << '\n';
                                    main_storage.get_birthdaycollisions(collisions);
                                    haveenoughcoll = true;
                                }
                */
                if (quit)
                    return;
            }
#ifdef HAVE_CUDA
            if (_cuda_device_nr >= 0)
                _cuda_device.cuda_fill_trail_buffer(id, seed, work, collisions,
                                                    bool(maxblocks == 1));
            else
                return;
#endif

            // insert the trails into the trail hash
            if (collisions.size() > 0) {
                //				std::cout << "Thread " << id << " (CUDA): " << collisions.size() <<
                //" colliding trails returned" << '\n';
                std::cout << "C" << collisions.size() << std::flush;
                for (unsigned i = 0; i < collisions.size(); ++i)
                    find_collision(collisions[i].first, collisions[i].second);
                collisions.clear();
            } else {
                LOCK_GLOBAL_MUTEX;
                for (unsigned i = 0; i < work.size(); ++i)
                    distribute_trail(work[i]);
                distribute_trail_flush();
            }
            if (single)
                break;
        }
    }

    void loop_simd(bool single = false) {
        std::vector<trail_type> work;
        std::vector<std::pair<trail_type, trail_type>> collisions;
        bool verified = false;
        std::size_t workamount = (std::size_t(1) << 26) / std::size_t(distinguishedpointmask + 1);
        while (true) {
            uint64 seed;
            {
                LOCK_GLOBAL_MUTEX;
                seed = uint64(xrng128()) + (uint64(xrng128()) << 32) + 1111 * procmodi;
                xrng128();
                xrng128();
                main_storage.get_birthdaycollisions(collisions);

                if (quit)
                    return;
            }

            // insert the trails into the trail hash
            if (collisions.size() > 0) {
                for (unsigned i = 0; i < collisions.size(); ++i)
                    find_collision(collisions[i].first, collisions[i].second);
            }
            // if little work has been done processing trail collisions then still do some work
            if (collisions.size() * std::size_t(4) < workamount) {
                work.clear();
                _simd_device.fill_trail_buffer(seed, work, bool(maxblocks == 1));
                if (!verified && !work.empty()) {
                    trail_type tmp = work[0];
                    tmp.len = 0;
                    generate_trail(tmp);
                    if (tmp != work[0]) {
                        // found an error: disable SIMD and switch back to normal loop
                        _nosimd = true;
                        loop(single);
                        return;
                    }
                    verified = true;
                }
                LOCK_GLOBAL_MUTEX;
                for (unsigned i = 0; i < work.size(); ++i)
                    distribute_trail(work[i]);
                distribute_trail_flush();
            }
            collisions.clear();
            if (single)
                break;
        }
    }

    void loop(bool single = false) {
#ifdef HAVE_CUDA
        if (_cuda_device_nr >= 0) {
            loop_cuda(single);
            return;
        }
#endif // CUDA

        if (!_nosimd) {
            loop_simd(single);
            return;
        }
        std::size_t workamount = (std::size_t(1) << 26) / std::size_t(distinguishedpointmask + 1);
        std::vector<trail_type> work(workamount);
        std::vector<std::pair<trail_type, trail_type>> collisions;
        while (true) {
            collisions.clear();
            // generate a batch of new trail starting points
            {
                LOCK_GLOBAL_MUTEX;
                if (quit)
                    return;
                main_storage.get_birthdaycollisions(collisions);
                work.resize(workamount);
                for (unsigned i = 0; i < work.size(); ++i) {
                    work[i].start[0] = xrng128();
                    work[i].start[1] = xrng128();
                    work[i].start[2] = xrng128() & hybridmask;
                    work[i].len = 0;
                    xrng128();
                    xrng128();
                }
            }
            if (collisions.size() > 0) {
                for (unsigned i = 0; i < collisions.size(); ++i)
                    find_collision(collisions[i].first, collisions[i].second);
                // if not enough work has been done on processing collisions then generate trails as
                // well if enough work has been done then go to start of loop
                if (collisions.size() * std::size_t(4) >= workamount)
                    continue;
            }

            // generate the trails
            for (unsigned i = 0; i < work.size(); ++i)
                generate_trail(work[i]);

            // insert the trails into the trail hash
            {
                LOCK_GLOBAL_MUTEX;
                for (unsigned i = 0; i < work.size(); ++i)
                    distribute_trail(work[i]);
                distribute_trail_flush();
            }

            if (single)
                break;
        }
    }

    void thread_run() {
        try {
            {
                LOCK_GLOBAL_MUTEX;
#ifdef HAVE_CUDA
                if (_cuda_device_nr >= 0) {
                    std::cout << "Thread " << id << " created (CUDA)." << '\n';
                    if (!_cuda_device.init(_cuda_device_nr, ihv1, ihv2, ihv2mod, msg1, msg2,
                                           hybridmask, distinguishedpointmask, maximumpathlength))
                        return;
                    //					_cuda_device.benchmark();
                } else
#endif // CUDA
                {
                    if (avx2_available() &&
                        _simd_device.init(ihv1, ihv2, ihv2mod, precomp1, precomp2, msg1, msg2,
                                          hybridmask, distinguishedpointmask, maximumpathlength))
                        std::cout << "Thread " << id << " created (AVX256)." << '\n';
                    else {
                        _nosimd = true;
                        std::cout << "Thread " << id << " created." << '\n';
                    }
                }
            }
            loop();
            std::cout << "Thread " << id << " exited.          " << '\n';
        } catch (...) {
            std::cerr << "Exception in thread " << id << "!         " << '\n';
        }
        LOCK_GLOBAL_MUTEX;
    }

    void single_run() { loop(true); }

    void operator()() { thread_run(); }
};
uint32 birthday_thread::id_counter = 1;
struct birthday_thread_shell {
    birthday_thread *bt;
    birthday_thread_shell(birthday_thread *_bt) : bt(_bt) {}
    void operator()() { bt->operator()(); }
};

void birthday(birthday_parameters &parameters) {
    procmodn = parameters.modn;
    procmodi = parameters.modi;
    generatormode = parameters.generatormode;

    trail_distribution.resize(procmodn);

    // add the modi parameter to the seed
    addseed(parameters.modi);

    hybridmask =
        (parameters.hybridbits == 0) ? uint32(0) : (uint32(~0) >> (32 - parameters.hybridbits));
    parameterspathtyperange = parameters.pathtyperange;
    memhardlimit = parameters.memhardlimit;
    uint64 maxtrails = 0;
    if (parameters.maxblocks > 16)
        parameters.maxblocks = 16;
    double logprob =
        log(dist[parameters.hybridbits][parameters.pathtyperange][parameters.maxblocks]) /
        std::numbers::ln2;
    if (parameters.maxblocks == 1) {
        bool loaded = false;
        try {
            std::cout << "Loading 'singleblockdata.bin.bz2'..." << std::flush;
            load_bz2(singleblockdata, "singleblockdata", binary_archive);
            loaded = true;
        } catch (const std::exception &) {
        }
        if (!loaded) {
            std::cout << "failed!" << '\n';
            std::cout << "Loading 'singleblockdata.txt.bz2'..." << std::flush;
            try {
                load_bz2(singleblockdata, "singleblockdata", text_archive);
                loaded = true;
                save_bz2(singleblockdata, "singleblockdata", binary_archive);
            } catch (const std::exception &) {
            }
            if (!loaded) {
                std::cout << "failed!" << '\n';
                return;
            }
        }
        std::cout << "done: " << singleblockdata.size() << " dIHVs loaded." << '\n';
        logprob = (log(double(singleblockdata.size())) / std::numbers::ln2) -
                  (64 - parameters.hybridbits);
    }
    double estcomplexity = 32.825748 - 0.5 * logprob + 0.5 * double(parameters.hybridbits);
    double estcollisions = 1 - logprob;

    if (parameters.maxmemory != 0) {
        maxtrails = uint64(parameters.maxmemory) << 19;
        maxtrails /= sizeof(trail_type);
    }
    if (parameters.logpathlength < 0) {
        double comppertrail = pow(double(2), estcomplexity) / double(maxtrails);
        double logcpt = log(comppertrail) / std::numbers::ln2;
        parameters.logpathlength = unsigned(logcpt + 0.9);
    }
    if (parameters.maxmemory == 0) {
        maxtrails = pow(double(2), estcomplexity - double(parameters.logpathlength));
        uint64 tmp = (maxtrails * sizeof(trail_type)) >> 19;
        parameters.maxmemory = unsigned(tmp);
    }
    std::cout << "Maximum of near-collision blocks: " << parameters.maxblocks << '\n';
    std::cout << "Differential Path Type range: " << parameters.pathtyperange << '\n';
    std::cout << "Hybrid bits: " << parameters.hybridbits << '\n';
    std::cout << "Maximum amount of memory in MB for trails: " << parameters.maxmemory
              << " (local: " << parameters.maxmemory / parameters.modn << ")" << '\n';
    std::cout << "Estimated number of trails that will be stored:    " << maxtrails
              << " (local: " << maxtrails / parameters.modn << ")" << '\n';
    std::cout << "Estimated number of trails that will be generated: "
              << uint64(pow(double(2), estcomplexity - parameters.logpathlength)) << '\n';
    std::cout << "Estimated complexity per trail: 2^(" << parameters.logpathlength << ")" << '\n';
    std::cout << "Estimated complexity on trails: 2^(" << estcomplexity << ")" << '\n';
    std::cout << "Estimated complexity on collisions: 2^("
              << parameters.logpathlength + estcollisions + 1.321928 << ")" << '\n';
    std::cout << '\n';

    distinguishedpointmask = 0;
    for (unsigned k = 0; k < unsigned(parameters.logpathlength) && k < 32; ++k)
        distinguishedpointmask |= 1 << k;

    uint64 mpl = (uint64(1) << parameters.logpathlength) * 20;
    maximumpathlength = (mpl >> 31) ? 0x7FFFFFFF : mpl;

    maxblocks = parameters.maxblocks;

    for (unsigned k = 0; k < 4; ++k) {
        precomp1[k] = ihv1[k] = parameters.ihv1[k];
        precomp2[k] = ihv2[k] = parameters.ihv2[k];
        ihv2mod[k] = ihv2[k];
    }
    if (maxblocks == 1) {
        ihv2mod[0] -= -(1 << 5);
        ihv2mod[3] -= -(1 << 5) + (1 << 25);
        ihv2mod[2] -= -(1 << 5) + (1 << 25) + (1 << 23);
    }
    for (unsigned k = 0; k < 16; ++k) {
        msg1[k] = parameters.msg1[k];
        msg2[k] = parameters.msg2[k];
    }
    precomputestate(precomp1, msg1);
    precomputestate(precomp2, msg2);

    main_storage.set_parameters(parameters);
    main_storage.reserve_memory(maxtrails / parameters.modn);

    if (parameters.threads == 0 || parameters.threads > std::thread::hardware_concurrency())
        parameters.threads = std::thread::hardware_concurrency();

    [[maybe_unused]] int cuda_dev_cnt = 0;
#ifdef HAVE_CUDA
    if (parameters.cuda_enabled) {
        try {
            cuda_dev_cnt = get_num_cuda_devices();
        } catch (std::exception &e) {
            std::cerr << "CUDA ERROR: " << e.what() << '\n';
            cuda_dev_cnt = 0;
        }
        std::cout << "Found " << cuda_dev_cnt << " CUDA devices." << '\n';
    }
    // if possible save 1 cpucore for better cuda latency
    if (parameters.threads > static_cast<unsigned>(cuda_dev_cnt))
        parameters.threads -= static_cast<unsigned>(cuda_dev_cnt);
#endif

    // Declaration order is load-bearing: locals are destroyed in reverse order, so
    // threads_data must be declared first. That way the jthreads are destroyed
    // (and thereby joined) before the birthday_thread objects they reference are
    // freed, also when this function is left by an exception.
    std::vector<std::unique_ptr<birthday_thread>> threads_data;
    std::vector<std::jthread> threads;
    for (unsigned i = 0; i < parameters.threads; ++i) {
        threads_data.push_back(std::make_unique<birthday_thread>());
        threads.emplace_back(birthday_thread_shell(threads_data.back().get()));
    }
#ifdef HAVE_CUDA
    if (parameters.cuda_enabled) {
        for (int i = 0; i < cuda_dev_cnt; ++i) {
            threads_data.push_back(std::make_unique<birthday_thread>(i));
            threads.emplace_back(birthday_thread_shell(threads_data.back().get()));
        }
    }
#endif
    if (threads_data.empty())
        quit = true;

    timer save_timer(true);
    while (!quit) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        if (procmodn > 1 || generatormode) {
            if (save_timer.time() > parameters.saveloadwait) {
                load_save_trails();
                save_timer.start();
            } // else load_save_trails(false);
        }
        LOCK_GLOBAL_MUTEX;
        status_line();
    }
    std::cout << '\n' << "Waiting for threads to finish..." << std::flush;
    for (auto &thread : threads)
        thread.join();
    std::cout << "done." << '\n';
}
