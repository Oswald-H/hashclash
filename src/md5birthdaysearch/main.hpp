/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>
#include <vector>
#include <string>

#include <filesystem>

#include <hashclash/types.hpp>
#include <hashclash/saveload_gz.hpp>
#include <hashclash/saveload_bz2.hpp>

using namespace hashclash;

#include "birthday_types.hpp"

namespace cereal {
template <class Archive> void serialize(Archive &ar, trail_type &t) {
    ar(cereal::make_nvp("start0", t.start[0]), cereal::make_nvp("start1", t.start[1]),
       cereal::make_nvp("start2", t.start[2]), cereal::make_nvp("end0", t.end[0]),
       cereal::make_nvp("end1", t.end[1]), cereal::make_nvp("end2", t.end[2]),
       cereal::make_nvp("len", t.len));
}
} // namespace cereal

extern std::string workdir;
extern uint32 colla1, collb1, collc1, colla2, collb2, collc2;

int dostep(birthday_parameters &parameters);
void birthday(birthday_parameters &parameters);
void determine_nrblocks_distribution(birthday_parameters &parameters);

unsigned load_block(std::istream &i, uint32 block[]);
void save_block(std::ostream &o, uint32 block[]);

// cuda

#ifdef HASHCLASH_HAVE_CUDA
#define HAVE_CUDA
#endif

void cuda_device_query();
int get_num_cuda_devices();

#endif // MAIN_HPP
