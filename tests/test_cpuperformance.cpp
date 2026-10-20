#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include <hashclash/cpu_avx2.hpp>
#include <hashclash/cpuperformance.hpp>

TEST_CASE("cpu_timestamp is callable and non-decreasing") {
    const hashclash::uint64 first = hashclash::cpu_timestamp();
    const hashclash::uint64 second = hashclash::cpu_timestamp();

    CHECK(first <= second);
}

TEST_CASE("avx2_usable requires CPU support, OSXSAVE and saved YMM state") {
    const std::uint32_t avx2_bit = std::uint32_t(1) << 5;
    const std::uint64_t xmm_and_ymm = std::uint64_t(0x6);

    CHECK(hashclash::avx2_usable(true, avx2_bit, xmm_and_ymm));

    // CPU does not advertise AVX2 (CPUID leaf 7 EBX bit 5), e.g. a pre-Haswell
    // CPU where the next lower CPUID bit is set instead.
    CHECK_FALSE(hashclash::avx2_usable(true, std::uint32_t(1) << 4, xmm_and_ymm));
    CHECK_FALSE(hashclash::avx2_usable(true, 0, xmm_and_ymm));

    // OS does not enable XGETBV/XSETBV at all, or does not save the YMM state
    // that the AVX2 instructions touch.
    CHECK_FALSE(hashclash::avx2_usable(false, avx2_bit, xmm_and_ymm));
    CHECK_FALSE(hashclash::avx2_usable(true, avx2_bit, xmm_and_ymm & ~std::uint64_t(0x2)));
    CHECK_FALSE(hashclash::avx2_usable(true, avx2_bit, std::uint64_t(0x2)));
    CHECK_FALSE(hashclash::avx2_usable(true, avx2_bit, 0));
}

// The probe must never claim AVX2 on a build or CPU that cannot run it.
#if !defined(HASHCLASH_HAVE_AVX2)
TEST_CASE("the AVX2 probe reports no AVX2 when the build has no AVX2 code") {
    // e.g. arm64 or Windows on ARM: no AVX2 translation unit was compiled, so
    // the birthday search must stay on the scalar path.
    CHECK_FALSE(hashclash::avx2_available());
}
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
TEST_CASE("the AVX2 probe agrees with the compiler's own AVX2 query") {
    // HASHCLASH_ENABLE_AVX2=ON does not imply the CPU has AVX2; on a CPU
    // without it the probe must report false so the scalar path is used.
    CHECK(hashclash::avx2_available() == (__builtin_cpu_supports("avx2") != 0));
}
#endif
