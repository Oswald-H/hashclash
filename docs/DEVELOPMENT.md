# Development

## Toolchain

| Tool         | Purpose                                    |
| ------------ | ------------------------------------------ |
| CMake ≥ 3.25 | build system (Ninja generator)             |
| clang-format | formatting, pinned in `.clang-format`      |
| Catch2 3.16.1 | unit tests (an installed Catch2 3.x is used when available) |
| vcpkg        | optional, reproducible dependencies        |

The pinned format style is LLVM-based, 4-space indent, 100 columns,
`BreakStringLiterals: false`, include order preserved.

## Dependencies

The build requires a C++20 compiler, CMake 3.25+, Ninja, zlib, bzip2, CLI11
and cereal. System packages work directly.

For vcpkg, the [manifest](../vcpkg.json) lists the dependencies and pins the
built-in registry baseline. Keep the manifest in the project root so vcpkg
discovers it automatically; no separate registry configuration is needed.

```sh
export VCPKG_ROOT=/path/to/vcpkg
cmake --preset release -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset release
```

The vcpkg checkout must contain baseline commit
`9e593bb18ea69cc5095e012465dcd675a822ed0d` from the `2026.07.29` release.
Update an older or shallow checkout with
`git -C "$VCPKG_ROOT" fetch --depth 1 origin tag 2026.07.29` if baseline lookup
fails. For a development build, configure the `dev` preset with the same
toolchain and `-DVCPKG_MANIFEST_FEATURES=tests` to install Catch2 as well.

## Development commands

```sh
cmake --preset dev                  # configure Debug with tests
cmake --build --preset dev          # build library, tools and tests
ctest --preset dev                  # run the test suite
./scripts/format.sh                 # format the tree
./scripts/format.sh --check         # verify formatting without writing
```

## Presets

| Preset    | Build dir              | Purpose                                  |
| --------- | ---------------------- | ---------------------------------------- |
| `dev`     | `build/dev`            | Debug, tests ON                          |
| `release` | `build/release`        | Optimized user build, LTO when supported |

All build presets limit concurrent build jobs to two; pass `--parallel <count>` to override the
limit for a particular build.

CUDA stays optional: `cmake --preset dev -DHASHCLASH_ENABLE_CUDA=ON`.

### Editor setup (clangd)

`.clangd` selects the compilation database in `build/dev`. Configure the `dev`
preset before running clangd, and reconfigure it when dependencies or build
options change:

```sh
cmake --preset dev
clangd --check=src/md5birthdaysearch/birthday.cpp --tweaks=
```

The empty `--tweaks` value keeps check mode focused on parsing and diagnostics. It avoids
Apple clangd's refactoring-action probes, which can report false failures inside macro-heavy code.

### Quality gates

Sanitizer and warnings-as-errors builds are available as presets; the Catch2
test suite runs under each of them:

```sh
cmake --preset dev-asan    && cmake --build --preset dev-asan    && ctest --preset dev-asan
cmake --preset dev-tsan    && cmake --build --preset dev-tsan    && ctest --preset dev-tsan
cmake --preset dev-ubsan   && cmake --build --preset dev-ubsan   && ctest --preset dev-ubsan
cmake --preset dev-werror  && cmake --build --preset dev-werror  && ctest --preset dev-werror
```

The local quality gates are `./scripts/format.sh --check` and a
build with `HASHCLASH_WARNINGS_AS_ERRORS=ON` (the `dev-werror` preset).

## Tests

Tests live in `tests/` and are registered with CTest through
`catch_discover_tests`, so each `TEST_CASE` can be selected with
`ctest --preset dev -R <name>`.

- `tests/test_saveload.cpp` covers the cereal archives and the gzip/bzip2
  wrappers.
- `tests/test_cli11_options.cpp` covers the Boost.Program_options
  compatibility layer in `include/hashclash/cli11_options.hpp`, including
  command-line precedence over configuration files and compact short options.
- When Bash is available, CTest also runs `tests/test_cpc.sh`, which exercises
  the CPC collision-search process lifecycle with stub tools instead of running
  an expensive collision search.
- `tests/test_md5_collision_failure.cmake` exercises the real MD5 collision
  helper: worker exceptions and output failures must exit nonzero, while a
  zero-difference fixture verifies that successful searches save both blocks.

Only the `dev*` presets set `HASHCLASH_BUILD_TESTS=ON`, so they are the only
ones with test presets. The `release` preset builds optimized tools and
registers no tests, so `ctest --preset release` is
rejected instead of reporting an empty run as success.

Add new tests next to the corresponding area and prefer round-trip/property
style checks for the serialization code.

## Tool smoke tests

