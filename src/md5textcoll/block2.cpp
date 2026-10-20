/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <cmath>
#include <algorithm>
#include <map>
#include <stdexcept>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <random>
#include <atomic>

#include <hashclash/saveload_gz.hpp>
#include <hashclash/md5detail.hpp>
#include <hashclash/differentialpath.hpp>
#include <hashclash/booleanfunction.hpp>
#include <hashclash/rng.hpp>
#include <hashclash/timer.hpp>
#include <hashclash/progress_display.hpp>

#include "main.hpp"

// #define CPUPERFORMANCE
#ifdef CPUPERFORMANCE
#include <hashclash/cpuperformance.hpp>
// uint64 cpu_step_t[64];
#define UPDATE(s) update_performance_counter __update(cpu_step_t[s]);
#ifdef __GNUC__
#include <sched.h>
#endif
#else
#define UPDATE(s)
#endif

#ifdef DOTESTCOUNTS
#define TESTCOUNT(s) ++testcounts[s];
#else
#define TESTCOUNT(s)
#endif

bool test_collision(const uint32_t ihv1[4], const uint32_t ihv2[4], const uint32_t m[16]) {
    uint32_t tihv1[4];
    uint32_t tihv2[4];
    for (unsigned k = 0; k < 4; ++k) {
        tihv1[k] = ihv1[k];
        tihv2[k] = ihv2[k];
    }
    md5compress(tihv1, m);
    md5compress(tihv2, m);
    if (tihv1[0] == tihv2[0])
        std::cout << "partial solution found" << '\n';
    bool collision = true;
    for (unsigned k = 0; k < 4; ++k)
        if (tihv1[k] != tihv2[k])
            collision = false;
    return collision;
}

