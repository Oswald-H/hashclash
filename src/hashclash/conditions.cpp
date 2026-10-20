/* SPDX-License-Identifier: GPL-3.0-or-later
 * See LICENSE for the upstream notice and full license text.
 */

#include <stdexcept>

#include <hashclash/conditions.hpp>

namespace hashclash {

std::ostream &operator<<(std::ostream &o, const bitcondition &v) {
    switch (v) {
    case bc_constant:
        o << '.';
        break;
    case bc_plus:
        o << '+';
        break;
    case bc_minus:
        o << '-';
        break;
    case bc_zero:
        o << '0';
        break;
    case bc_one:
        o << '1';
        break;
    case bc_prev:
        o << '^';
        break;
    case bc_prevn:
        o << '!';
        break;
    case bc_prev2:
        o << 'm';
        break;
    case bc_prev2n:
        o << '#';
        break;
    case bc_or2:
        o << '?';
        break;
    case bc_next:
        o << 'V';
        break;
    case bc_nextn:
        o << 'Y';
        break;
    case bc_next2:
        o << 'W';
        break;
    case bc_next2n:
        o << 'H';
        break;
    case bc_or2b:
        o << 'Q';
        break;
    default:
        throw std::logic_error("conditions.cpp: operator<<: unknown bitcondition");
    }
    return o;
}

std::istream &operator>>(std::istream &i, bitcondition &v) {
    char c;
    if (!(i >> c))
        return i;
    switch (c) {
    case '.':
        v = bc_constant;
        break;
    case '+':
        v = bc_plus;
        break;
    case '-':
        v = bc_minus;
        break;
    case '0':
        v = bc_zero;
        break;
    case '1':
        v = bc_one;
        break;
    case '^':
        v = bc_prev;
        break;
    case '!':
        v = bc_prevn;
        break;
    case 'm':
        v = bc_prev2;
        break;
    case '#':
        v = bc_prev2n;
        break;
    case '?':
        v = bc_or2;
        break;
    case 'V':
        v = bc_next;
        break;
    case 'Y':
        v = bc_nextn;
        break;
    case 'W':
        v = bc_next2;
        break;
    case 'H':
        v = bc_next2n;
        break;
    case 'Q':
        v = bc_or2b;
        break;
    default:
        i.putback(c);
        i.setstate(std::ios::failbit);
    }
    return i;
}

std::ostream &operator<<(std::ostream &o, const byteconditions &bc) {
    for (unsigned b = 8; b > 0; --b)
        o << bc[b - 1];
    return o;
}

std::istream &operator>>(std::istream &i, byteconditions &bc) {
    unsigned b = 8;
    while ((i) && b > 0) {
        bitcondition tmp = bc_constant;
        i >> tmp;
        if (i)
            bc.set(b - 1, tmp);
        --b;
    }
    return i;
}

std::ostream &operator<<(std::ostream &o, const wordconditions &w) {
    return o << '|' << w.bytes[3] << ' ' << w.bytes[2] << ' ' << w.bytes[1] << ' ' << w.bytes[0]
             << '|';
}

std::istream &operator>>(std::istream &i, wordconditions &w) {
    char c;
    if (!(i >> c))
        return i;
    if (c != '|') {
        i.putback(c);
        i.setstate(std::ios::failbit);
        return i;
    }
    // The byte separators must be read without skipping whitespace: the bytes themselves are
    // extracted with formatted reads, which would otherwise skip the spaces and fail to
    // parse the output of operator<<.
    for (int index : {3, 2, 1, 0}) {
        if (!(i >> w.bytes[index]))
            return i;
        if (!(i.get(c)))
            return i;
        if (index == 0 ? c != '|' : c != ' ') {
            i.putback(c);
            i.setstate(std::ios::failbit);
            return i;
        }
    }
    return i;
}

} // namespace hashclash
