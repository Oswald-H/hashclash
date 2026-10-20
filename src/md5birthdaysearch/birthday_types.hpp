/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef BIRTHDAY_TYPES_HPP
#define BIRTHDAY_TYPES_HPP

#include <utility>
#include <vector>

#include <hashclash/aligned_buffer.hpp>

// service for CUDA
void thread_yield();

// parameters
struct birthday_parameters {
    birthday_parameters()
        : threads(1), sputhreads(0), modn(1), modi(0), generatormode(false), inputfile1(),
          inputfile2(), outputfile1(), outputfile2(), logpathlength(-1), maxblocks(16),
          saveloadwait(60), memhardlimit(false), distribution(false), cuda_enabled(false) {}
    unsigned threads;
    unsigned sputhreads;
    unsigned modn, modi;
    bool generatormode;
    std::string inputfile1, inputfile2;
    std::string outputfile1, outputfile2;
    int logpathlength;
    unsigned maxblocks;
    unsigned hybridbits, pathtyperange;
    unsigned maxmemory; // in MB, this is NOT a hard limit
    unsigned saveloadwait;
    bool memhardlimit;
    bool distribution;
    bool cuda_enabled;
    uint32 ihv1[4];
    uint32 ihv2[4];
    uint32 ihv2mod[4];
    uint32 msg1[16];
    uint32 msg2[16];
};

struct trail_type {
    uint32 start[3];
    uint32 end[3];
    uint32 len;

// CUDA compiler fails when using non-trivial constructors on device memory
#ifndef TRAIL_NOCONSTRUCTOR
    trail_type() : len(0) {}
#endif

    bool operator==(const trail_type &rhs) const {
        return start[0] == rhs.start[0] && start[1] == rhs.start[1] && start[2] == rhs.start[2] &&
               end[0] == rhs.end[0] && end[1] == rhs.end[1] && end[2] == rhs.end[2] &&
               len == rhs.len;
    }
    bool operator!=(const trail_type &rhs) const { return !(*this == rhs); }
};

class cuda_device_detail;
class cuda_device {
  public:
    bool init(uint32 device, const uint32 ihv1[4], const uint32 ihv2[4], const uint32 ihv2mod[4],
              const uint32 msg1[16], const uint32 msg2[16], uint32 hmask, uint32 dpmask,
              uint32 maxlen);
    void cuda_fill_trail_buffer(uint32 id, uint64 seed, std::vector<trail_type> &buffer,
                                std::vector<std::pair<trail_type, trail_type>> &collisions,
                                bool mod = false);
    void benchmark();

  private:
    cuda_device_detail *detail;
};

struct simd_avx256_detail;
class simd_device_avx256 {
  public:
    simd_device_avx256() = default;
    ~simd_device_avx256();
    simd_device_avx256(const simd_device_avx256 &) = delete;
    simd_device_avx256 &operator=(const simd_device_avx256 &) = delete;

    bool init(const uint32 ihv1[4], const uint32 ihv2[4], const uint32 ihv2mod[4],
              const uint32 precomp1[4], const uint32 precomp2[4], const uint32 msg1[16],
              const uint32 msg2[16], uint32 hmask, uint32 dpmask, uint32 maxlen);
    void fill_trail_buffer(uint64 seed, std::vector<trail_type> &buffer, bool mod = false);

  private:
    [[maybe_unused]] simd_avx256_detail *detail = nullptr;
    [[maybe_unused]] hashclash::aligned_buffer detail_storage;
};

#endif // BIRTHDAY_TYPES_HPP