void textcoll_solver_t::start_block2() {
    fullstate_t S1, S2;
    S1.Qt(-3) = ihv1[0];
    S1.Qt(-2) = ihv1[3];
    S1.Qt(-1) = ihv1[2];
    S1.Qt(0) = ihv1[1];
    S2.Qt(-3) = ihv2[0];
    S2.Qt(-2) = ihv2[3];
    S2.Qt(-1) = ihv2[2];
    S2.Qt(0) = ihv2[1];

    std::cout << "MSBs: ";
    for (int t = 0; t >= -3; --t)
        std::cout << (S1.Qt(t) >> 31);
    std::cout << '\n';
    if ((S1.Qt(0) >> 31) != (S1.Qt(-1) >> 31) || (S1.Qt(-1) >> 31) != (S1.Qt(-2) >> 31))
        throw std::runtime_error(
            "IHV1 and IHV2 do not fulfill the necessary conditions such that dF0,dF1 = 1<<31.");

    /*
        MA = message_alphabet("()");
        std::vector<uint32_t> words;
        for (uint32_t m0 : MA.word_range(0))
            words.emplace_back(m0);

        MA.set_byte_alphabet(0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(1, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(2, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(3, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(4, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(5, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        MA.set_byte_alphabet(6, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        //MA.set_byte_alphabet(7, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    */
    run_guarded(threads, [this](size_t ji, size_t) {
        fullstate_t S1;
        localxrng rng;
        counter_exponential_print t15ok("t15ok");
        counter_exponential_print t0ok("t0ok");
        S1.Qt(-3) = ihv1[0];
        S1.Qt(-2) = ihv1[3];
        S1.Qt(-1) = ihv1[2];
        S1.Qt(0) = ihv1[1];
        while (true) {
            if (solution_found.load())
                return;
            /*
                S1.m[0] = MA.sampleword(0);

                if (S1.m[0] % 41 != ji) // split the search space
                    continue;

                S1.computeQtp1(0);
                if ( (S1.Qt(1)>>31) != (S1.Qt(0)>>31) )
                    continue;
                if (ji == 0) ++t0ok;

            */
            int t = 0;
            for (; t < 13; ++t) {
                if (t < 0)
                    continue;
                for (int i = 0; i < 16; ++i) // while (true) //for (uint32_t mt : words)
                {
                    S1.m[t] = // mt; //words[xrng64() % words.size()];
                        MA.sampleword(t, rng);
                    S1.computeQtp1(t);
                    if ((S1.Qt(t + 1) >> 31) == (S1.Qt(t) >> 31))
                        break;
                }
                if ((S1.Qt(t + 1) >> 31) != (S1.Qt(t) >> 31)) {
                    t -= 4;
                    continue;
                    break;
                }
            }
            //			if (t < 12) continue;

            /*
                        for (uint32_t m1 : MA.word_range(1))
                        {
                            S1.m[1] = m1; S1.computeQtp1(1); if ( (S1.Qt(2)>>31) != (S1.Qt(0)>>31) )
               continue;

                        for (uint32_t m2 : words)//MA.word_range(13))
                        {
                            S1.m[2] = m2; S1.computeQtp1(2); if ( (S1.Qt(3)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m3 : words)//MA.word_range(13))
                        {
                            S1.m[3] = m3; S1.computeQtp1(3); if ( (S1.Qt(4)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m4 : words)//MA.word_range(13))
                        {
                            S1.m[4] = m4; S1.computeQtp1(4); if ( (S1.Qt(5)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m5 : words)//MA.word_range(13))
                        {
                            S1.m[5] = m5; S1.computeQtp1(5); if ( (S1.Qt(6)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m6 : words)//MA.word_range(13))
                        {
                            S1.m[6] = m6; S1.computeQtp1(6); if ( (S1.Qt(7)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m7 : words)//MA.word_range(13))
                        {
                            S1.m[7] = m7; S1.computeQtp1(7); if ( (S1.Qt(8)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m8 : words)//MA.word_range(13))
                        {
                            S1.m[8] = m8; S1.computeQtp1(8); if ( (S1.Qt(9)>>31) != (S1.Qt(0)>>31) )
               continue; for (uint32_t m9 : words)//MA.word_range(13))
                        {
                            S1.m[9] = m9; S1.computeQtp1(9); if ( (S1.Qt(10)>>31) != (S1.Qt(0)>>31)
               ) continue; for (uint32_t m10 : words)//MA.word_range(13))
                        {
                            S1.m[10] = m10; S1.computeQtp1(10); if ( (S1.Qt(11)>>31) !=
               (S1.Qt(0)>>31) ) continue;

                        for (uint32_t m11 : words)//MA.word_range(13))
                        {
                            S1.m[11] = m11; S1.computeQtp1(11); if ( (S1.Qt(12)>>31) !=
               (S1.Qt(0)>>31) ) continue;

                        for (uint32_t m12 : words)//MA.word_range(13))
                        {
                            S1.m[12] = m12; S1.computeQtp1(12); if ( (S1.Qt(13)>>31) !=
               (S1.Qt(0)>>31) ) continue;
            */
            for (uint32_t m13 : MA.word_range(13)) {
                if (solution_found.load())
                    return;
                S1.m[13] = m13;
                S1.computeQtp1(13);
                if ((S1.Qt(14) >> 31) != (S1.Qt(0) >> 31))
                    continue;
                for (uint32_t m14 : MA.word_range(14)) {
                    if (solution_found.load())
                        return;
                    S1.m[14] = m14;
                    S1.computeQtp1(14);
                    if ((S1.Qt(15) >> 31) != (S1.Qt(0) >> 31))
                        continue;

                    for (uint32_t m15 : MA.word_range(15)) {
                        S1.m[15] = m15; // S1.computeQtp1(15); if ( (S1.Qt(16)>>31) !=
                                        // (S1.Qt(0)>>31) ) continue;
                        if (ji == 0)
                            ++t15ok;
                        if (test_collision(ihv1, ihv2, S1.m)) {

                            std::cout << "MSG: ";
                            for (unsigned t = 0; t < 16; ++t) {
                                for (unsigned b = 0; b < 4; ++b)
                                    std::cout << char((S1.m[t] >> (8 * b)) & 0xFF);
                            }
                            std::cout << '\n';
                            std::cout << " ===== FULL SOLUTION FOUND =====" << '\n';
                            uint32 x = xrng128_locked();
                            std::string filename1 =
                                "textcoll1_block2_" + std::to_string(x) + ".txt";
                            std::string filename2 =
                                "textcoll2_block2_" + std::to_string(x) + ".txt";
                            std::ofstream of1(filename1.c_str(), std::ios::binary);
                            std::ofstream of2(filename2.c_str(), std::ios::binary);
                            if ((!of1) || (!of2)) {
                                std::cerr << "Cannot open output files!" << '\n';
                            }
                            save_block(of1, S1.m);
                            save_block(of2, S1.m);
                            of1.close();
                            of2.close();
                            throw full_solution_found{};
                        }
                    } // m15
                } // m14
            } // m13
            /*			} // m12
                        } // m11
                        } // m10
                        } // m9
                        } // m8
                        } // m7
                        } // m6
                        } // m5
                        } // m4
                        } // m3
                        } // m2
                        } // m1*/
        }
    });
}
