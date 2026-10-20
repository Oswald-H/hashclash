#ifndef HASHCLASH_SAVELOAD_GZ_HPP
#define HASHCLASH_SAVELOAD_GZ_HPP
#include "saveload.hpp"
namespace hashclash {
template <class T> void save_gz(const T &v, const std::string &f, archive_type a) {
    save_gz(v, a,
            path(f + (a == binary_archive ? ".bin.gz"
                      : a == xml_archive  ? ".xml.gz"
                                          : ".txt.gz")));
}
template <class T> void save_gz(const T &v, archive_type a, const path &f) {
    detail::save_compressed(v, a, f, detail::gzip_compress);
}
template <class T> void load_gz(T &v, const std::string &f, archive_type a) {
    load_gz(v, a,
            path(f + (a == binary_archive ? ".bin.gz"
                      : a == xml_archive  ? ".xml.gz"
                                          : ".txt.gz")));
}
template <class T> void load_gz(T &v, archive_type a, const path &f) {
    detail::load_compressed(v, a, f, detail::gzip_decompress);
}
template <> inline void save_gz(const differentialpath &v, archive_type a, const path &f) {
    save_gz(saveload_const_diffpath(v), a, f);
}
template <> inline void load_gz(differentialpath &v, archive_type a, const path &f) {
    saveload_diffpath t(v);
    load_gz(t, a, f);
}
template <>
inline void save_gz(const std::vector<differentialpath> &v, archive_type a, const path &f) {
    save_gz(saveload_const_vecdiffpath(v), a, f);
}
template <> inline void load_gz(std::vector<differentialpath> &v, archive_type a, const path &f) {
    saveload_vecdiffpath t(v);
    load_gz(t, a, f);
}
} // namespace hashclash
#endif
