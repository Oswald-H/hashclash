/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <stdexcept>

#include <filesystem>
#include <thread>
#include <mutex>

#include "main.hpp"
#include "storage.hpp"

std::mutex storage_mutex;

using namespace hashclash;

// service for CUDA part
void thread_yield() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

void storage_type::reserve_memory(uint64 m) {
    std::scoped_lock lock(storage_mutex);
    trail_hash.resize(m >> 5);
    for (unsigned i = 0; i < trail_hash.size(); ++i)
        trail_hash[i].reserve(2 << 5);
}

void storage_type::insert_trail(const trail_type &tr) {
    std::scoped_lock lock(storage_mutex);
    uint32 hashindex = (tr.end[1] / procmodn) % trail_hash.size();
    std::vector<trail_type>::const_iterator cit = trail_hash[hashindex].begin(),
                                            citend = trail_hash[hashindex].end();

    for (; cit != citend; ++cit) {
        if (cit->end[0] == tr.end[0] && cit->end[1] == tr.end[1] && cit->end[2] == tr.end[2]) {
            if (tr != *cit) {
                ++totcoll;
                collisions.push_back(std::make_pair(tr, *cit));
            }
        }
    }
    if (!memhardlimit || trail_hash[hashindex].size() < trail_hash[hashindex].capacity()) {
        trail_hash[hashindex].push_back(tr);
    } else {
        unsigned i = (++tmpptr) % trail_hash[hashindex].size();
        trail_hash[hashindex][i] = tr;
    }
}

void storage_type::insert_trails(const std::vector<trail_type> &trs) {
    std::scoped_lock lock(storage_mutex);
    std::vector<trail_type>::const_iterator cit, citend;
    std::vector<trail_type>::const_iterator trsit = trs.begin(), trsend = trs.end();
    for (; trsit != trsend; ++trsit) {
        uint32 hashindex = (trsit->end[1] / procmodn) % trail_hash.size();
        cit = trail_hash[hashindex].begin();
        citend = trail_hash[hashindex].end();
        for (; cit != citend; ++cit) {
            if (cit->end[0] == trsit->end[0] && cit->end[1] == trsit->end[1] &&
                cit->end[2] == trsit->end[2]) {
                if (*trsit != *cit) {
                    ++totcoll;
                    collisions.push_back(std::make_pair(*trsit, *cit));
                }
            }
        }
        if (!memhardlimit || trail_hash[hashindex].size() < trail_hash[hashindex].capacity()) {
            trail_hash[hashindex].push_back(*trsit);
        } else {
            unsigned i = (++tmpptr) % trail_hash[hashindex].size();
            trail_hash[hashindex][i] = *trsit;
        }
    }
}

void storage_type::get_birthdaycollisions(std::vector<std::pair<trail_type, trail_type>> &coll,
                                          unsigned multiple_of) {
    std::scoped_lock lock(storage_mutex);
    coll.clear();
    if (collisions.size() >= multiple_of) {
        std::size_t count = (collisions.size() / multiple_of) * multiple_of;
        coll.resize(count);
        std::copy(collisions.end() - count, collisions.end(), coll.begin());
        collisions.resize(collisions.size() - count);
    }
}

unsigned storage_type::get_collqueuesize() {
    unsigned k;
    {
        std::scoped_lock lock(storage_mutex);
        k = unsigned(collisions.size());
    }
    return k;
}

unsigned storage_type::get_totcoll() {
    unsigned result;
    {
        std::scoped_lock lock(storage_mutex);
        result = totcoll;
    }
    return result;
}

storage_type main_storage;
