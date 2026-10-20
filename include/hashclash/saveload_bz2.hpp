#ifndef HASHCLASH_SAVELOAD_BZ2_HPP
#define HASHCLASH_SAVELOAD_BZ2_HPP
#include "saveload.hpp"
namespace hashclash {
template <class T> void save_bz2(const T &v, const std::string &f, archive_type a) {
    save_bz2(v, a,
             path(f + (a == binary_archive ? ".bin.bz2"
                       : a == xml_archive  ? ".xml.bz2"
                                           : ".txt.bz2")));
}
template <class T> void save_bz2(const T &v, archive_type a, const path &f) {
    detail::save_compressed(v, a, f, detail::bz2_compress);
}
template <class T> void load_bz2(T &v, const std::string &f, archive_type a) {
    load_bz2(v, a,
             path(f + (a == binary_archive ? ".bin.bz2"
                       : a == xml_archive  ? ".xml.bz2"
                                           : ".txt.bz2")));
}
template <class T> void load_bz2(T &v, archive_type a, const path &f) {
    detail::load_compressed(v, a, f, detail::bz2_decompress);
}
template <> inline void save_bz2(const differentialpath &v, archive_type a, const path &f) {
    save_bz2(saveload_const_diffpath(v), a, f);
}
template <> inline void load_bz2(differentialpath &v, archive_type a, const path &f) {
    saveload_diffpath t(v);
    load_bz2(t, a, f);
}
template <>
inline void save_bz2(const std::vector<differentialpath> &v, archive_type a, const path &f) {
    save_bz2(saveload_const_vecdiffpath(v), a, f);
}
template <> inline void load_bz2(std::vector<differentialpath> &v, archive_type a, const path &f) {
    saveload_vecdiffpath t(v);
    load_bz2(t, a, f);
}
} // namespace hashclash
#endif