The unit tests exercise the library; the CLI tools need input fixtures. The
following recipes are enough to run every tool under ASan/TSan/UBSan.

MD5: generate the path fixtures in this order (forward produces `paths4`,
backward consumes it and produces `paths3`, connect joins them):

```sh
B=build/dev/bin
$B/md5_diffpathhelper --diffm2 9 --pathfromtext \
    --inputfile1 scripts/md5_text_path.txt --outputfile1 data/path_prefix.bin
$B/md5_diffpathforward --diffm2 9 -w data -a 16000 -e 4 --fillfraction 1 \
    -q 8 -f data/path_prefix.bin --normalt01 -t 2 --trange 2
$B/md5_diffpathbackward --diffm2 9 -w data -t 3 -a 20000 --trange 0
$B/md5_diffpathconnect -w data --inputfilelow data/paths3_0of1.bin.gz \
    --inputfilehigh data/paths4_0of1.bin.gz -t 3   # searches for minutes
```

The textcoll fixtures come from `scripts/textcoll.sh` (which generates
`textcoll_path.bin.gz` and `Q7Q24.bin.gz`); then e.g.

```sh
$B/md5_diffpathhelper --diffm5 11 --findcollision \
    --inputfile1 textcoll_path.bin.gz --threads 4 -w .
```

SHA-1 needs a text differential path; only the `me` field after the closing
`|` may be omitted by mistake, which silently truncates the path to one entry:

```sh
python3 - <<'EOF'
lines = [f"Q{t}: |00000000 00000000 00000000 00000000| [!!]" for t in range(80)]
open("fixed_path.txt", "w").write("\n".join(lines) + "\n")
EOF
B=build/dev/bin
$B/sha1_diffpathhelper --pathfromtext --inputfile1 fixed_path.txt \
    --outputfile1 fixed.bin.gz
$B/sha1_diffpathforward  -w . -f fixed.bin.gz -t 0  --trange 3 -m 1 -i 0
$B/sha1_diffpathbackward -w . -f fixed.bin.gz -t 79 --trange 3 -m 1 -i 0
$B/sha1_diffpathconnect  -w . --inputfilelow fixed.bin.gz \
    --inputfilehigh fixed.bin.gz -t 4 --threads 4
```

The `sha1_nearcollisionattack` tool is self-contained (384 target dIHVs are
compiled in), so it can be smoke-tested directly:

```sh
$B/sha1_nearcollisionattack --seed smoke        # search; stop with Ctrl-C
$B/sha1_nearcollisionattack --maxruntime 1 --statsinterval 1  # exits 0 after ~1 min
$B/sha1_nearcollisionattack --sha1benchmark     # compression benchmark (~10 min)
```

`sha1_diffpathhelper --filterfeasible` stops after 100,000 candidate states by default and reports
progress every 10,000 states. Use `--maxstates <count>` to change the limit, or `--maxstates 0` for
an exhaustive search without a limit. Reaching a nonzero limit exits with an error without saving
a filtered path set.

## Installation

```sh
cmake --build --preset release
cmake --install build/release --prefix /path/to/install
```

This installs the library, public headers and compiled tools. The shell attack
scripts run from the source checkout. The `release` preset selects compiler
optimization; it does not require a publishing environment or create an archive.

An installed project can consume the core library with
`find_package(HashClash CONFIG REQUIRED)` and link `HashClash::hashclash`.
The CLI11 compatibility target is installed as an optional package component.
Request it with `find_package(HashClash CONFIG REQUIRED COMPONENTS CLI11)`,
then link `HashClash::cli11_options`.

## Modern C++ guidelines

- Prefer C++20 facilities over hand-rolled code: `std::popcount`/`<bit>`,
  `std::string_view`, concepts, `if constexpr`, `std::jthread`,
  `std::scoped_lock`, function-local statics for lazy initialization.
- Avoid `std::function` in hot paths; store function pointers or templates.
- Expensive lookup tables (SDR carries, boolean-function tables) must be built
  lazily on first use, never in global constructors, so short-lived CLI
  invocations stay fast.
- Keep crypto inner loops plain and explicit; template metaprogramming is
  welcome for compile-time dispatch, but do not trade measurable performance
  for cleverness.

## Known technical debt

- The `po::options_description` chains in the tool `main.cpp` files format
  badly (staircase indentation); refactoring them to per-option registration
  statements is planned and should be test-covered.
- `md5_diffpathhelper --startpartialpathfromfile` cannot preserve a prefix ending in a partial
  message word after the first word of a block; those inputs now fail explicitly.
- The CUDA birthday-search backend is not build-tested (no CUDA toolchain in
  the local gates) and still carries its own timer helpers.
