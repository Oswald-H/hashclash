/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <random>
#include <iostream>
#include <hashclash/rng.hpp>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  // open
#include <unistd.h> // read, close
#include <cerrno>   // errno, EINTR
namespace hashclash {
void getosrnd(uint32 buf[256]) {
    constexpr size_t nbytes = 256 * sizeof(uint32);
    size_t got = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        while (got < nbytes) {
            ssize_t n = read(fd, reinterpret_cast<char *>(buf) + got, nbytes - got);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            got += size_t(n);
        }
        close(fd);
    }
    if (got < nbytes) {
        std::random_device rd;
        unsigned char *bytes = reinterpret_cast<unsigned char *>(buf);
        for (size_t i = got; i < nbytes; ++i)
            bytes[i] = static_cast<unsigned char>(rd());
    }
}
} // namespace hashclash
#elif defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
namespace hashclash {
void getosrnd(uint32 buf[256]) {
    HCRYPTPROV g_hCrypt = 0;
    if (!CryptAcquireContext(&g_hCrypt, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        std::random_device rd;
        for (unsigned i = 0; i < 256; ++i)
            buf[i] = rd();
        return;
    }
    CryptGenRandom(g_hCrypt, 256 * 4, reinterpret_cast<BYTE *>(buf));
    CryptReleaseContext(g_hCrypt, 0);
}
} // namespace hashclash
#else
namespace hashclash {
void getosrnd(uint32 buf[256]) {
    // no OS randomness available: fall back to std::random_device
    std::random_device rd;
    for (unsigned i = 0; i < 256; ++i)
        buf[i] = rd();
}
} // namespace hashclash
#endif

namespace hashclash {

void getosrnd(uint32 buf[256]);

uint32 seedd;
uint32 seed32_1;
uint32 seed32_2;
uint32 seed32_3;
uint32 seed32_4;

void seed(uint32 s) {
    seedd = 0;
    seed32_1 = s;
    seed32_2 = 2;
    seed32_3 = 3;
    seed32_4 = 4;
    for (unsigned i = 0; i < 0x1000; ++i)
        xrng128();
}

void seed(uint32 *sbuf, unsigned len) {
    seedd = 0;
    seed32_1 = 1;
    seed32_2 = 2;
    seed32_3 = 3;
    seed32_4 = 4;
    for (unsigned i = 0; i < len; ++i) {
        seed32_1 ^= sbuf[i];
        xrng128();
    }
    for (unsigned i = 0; i < 0x1000; ++i)
        xrng128();
}

void addseed(uint32 s) {
    xrng128();
    seed32_1 ^= s;
    xrng128();
}

void addseed(const uint32 *sbuf, unsigned len) {
    xrng128();
    for (unsigned i = 0; i < len; ++i) {
        seed32_1 ^= sbuf[i];
        xrng128();
    }
}

struct hashclash_rng__init {
    hashclash_rng__init() {
        std::random_device rd;
        seed(uint32(rd()));
        uint32 rndbuf[256] = {};
        addseed(rndbuf, 256);
        getosrnd(rndbuf);
        addseed(rndbuf, 256);
    }
};
hashclash_rng__init hashclash_rng__init__now;

void hashclash_rng_hpp_init() { hashclash_rng__init here; }

} // namespace hashclash
