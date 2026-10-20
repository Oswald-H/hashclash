# HashClash

HashClash is a fork of [Marc Stevens' HashClash](https://github.com/cr-marcstevens/hashclash).
It provides command-line tools for studying and generating MD5 and SHA-1 collisions.

This fork primarily updates the build system and toolchain while retaining the original
cryptanalytic tools and workflows. It requires a C++20 compiler and CMake 3.25 or newer.

## Build

Configure and build the optimized Release preset:

```sh
cmake --preset release
cmake --build --preset release
```

The executables are written to `build/release/bin`; add that directory to `PATH` before running the
examples.

For test and sanitizer builds, vcpkg setup, and other development details, see
[`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md).

## Usage

Generate two different files with the same MD5 digest:

```sh
md5_fastcoll -o collision1.bin collision2.bin
```

The `-o` option requires exactly two output filenames. To prepend the same file to both messages,
use `-p`:

```sh
md5_fastcoll -p prefix.bin -o prefixed1.bin prefixed2.bin
```

The prefix is copied into both outputs. If it ends partway through a 64-byte MD5 block, the
generator pads that block with zero bytes before appending the collision blocks.

Run `md5_fastcoll --help` for the available options.

## License

The MIT and GPL-3.0-or-later license texts are in [LICENSE](LICENSE). Some bundled files have
additional terms; the classic MD5 generator retains its research and education-use notice.
