/**************************************************************************\
| HashClash serialization helpers.
\**************************************************************************/
#ifndef HASHCLASH_SAVELOAD_HPP
#define HASHCLASH_SAVELOAD_HPP

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <bzlib.h>
#include <zlib.h>

#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include "differentialpath.hpp"
#include "types.hpp"

namespace hashclash {
namespace fs = std::filesystem;
using fs::path;

enum class archive_type { binary_archive = 0, xml_archive, text_archive };
// Keep the historical unqualified enumerator names available to callers.
using enum archive_type;

namespace detail {
template <class T> void save_stream(const T &value, archive_type type, std::ostream &os) {
    switch (type) {
    case binary_archive: {
        cereal::PortableBinaryOutputArchive ar(os);
        ar(value);
        break;
    }
    case xml_archive:
    case text_archive: {
        cereal::JSONOutputArchive ar(os);
        ar(cereal::make_nvp("saveload", value));
        break;
    }
    default:
        throw std::runtime_error("save(): archive type not supported!");
    }
}

template <class T> void load_stream(T &value, archive_type type, std::istream &is) {
    switch (type) {
    case binary_archive: {
        cereal::PortableBinaryInputArchive ar(is);
        ar(value);
        break;
    }
    case xml_archive:
    case text_archive: {
        cereal::JSONInputArchive ar(is);
        ar(cereal::make_nvp("saveload", value));
        break;
    }
    default:
        throw std::runtime_error("load(): archive type not supported!");
    }
}

inline std::string gzip_compress(std::string input) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) !=
        Z_OK)
        throw std::runtime_error("gzip: initialization failed");
    std::string output(deflateBound(&stream, static_cast<uLong>(input.size())), '\0');
    stream.next_in = reinterpret_cast<Bytef *>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int result = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (result != Z_STREAM_END)
        throw std::runtime_error("gzip: compression failed");
    output.resize(stream.total_out);
    return output;
}

inline std::string gzip_decompress(std::string input) {
    z_stream stream{};
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
        throw std::runtime_error("gzip: initialization failed");
    std::string output;
    char buffer[16384];
    stream.next_in = reinterpret_cast<Bytef *>(input.data());
    stream.avail_in = static_cast<uInt>(input.size());
    int result;
    do {
        stream.next_out = reinterpret_cast<Bytef *>(buffer);
        stream.avail_out = sizeof(buffer);
        result = inflate(&stream, Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END) {
            inflateEnd(&stream);
            throw std::runtime_error("gzip: decompression failed");
        }
        output.append(buffer, sizeof(buffer) - stream.avail_out);
    } while (result != Z_STREAM_END);
    inflateEnd(&stream);
    return output;
}

inline std::string bz2_compress(std::string input) {
    unsigned int size = static_cast<unsigned int>(input.size() + input.size() / 100 + 601);
    std::string output(size, '\0');
    int result;
    do {
        result = BZ2_bzBuffToBuffCompress(output.data(), &size, input.data(),
                                          static_cast<unsigned int>(input.size()), 9, 0, 30);
        if (result == BZ_OUTBUFF_FULL) {
            output.resize(output.size() * 2);
            size = static_cast<unsigned int>(output.size());
        }
    } while (result == BZ_OUTBUFF_FULL);
    if (result != BZ_OK)
        throw std::runtime_error("bzip2: compression failed");
    output.resize(size);
    return output;
}

inline std::string bz2_decompress(std::string input) {
    unsigned int size = static_cast<unsigned int>(input.size() * 8 + 1024);
    std::string output(size, '\0');
    int result;
    do {
        result = BZ2_bzBuffToBuffDecompress(output.data(), &size, input.data(),
                                            static_cast<unsigned int>(input.size()), 0, 0);
        if (result == BZ_OUTBUFF_FULL) {
            output.resize(output.size() * 2);
            size = static_cast<unsigned int>(output.size());
        }
    } while (result == BZ_OUTBUFF_FULL);
    if (result != BZ_OK)
        throw std::runtime_error("bzip2: decompression failed");
    output.resize(size);
    return output;
}

template <typename Compressor>
concept string_compressor = requires(Compressor compressor, const std::string &input) {
    { compressor(input) } -> std::convertible_to<std::string>;
};

template <typename Decompressor>
concept string_decompressor = string_compressor<Decompressor>;

template <class T, string_compressor Compressor>
void save_compressed(const T &value, archive_type type, const path &file, Compressor compressor) {
    std::ostringstream serialized(std::ios::binary);
    save_stream(value, type, serialized);
    const std::string data = compressor(serialized.str());
    std::ofstream output(file, std::ios::binary);
    if (!output)
        throw std::runtime_error("save_compressed(): could not open file!");
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
        throw std::runtime_error("save_compressed(): write error!");
}

