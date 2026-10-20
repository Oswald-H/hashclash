/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_CPU_AVX2_HPP
#define HASHCLASH_CPU_AVX2_HPP

#include <cstdint>

#include <hashclash/config.h>

#ifdef _MSC_VER
#include <intrin.h>    // __cpuid, __cpuidex
#include <immintrin.h> // _xgetbv
#endif

namespace hashclash {

// AVX2 code may only run when the CPU advertises AVX2 (CPUID leaf 7, subleaf 0,
// EBX bit 5) *and* the OS preserves the extended register state on context
// switches: OSXSAVE (CPUID leaf 1, ECX bit 27) says XGETBV/XSETBV are available,
// and XCR0 must then report the XMM (bit 1) and YMM (bit 2) state as enabled.
// This is the same predicate GCC/Clang evaluate for
// __builtin_cpu_supports("avx2").
inline constexpr bool avx2_usable(bool osxsave, std::uint32_t cpuid_leaf7_ebx, std::uint64_t xcr0) {
    constexpr std::uint32_t cpuid_leaf7_avx2 = std::uint32_t(1) << 5;
    constexpr std::uint64_t xcr0_xmm_ymm = (std::uint64_t(1) << 1) | (std::uint64_t(1) << 2);
    return osxsave && (cpuid_leaf7_ebx & cpuid_leaf7_avx2) != 0 &&
           (xcr0 & xcr0_xmm_ymm) == xcr0_xmm_ymm;
}

// Runtime AVX2 availability for code guarded by HASHCLASH_HAVE_AVX2. Returns
// false whenever this build contains no AVX2 code, and false on any CPU that
// cannot execute it, so that callers can always fall back to the scalar path.
inline bool avx2_available() {
#if !defined(HASHCLASH_HAVE_AVX2)
    return false;
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    return __builtin_cpu_supports("avx2") != 0;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int regs[4] = {0, 0, 0, 0};
    __cpuid(regs, 1);
    const bool osxsave = (regs[2] & (1 << 27)) != 0;
    // XGETBV raises #UD when OSXSAVE is clear, so read XCR0 only if it is set.
    const std::uint64_t xcr0 = osxsave ? _xgetbv(0) : 0;
    __cpuidex(regs, 7, 0);
    return avx2_usable(osxsave, static_cast<std::uint32_t>(regs[1]), xcr0);
#else
    // MSVC targeting ARM64, or any other architecture we have no probe for.
    return false;
#endif
}

} // namespace hashclash

#endif // HASHCLASH_CPU_AVX2_HPP
