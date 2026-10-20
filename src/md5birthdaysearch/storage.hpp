/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef STORAGE_HPP
#define STORAGE_HPP

// to be extended to support hard drive storage

class storage_type {
  public:
    storage_type() : tmpptr(0), totcoll(0) {}
    ~storage_type() {}

    void set_parameters(const birthday_parameters &parameters) {
        procmodn = parameters.modn;
        memhardlimit = parameters.memhardlimit;
    }

    void reserve_memory(uint64 m);
    void insert_trail(const trail_type &tr);
    void insert_trails(const std::vector<trail_type> &trs);
    void get_birthdaycollisions(std::vector<std::pair<trail_type, trail_type>> &collisions,
                                unsigned multiple_of = 1);
    unsigned get_collqueuesize();
    unsigned get_totcoll();

  private:
    std::vector<std::vector<trail_type>> trail_hash;
    std::vector<std::pair<trail_type, trail_type>> collisions;
    unsigned procmodn;
    bool memhardlimit;
    uint32 tmpptr;

    unsigned totcoll;
};

extern storage_type main_storage;

#endif // STORAGE_HPP
