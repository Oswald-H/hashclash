/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#ifndef HASHCLASH_BOOLEANFUNCTION_HPP
#define HASHCLASH_BOOLEANFUNCTION_HPP

#include <array>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <mutex>

#include "types.hpp"
#include "sdr.hpp"
#include "md5detail.hpp"
#include "conditions.hpp"

namespace hashclash {

using bf_conditions = triple<bitcondition, bitcondition, bitcondition>;

struct bf_outcome {
    unsigned char c;

    static const unsigned char fconstant = 1;
    static const unsigned char fplus = 2;
    static const unsigned char fminus = 4;
    bf_outcome() : c(0) {}
    bf_outcome(unsigned char k) : c(k) {}
    bf_outcome(const bf_outcome &) = default;
    bf_outcome &operator=(const bf_outcome &r) {
        c = r.c;
        return *this;
    }
    bf_outcome &operator=(unsigned char k) {
        c = k;
        return *this;
    }

    bool constant() const { return (c & fconstant) != 0; }
    bool plus() const { return (c & fplus) != 0; }
    bool minus() const { return (c & fminus) != 0; }
    unsigned size() const {
        unsigned w = 0;
        if (constant())
            ++w;
        if (plus())
            ++w;
        if (minus())
            ++w;
        return w;
    }
    uint32 operator()(unsigned index, unsigned b) const {
        const uint32 bit = uint32(1) << b;
        if (index == 0) {
            if (constant())
                return 0;
            if (plus())
                return bit;
            return 0u - bit;
        } else if (index == 1) {
            if (constant()) {
                if (plus())
                    return bit;
                return 0u - bit;
            } else
                return 0u - bit;
        } else
            return 0u - bit;
    }
    bitcondition operator[](unsigned index) const {
        if (index == 0) {
            if (constant())
                return bc_constant;
            if (plus())
                return bc_plus;
            return bc_minus;
        } else if (index == 1) {
            if (constant()) {
                if (plus())
                    return bc_plus;
                return bc_minus;
            } else
                return bc_minus;
        } else
            return bc_minus;
    }
};

class booleanfunction {
  public:
    using function_type = uint32 (*)(uint32, uint32, uint32);

    booleanfunction(function_type F, std::string description = "");

    const bf_outcome &outcome(bitcondition input1, bitcondition input2, bitcondition input3) const {
        ensure_initialized();
        return outcome_table[(input1 << 8) + (input2 << 4) + input3];
    }
    const bf_outcome &outcome(const bf_conditions &c) const {
        return outcome(c.first, c.second, c.third);
    }

    const bf_conditions &forwardconditions(bitcondition input1, bitcondition input2,
                                           bitcondition input3, bitcondition outcome) const {
        ensure_initialized();
        return forward_table[(outcome << 12) + (input1 << 8) + (input2 << 4) + input3];
    }
    const bf_conditions &forwardconditions(const bf_conditions &c, bitcondition outcome) const {
        return forwardconditions(c.first, c.second, c.third, outcome);
    }

    const bf_conditions &backwardconditions(bitcondition input1, bitcondition input2,
                                            bitcondition input3, bitcondition outcome) const {
        ensure_initialized();
        return backward_table[(outcome << 12) + (input1 << 8) + (input2 << 4) + input3];
    }
    const bf_conditions &backwardconditions(const bf_conditions &c, bitcondition outcome) const {
        return backwardconditions(c.first, c.second, c.third, outcome);
    }

    uint32 F(uint32 input1, uint32 input2, uint32 input3) const {
        return f(input1, input2, input3);
    }

    const std::string &description() const { return f_description; }

  private:
    void ensure_initialized() const;

    function_type f;
    std::string f_description;

    mutable std::once_flag init_once_;
    // Lookup tables have compile-time sizes; std::array avoids three heap
    // allocations per booleanfunction and improves locality.
    mutable std::array<bf_outcome, static_cast<std::size_t>(bc_max) * 16 * 16> outcome_table{};
    mutable std::array<bf_conditions, 3 * 16 * 16 * 16> forward_table{};
    mutable std::array<bf_conditions, 3 * 16 * 16 * 16> backward_table{};

    mutable std::map<bf_conditions, std::set<uint32>> conds_to_values;

    void find_booleanfunction_outcomes(const bf_conditions &cond, bf_outcome &outcomes,
                                       std::set<uint32> &values) const;
    bf_conditions preferred_conditions(const bf_conditions &cond,
                                       std::vector<bf_conditions> &vec_conds) const;
};

extern booleanfunction MD5_F_data, MD5_G_data, MD5_H_data, MD5_I_data;

} // namespace hashclash

#endif // HASHCLASH_BOOLEANFUNCTION_HPP