// Reads a whole regular file into a string without an intermediate stream copy.
inline std::string read_file(const path &file) {
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    if (!input)
        throw std::runtime_error("load_compressed(): could not open file!");
    const std::streampos end = input.tellg();
    if (end < 0)
        throw std::runtime_error("load_compressed(): could not determine file size!");
    std::string data(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    input.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (!input)
        throw std::runtime_error("load_compressed(): read error!");
    return data;
}

template <class T, string_decompressor Decompressor>
void load_compressed(T &value, archive_type type, const path &file, Decompressor decompressor) {
    std::istringstream serialized(decompressor(read_file(file)), std::ios::binary);
    load_stream(value, type, serialized);
}

// Maps wrapper value types to the type name used in diagnostic messages.
template <class T> constexpr const char *saveload_type_name();
template <> constexpr const char *saveload_type_name<differentialpath>() {
    return "differentialpath";
}
template <> constexpr const char *saveload_type_name<std::vector<differentialpath>>() {
    return "vector<differentialpath>";
}
} // namespace detail

template <class T> void save(const T &value, const std::string &file, archive_type type) {
    save(value, type,
         path(file + (type == binary_archive ? ".bin"
                      : type == xml_archive  ? ".xml"
                                             : ".txt")));
}
template <class T> void save(const T &value, archive_type type, const path &file) {
    std::ofstream output(file, type == binary_archive ? std::ios::binary : std::ios::out);
    if (!output)
        throw std::runtime_error("save(): could not open file!");
    detail::save_stream(value, type, output);
    if (!output)
        throw std::runtime_error("save(): write error!");
}
template <class T> void load(T &value, const std::string &file, archive_type type) {
    load(value, type,
         path(file + (type == binary_archive ? ".bin"
                      : type == xml_archive  ? ".xml"
                                             : ".txt")));
}
template <class T> void load(T &value, archive_type type, const path &file) {
    std::ifstream input(file, type == binary_archive ? std::ios::binary : std::ios::in);
    if (!input)
        throw std::runtime_error("load(): could not open file!");
    detail::load_stream(value, type, input);
    // Text/JSON archives consume the stream up to (and including) EOF, which
    // sets failbit; only a hard stream error indicates a failed load here.
    if (input.bad())
        throw std::runtime_error("load(): read error!");
}

// Save-only wrapper: holds a const reference and writes the type magic.
// Versioned selects the same cereal entry point as the matching load wrapper,
// keeping the on-disk format identical (cereal writes a class version field
// for versioned serialize functions).
template <class T, uint32 Magic, bool Versioned = false> struct saveload_const {
    const T &p;
    uint32 magic;
    saveload_const(const T &value) : p(value), magic(Magic) {}
    template <class Archive> void serialize_impl(Archive &ar) {
        ar(cereal::make_nvp("magic", magic));
        if (magic != Magic)
            throw std::runtime_error(std::string("serialize(): type is not ") +
                                     detail::saveload_type_name<T>());
        ar(cereal::make_nvp("path", p));
    }
    template <class Archive>
        requires(!Versioned)
    void serialize(Archive &ar) {
        serialize_impl(ar);
    }
    template <class Archive>
        requires Versioned
    void serialize(Archive &ar, const unsigned int /*file_version*/) {
        serialize_impl(ar);
    }
};
using saveload_const_diffpath = saveload_const<differentialpath, 0x23580001>;
using saveload_const_vecdiffpath = saveload_const<std::vector<differentialpath>, 0x23580002>;

/* specialization types to distinguish between differentialpath & vector<differentialpath> */
struct saveload_diffpath {
    differentialpath &p;
    uint32 magic;
    saveload_diffpath(differentialpath &value) : p(value), magic(0x23580001) {}
    template <class Archive> void serialize(Archive &ar) {
        ar(cereal::make_nvp("magic", magic));
        if (magic != 0x23580001)
            throw std::runtime_error("serialize(): type is not differentialpath");
        ar(cereal::make_nvp("path", p));
    }
};
template <> inline void save(const differentialpath &value, archive_type type, const path &file) {
    save(saveload_const_diffpath(value), type, file);
}
template <> inline void load(differentialpath &value, archive_type type, const path &file) {
    saveload_diffpath tmp(value);
    load(tmp, type, file);
}

struct saveload_vecdiffpath {
    std::vector<differentialpath> &p;
    uint32 magic;
    saveload_vecdiffpath(std::vector<differentialpath> &value) : p(value), magic(0x23580002) {}
    template <class Archive> void serialize(Archive &ar) {
        ar(cereal::make_nvp("magic", magic));
        if (magic != 0x23580002)
            throw std::runtime_error("serialize(): type is not vector<differentialpath>");
        ar(cereal::make_nvp("path", p));
    }
};
template <>
inline void save(const std::vector<differentialpath> &value, archive_type type, const path &file) {
    save(saveload_const_vecdiffpath(value), type, file);
}
template <>
inline void load(std::vector<differentialpath> &value, archive_type type, const path &file) {
    saveload_vecdiffpath tmp(value);
    load(tmp, type, file);
}

} // namespace hashclash
#endif
