# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Known-answer tests for the compression cores: RFC 1321 MD5 vectors and
  raw-block state updates in `tests/test_md5detail.cpp`, and FIPS 180 SHA-1
  vectors plus `sha1compress_me`/`sha1compress` agreement in
  `tests/test_sha1detail.cpp`.
- `include/hashclash/workload.hpp` providing `hashclash::run_workload` and
  `hashclash::guard_exception`, extracted from `md5_textcoll` and now
  covered by `tests/test_workload.cpp`.
- Build and test facilities: `HASHCLASH_WARNINGS_AS_ERRORS`,
  `HASHCLASH_ENABLE_ASAN/TSAN/UBSAN/COVERAGE` options, `dev-asan`, `dev-tsan`,
  `dev-ubsan` and `dev-werror` presets, and `install()` rules for the library,
  public headers and tools.
- Restored AVX2 detection for `md5_birthdaysearch`: `HASHCLASH_ENABLE_AVX2`
  (default ON on x86) compiles `simd_avx256.cpp` with `-mavx2` and defines
  `HASHCLASH_HAVE_AVX2` in the generated `config.h`; the tool checks
  `__builtin_cpu_supports("avx2")` at runtime and falls back to the scalar
  trail generator otherwise. The flag was lost in the CMake migration, which
  silently reduced the birthday search to its stub implementation.
- `docs/DEVELOPMENT.md` describing the presets, development commands, tests, modern
  C++ guidelines and installation.
- Optional link-time optimization via `HASHCLASH_ENABLE_LTO`; the `release`
  preset enables it when the toolchain supports it.
- Developer CMake presets `dev` (Debug + tests) and `release` (optimized with
  LTO when supported).
- Catch2 test suite behind `HASHCLASH_BUILD_TESTS=ON`, wired into CTest via
  `cmake --preset dev` / `ctest --preset dev`. Catch2 3.x installations are
  accepted; a missing installation fetches v3.16.1.
- Compiler warnings enabled by default through `HASHCLASH_WARNINGS`.
- `scripts/format.sh` to format the tree or verify formatting in CI-less
  development (`scripts/format.sh --check`).

### Changed

- Rewrote the README as a concise English fork overview with build, basic
  collision-tool usage and license information.
- Replaced GCC-only population-count builtins in the text-collision tool with
  C++20 `std::popcount`, selected the Windows entropy source with `_WIN32`, and
  linked Advapi32 for its Windows crypto API calls.
- Consolidated the vcpkg registry baseline into its root manifest and generate
  the build configuration header directly in CMake; removed the separate registry and header
  templates.
- Renamed `LICENSE.txt` to `LICENSE` without changing its MIT text or upstream
  attribution, and added the complete GPL-3.0-or-later text. Replaced the 81
  repetitive source-file GPL notices with compact SPDX identifiers, retaining the upstream
  copyright notice centrally; other upstream and third-party notices are unchanged.
- Moved the unchanged text-collision path template to
  `scripts/md5_text_path.txt`, beside the script that uses it.
- Replaced `std::endl` with `'\n'` across the library and tools, keeping
  explicit `std::flush` on the status lines that scripts read while a tool
  is running (`md5_birthdaysearch`'s `Work:` line, `md5_diffpathconnect`'s
  `Best path: ... tottunnel` lines) and on interactive progress output.
- `docs/DEVELOPMENT.md` now documents the sanitizer tool-smoke fixtures
  (including the SHA-1 text-path recipe that unlocked those tools) and
  refreshes the technical-debt list.
- Diffpath serialization no longer uses `const_cast`: saving goes through
  dedicated const wrappers (`saveload_const*`), loading keeps the mutable
  wrappers, and the gzip/bz2 bridge helpers take their buffer by value so
  the C-API casts are gone too. The on-disk format is unchanged; new tests
  cover `sha1differentialpath` and vector round-trips through the binary
  and text archives.
- The AVX2 birthday sampler's aligned scratch buffer is owned by a new
  portable `hashclash::aligned_buffer` RAII helper instead of manual
  `posix_memalign`/`_aligned_malloc` plus `free`; the placement-new detail
  object logic is unchanged. The helper is unit-tested on all platforms
  (alignment, reallocation, move semantics, zero-size, idempotent release).
- Structured bindings in the md5forward step-merge loops, and
  `std::numbers::ln2` instead of `log(2)`-derived constants in the birthday,
  md5textcoll prepare and sha1messagespace log2 diagnostics.
- `using namespace std;` is gone from all 44 remaining translation units;
  every std name is explicitly qualified. Together with the earlier header
  cleanup, the codebase no longer pulls namespace std into scope by
  default.
- C++20 standard-library cleanups: `contains()` instead of
  `find(...) != end()` in the CLI shim, tunnel analysis, sha1messagespace and
  booleanfunction; `starts_with()` in checkokpaths; `std::numbers::pi` in the
  birthday block distribution.
- OS entropy gathering (`getosrnd`) retries interrupted/short reads, never
  returns a partially written buffer, and falls back to `std::random_device`
  on failure (POSIX, Windows and the no-entropy branch). The global RNG seed
  and the per-step `random_permutation` reseeds no longer use
  second-resolution `time(NULL)`; the forward/backward worker chunk sizes
  use `std::clamp`.
- Silent `catch (...)` blocks no longer hide unexpected exceptions: worker
  threads and retry loops report unknown exceptions on stderr; the
  `bestpath*.bin.gz` reshuffles catch only `std::filesystem::filesystem_error`;
  expected failures while loading optional/partial files are narrowed to
  `std::exception` with the existing `failed`/`loaded`/magic flags handling
  control flow.
- The non-AVX2 birthday sampler stub omits its unused parameters,
  `simd_avx256_detail` is forward-declared as a `struct` to match its
  definition, and the AVX2-only private fields are `[[maybe_unused]]`; the
  x86 AVX2 path and the arm64 build are both warning-free under
  `dev-werror`.
- Compiler warnings in the connect, helper and birthday tools are cleared
  (reorder-ctor, dangling-else, sign-compare, unused variables/parameters/
  fields, deprecated copies), and their manual mutex lock windows use
  `std::lock_guard` with unchanged scope.
- Compiler warnings in the SHA-1 attack generator and `sha1messagespace` are
  cleared (unused tables/parameters, sign comparisons, deprecated copy
  assignment); `iterator_wrapper::operator==` now compares equality instead
  of less-than.
- Compiler warnings in the MD5 path search tools (`md5_diffpathforward`,
  `md5_diffpathbackward`, `md5_textcoll`) are cleared so the `dev-werror`
  preset builds them cleanly: initializer lists follow declaration order,
  braces disambiguate dangling-else, comparisons use explicit casts, dead
  variables were removed. Manual `mutex::lock()`/`unlock()` windows in the
  forward/backward workers and the autobalance container now use
  `std::lock_guard` with the same lock scope, so an exception can no longer
  leave the mutex locked.
- `dostep_thread` in `md5_diffpathconnect` and `sha1_diffpathconnect` owns its
  worker through `std::unique_ptr` instead of manual `new`/`delete`; the
  SHA-1 worker type is movable again now that the user destructor is gone.
- The MD5/SHA-1 attack module headers no longer leak `using namespace std;`;
  all std names are explicitly qualified. Module-local helpers
  (`birthday_types.hpp`, `distribution.hpp`, `storage.hpp`) received the same
  treatment, and the affected translation units own any using-directive
  instead of inheriting it.
- The md5textcoll `Q7810m1213` dedup table and the sha1messagespace
  `tmpxortried` dedup set use `std::unordered_map`/`std::unordered_set` with
  FNV-1a hashes instead of ordered maps/sets; neither relied on iteration
  order.
- `archive_type` is now a scoped `enum class`; `using enum archive_type` keeps
  the existing unqualified enumerator names at call sites.
- `booleanfunction` lookup tables are fixed-size `std::array`s instead of
  `std::vector`s, removing three heap allocations per instance and improving
  locality; the outcome-table path is covered by a new test case.
- MD5/SHA-1 constant tables are now `inline constexpr` arrays (single
  definition, usable in constant expressions); the `_DEBUG`/`*_INLINE_IMPL`
  macro plumbing, the duplicate definitions in `md5detail.cpp`/`sha1detail.cpp`
  and the uncalled `test_compress()` helper that called `exit(0)` from a public
  header were removed. Covered by `tests/test_constants.cpp`.
- All 42 C-style `typedef`s in tracked sources were converted to `using`
  aliases (including the CUDA headers).
- `cpu_timestamp()` has a portable `std::chrono::steady_clock` fallback on
  non-x86 targets (the `rdtsc` asm previously broke CPUPERFORMANCE builds on
  Apple Silicon), `storage.cpp` uses `std::scoped_lock` instead of a macro,
  and `program_generator` reports a missing `sha1_collfind.sh` instead of
  silently running `system()`.
- Owned resources use smart pointers: `dostep_progress` in md5forward,
  md5backward, md5connect and sha1connect, the md5_birthdaysearch output
  streams, and `birthday_thread` objects (now
  `std::vector<std::unique_ptr<...>>`). The redundant `volatile` on
  `dostep_index` was dropped (it is always accessed under its mutex).
- `triple`, `sdr`, `byteconditions` and `wordconditions` use defaulted
  `operator<=>`/`operator==` instead of hand-written comparison operators, and
  `rotate_left`/`rotate_right` use `std::rotl`/`std::rotr` which are
  well-defined for a zero rotation.
- `hashclash::timer` now uses `std::chrono::steady_clock` with no pimpl or heap
  allocation, its API is marked `[[nodiscard]]` where applicable, and a
  never-started timer returns 0.0 instead of reading uninitialized state.
- `.clangd` no longer enforces strict unused-include removal (it caused broken
  transitive includes).
- Worker thread pools use `std::jthread` (exception-safe auto-join) and the
  SHA-1 connect worker loop uses `std::scoped_lock` instead of manual
  `mutex::lock`/`unlock` pairs.
- `load_compressed` reads archive files directly instead of copying through an
  intermediate stringstream; the compression callbacks are constrained with
  concepts.
- CLI11 compatibility shim internals use `std::string_view` parsing helpers, a
  `stream_insertable` concept and a shared option-definition lookup instead of
  duplicated loops.
- `booleanfunction` and the SDR carry tables are initialized lazily instead of
  in global constructors, `hw()` is implemented with `std::popcount` and the
  `std::function`-based boolean function indirection was replaced by a plain
  function pointer. Tool startup dropped from ~0.9-1.3s to ~0.06-0.3s.
- Standardized the formatting of all tracked C/C++ sources with `clang-format`
  using the pinned `.clang-format` (LLVM base, 4-space indent, 100 columns,
  string literals kept intact, includes preserved), including the newly added
  test sources.
- The root vcpkg manifest now uses the full commit
  `9e593bb18ea69cc5095e012465dcd675a822ed0d` from the `2026.07.29` release as
  its built-in baseline. The bzip2, cereal, CLI11, zlib and Catch2 port versions
  are unchanged from the previous baseline.
- Sanitizer and warnings-as-errors options are assembled once and applied by a
  shared target helper. Preset build directories derive from the hidden base
  preset, while the two-job build default remains in place.
- `.clangd` explicitly selects the `dev` preset's compilation database.

### Removed

- The release-archive packaging script. The `release` CMake preset remains the
  optimized user build and does not require a release or publishing environment.
- Unused `.clang-tidy` configuration, upstream GitHub sponsorship metadata and
  README promotional content. Development documentation now omits generic
  commit/TDD mandates and resolved items previously listed as technical debt.
- The optional `.git-blame-ignore-revs` configuration.
- Dead `m0`/`m1`/`m2` assignments in the SDR break-vector loops (stored
  values were never read; Clang static analyzer).
- Dead `#if 0`/`if (0)` blocks in `collfind.cpp`, `tunnel_analysis.cpp`,
  `sha1connect/connect.cpp`, `sha1helper/convert.cpp` and
  `md5connect/dostep.cpp`; literal-zero null pointers replaced with
  `nullptr` in the differential-path classes and connect tools; non-ASCII
  bytes in `md5fastcoll` comments transliterated to ASCII.
- The unused, thread-unsafe `sample()`, `sampleword(int)`, `samplebyte(int)`
  and `range_rnd()` overloads in the textcoll alphabet helpers; worker code
  uses the explicit `localxrng` overloads.
- The never-instantiated `bestof.hpp` template, its includes in the SHA-1 attack
  generator and the include it emitted into generated collision-finding
  programs.
- Compile-time-disabled code: the `#if 0` experiments in the textcoll, md5helper,
  md5fastcoll and SHA-1 attack-generator sources, the unused legacy Boost
  serialization strings in `collfind.cpp`, and the uncalled
  `collisionfinding_thread::verifyconds()` (its declaration was removed from
  `md5helper/main.hpp` too).
- Legacy GitHub Actions workflow (`.github/workflows/c-cpp.yml`).

### Fixed

- The test target resolves the SHA-1 attack-generator headers relative to the
  HashClash project, so enabling tests from a parent project via
  `add_subdirectory()` no longer looks for headers below the parent source tree.
- `md5_fastcoll --help` now exits successfully after printing its options; invoking the tool with
  no arguments still prints the same guidance and exits nonzero. The clangd check recipe also
  disables check-mode refactoring probes that produce false failures in macro-heavy code with
  Apple clangd.
- Helper scripts now prefer `release`, `dev`, the legacy `cmake-ninja` layout,
  packaged binaries and finally other preset builds. Explicit `HASHCLASH_BIN`
  overrides remain authoritative; the script regression suite covers every
  lookup tier.
- Boolean switches in the CLI11 compatibility layer now apply their default
  value on the first parse, without resetting explicit command-line values
  when configuration or help is processed later. An absent switch previously
  left its target unchanged, including uninitialized fields such as the SHA-1
  connector's `showinputpaths` (reproduced with UBSan). Regression tests cover
  absent switches, explicit defaults, configuration and help.
- The CLI11 compatibility layer preserves `--` for excess positional
  arguments. Once all positional slots were consumed, arguments after the
  separator could be interpreted as options again; they are now rejected as
  excess positional arguments, with regression coverage for all three cases.
- `md5_diffpathhelper --findcollision` propagates unexpected worker failures
  to the main thread and exits nonzero instead of reporting success after all
  workers failed. The first exception is retained until every worker joins,
  and errors request that the remaining workers stop. Path-selection locking
  also releases the mutex if copying a path throws. A CLI regression test
  exercises a contradictory differential path with one and two workers. Output
  open/write/close failures now throw too, instead of printing "Found collision!"
  with no saved blocks; the test checks successful block output and a missing
  output directory using a fast zero-difference fixture.
- `md5_birthdaysearch` no longer races on the global random generator while
  distributing trails: `load_save_trails()` drew its two filename tokens with
  the lock released, whereas every worker reaches `xrng128()` under
  `global_mutex`. The two run concurrently as soon as the search is sharded
  with `--mod`/`--index` or `--generatormode`, so the shared `seed32_*`/`seedd`
  state was read and written without synchronization. Both draws now happen
  inside a `global_mutex` scope; the filenames and their format are unchanged.
- `sha1_attackgenerator` no longer emits `exit()` guards into the collision
  finder it generates: the emitted `verify_block` consistency checks throw a
  tagged `near_collision_error`, which the emitted `main` turns into a nonzero
  exit status instead of the misleading success status of `exit(0)`. This keeps
  generated output in sync with the checked-in `collfind.cpp` fixed in 846aa5a;
  `tests/test_program_generator.cpp` runs the emission path and fails if an
  `exit()` call is emitted again.
- CLI configuration parsing preserves explicit command-line values and applies
  defaults only once; compact short options such as `-t12` are recorded in the
  variables map just like their separated forms. Added regression coverage for
  mixed command-line/configuration input and compact options.
- The CPC script uses the full `--findcollision` option and detects failed
  collision-search processes instead of waiting indefinitely for output files.
  A Bash regression test exercises the search lifecycle using stub tools.
- CUDA-enabled birthday-search initialization keeps the CPU fallback in its own
  branch, fixing the out-of-scope `avx2_available` reference and preventing CPU
  initialization after a successful CUDA initialization. Its device-count
  comparison also passes warnings-as-errors checks.

- `sha1differentialpath::offset` is now initialized to 0, matching the MD5
  class. A default-constructed SHA-1 path read an indeterminate value in
  `tbegin()`/`tend()`/comparisons and serialized garbage when no indexed
  access had happened first (found by auditing the class for the same
  issue fixed earlier in `differentialpath`); regression test added.
- `sha1_nearcollisionattack` (the checked-in `collfind.cpp` collision
  finder) no longer calls `exit()`: internal consistency failures throw a
  tagged `near_collision_error` and terminate with status 1 (most sites
  previously reported success), while normal terminations (maximum
  runtime, step-15 give-up, found result) throw `near_collision_stop` and
  exit 0. Both are handled at the top level; the tool is self-contained
  and now smoke-tested under ASan/TSan/UBSan, including a `--maxruntime 1`
  run that exits cleanly.
- `operator>>` for `wordconditions` could not parse the output of
  `operator<<` under default stream settings: the byte separators were
  read with formatted extraction, which skips the spaces. Separators are
  now consumed unformatted, and the new `tests/test_conditions.cpp` locks
  in the `sdr`/`byteconditions`/`wordconditions` text formats, malformed
  input rejection and the SDR accessor semantics.
- `sha1differentialpath::operator[]` (const) silently read out of bounds in
  release builds because its bounds check was `#if DEBUG`-only; it now
  always throws `std::out_of_range`, matching `differentialpath`. ASan had
  caught `sha1_diffpathforward` reading before the path vector at `t=0`.
- `sha1_diffpathforward`/`sha1_diffpathbackward` read `Q_{t-1}...Q_{t-4}`
  from the original const path (out of range for paths that start at
  `Q0`) and cached `wordconditions&` across `newpath` vector reallocation,
  which ASan reported as a heap-use-after-free; they now use the extended
  working copy and force the full index range before taking references.
  `tests/test_sha1differentialpath.cpp` covers the accessor semantics.
- Tools no longer terminate the process from code paths that can unwind
  cleanly: `md5_textcoll` full-solution `exit(0)` calls inside worker
  threads now throw a `full_solution_found` exception that is caught by a
  guard wrapper at the thread boundary, setting an atomic stop flag that
  makes the remaining workers unwind and `main` return 0; the
  `md5_diffpathhelper --findcollision` success `exit(0)` throws
  `collision_found` and stops the other workers the same way.
- `sha1_attack_generator` terminal `exit(0)` calls in `checkokpaths`,
  `tunnel_analysis`, `program_generator` and the tunnel-analysis branches
  of `collisionfinding` became returns/exceptions; malformed input in
  `--checkokpaths` is now reported through `std::runtime_error` instead of
  silently exiting with status 0.
- The `main()` catch handlers of all tools no longer rethrow into
  `std::terminate` (which turned ordinary errors such as unrecognized CLI
  arguments into an abort); they print the existing diagnostics and return
  status 1.
- `differentialpath::offset` is now initialized to 0. It was indeterminate,
  so a default-constructed path read uninitialized memory in `tbegin()`/
  `tend()` before the first indexed access (Clang static analyzer).
- The SHA-1 forward/backward step functions threw a null-pointer
  dereference for step indices outside `[0, 80)`; they now throw
  `std::out_of_range`.
- `bf_outcome::operator()` computed `0 - (1 << 31)` in `int`, a signed
  overflow UBSan flagged in `md5_diffpathforward`; the bit values are now
  computed in `uint32` with unchanged results, and a new test case covers
  the `b=31` boundary.
- The connect tools printed from multiple threads at once, racing on
  libc++'s unsynchronized `ios_base` formatting state (TSan reported
  `ios_base::width`/fill and hundreds of derived reports). Worker/loader
  `std::cout`/`std::cerr`/`show_path` output and `progress_display` ticks now
  serialize through `hashclash::console_mutex`; `md5_diffpathconnect` runs
  TSan-clean on the paths3/paths4 fixtures.
- The SHA-1 attack generator embedded the serialized path-bit-relation matrix
  into the generated `collfind.cpp` without escaping, so JSON quotes and
  backslashes produced uncompilable source; the string literal is now
  escaped. The dead `sha1_collfind.sh` `system()` fallback was removed.
- `md5_diffpathbackward` read the shared `maxcond` without the mutex (workers
  held a bare reference) while `estimate()`/`push_back()` adjusted it; the
  reads/writes are now under the mutex and callers use `get_maxcond()`. The
  SHA-1 autobalance estimator no longer caches `uboundf` in a function-local
  `static` (the threshold is per call).
- The SDR count tables (`hashclash_scn`) were built with an unsynchronized
  check-then-init while workers read them; they are now an immutable
  function-local static (`scn_table()`), which TSan confirms is race-free.
- The remaining worker-thread RNG races found by the new TSan preset:
  `check_rotation`/`check_rotation_fast` now use a `thread_local` generator
  (they are called from path verification in every worker) and the textcoll
  second-block worker uses its own `localxrng`. `md5_diffpathforward` runs
  race-free under TSan after the fix.
- `simd_device_avx256` uses `_aligned_malloc` on MSVC (it previously called
  `posix_memalign`, which does not exist there), throws instead of `abort()` on
  allocation failure, and frees the placement-newed detail buffer in a new
  destructor (it used to leak).
- md5connect's best-path counters (`bestpathcond`, `bestmaxtunnel`,
  `bestmaxcomp`) are `std::atomic`; they were read outside the mutex while
  being written under it, and `volatile` provides no synchronization.
- Worker threads no longer share the global xorshift generator (a data race):
  `localxrng` moved into `hashclash/rng.hpp`, md5connect/md5helper workers own a
  per-thread generator, and the rare filename-randomization sites use the new
  `xrng128_locked()` helper. The misleading `volatile` on `dostep_index` was
  removed (it is always accessed under its mutex).
- Non-catching bare `throw;` statements were replaced with typed exceptions so
  internal-invariant failures report where they happened instead of calling
  `std::terminate()` with no diagnostic.
- Memory leak in `sha1_diffpathconnect`: worker functors were heap-allocated
  and copied into `std::thread` without being freed.
- Undefined behaviour from missing `return` statements in
  `showmespaceconditions` and `collisionfinding`.
- OS randomness now also works on macOS (`/dev/urandom`); the seed buffer is
  zero-initialized instead of reading indeterminate stack memory.
- Explicitly handled every `bitcondition` value in the tunnel condition
  switches (silences `-Wswitch`).
- CLI11 compatibility layer: short multi-token options no longer swallow
  following positional arguments (`md5_fastcoll -o a.bin b.bin` works again
  without a prefix file).
- CLI11 compatibility layer: config files expand multi-token/vector values
  (`diffm0 = 1 2 3`) like Boost.Program_options did.
- `bz2_decompress` no longer loops forever on payloads larger than the initial
  guess: the destination capacity is updated after each grow.
- `gzip_compress` sizes its buffer with `deflateBound`, so incompressible
  payloads no longer fail with `gzip: compression failed`.
- Uncompressed text/JSON archive loads no longer fail with `load(): read error!`
  after the parser consumed the stream up to EOF.
- Build break in `sha1_diffpathconnect`: `std::thread`/`std::mutex` are now
  included explicitly instead of relying on transitive includes.
- `md5_birthdaysearch`'s global `quit` flag is now `std::atomic<bool>`. Worker
  threads set it after finding a collision and the main loop polls it, so the
  previous unsynchronized read was a data race (undefined behavior).
- SHA-1 differential-path equality and ordering no longer ignore the message
  difference of steps that exist on only one side. The old comparison was
  intransitive, so `std::sort`/`std::unique` could merge distinct paths (for
  example under `sha1_diffpathhelper --split 1 --unique`) and `std::set`/`map`
  keys compared inconsistently.
- `differentialpath::operator==` (MD5) no longer throws `std::out_of_range`
  when comparing paths with non-overlapping step ranges: missing steps are
  compared as trivial conditions like in `operator<`, in both directions.
  Regression tests cover empty paths, non-overlapping ranges and both
  comparison directions.
- The SHA-1 collision-finder generator and the checked-in `collfind.cpp` no
  longer form a pointer one element before the value-vector start (and no
  longer for empty ranges); the descending loops now decrement only while the
  pointer is above the range begin.
- `scripts/fastcpc.sh` detects the CPU count with `getconf _NPROCESSORS_ONLN`
  instead of the non-portable `nproc` and clamps the thread count to at least
  one, so it also runs on macOS.
- `sha1_attack_generator` no longer throws an unconditional "compile script
  not found" error after writing `collfind.cpp`. `generate_program()` returns
  whether the file was written and `collisionfinding()` propagates that to the
  exit status instead of swallowing it and returning success; a path loop that
  ends without generating anything now also reports failure.
- `md5_birthdaysearch`'s birthday search declares its worker-thread data
  before the `std::jthread` vector, so the threads are joined before the
  `birthday_thread` objects they reference are destroyed; an exception
  between thread creation and the join loop no longer causes a
  use-after-free (verified with an ASan reproduction).
- `sha1_attackgenerator`'s `fill_tables()` cleared its 80-entry `dF` table
  inside the `t = -4 ... 80` loop that clears the 85-entry tables, writing
  five elements outside `dF` (ASan global-buffer-overflow in the emission
  path); `dF` now has its own in-range clearing loop. The table contents
  computed afterwards are unchanged.
- `scripts/generic_ipc.sh` no longer reports success after a failed collision
  search. The `--findcollision` step was piped into `tee`, which discarded the
  exit status of the helper, and the collision blocks were then concatenated
  with an unmatched glob (`cat ./data/coll1_*`) that `cat` received as a
  literal filename: a failed run produced truncated "collision" files and
  exited 0. The exit status is now propagated, missing collision blocks are an
  explicit error before anything is concatenated, and the script exits nonzero
  instead of continuing with bad data.
- `scripts/cpc.sh` terminates the `md5_*` worker processes by matching the
  command line of the binaries in `$BINDIR` with `pkill -f` instead of
  `killall -r`: the latter is a GNU extension that the BSD `killall` of macOS
  rejects with `unknown signal r`, so on macOS every worker kept running. If
  process enumeration is unavailable, it falls back to the shell job table.
- `scripts/fastcpc.sh` builds its log timestamps with
  `date +%Y-%m-%dT%H:%M:%S%z` instead of `date -Is`, which the BSD `date` of
  macOS rejects with `invalid argument 's' for -I`, leaving the timestamp of
  every log line empty.
- `scripts/hashclash_paths.sh` also looks for the built binaries in
  `build/<preset>/bin`, where every CMake preset builds them, next to the
  legacy `build/cmake-ninja/bin` and the `bin/` of a release tarball. After
  `cmake --preset dev` (or `release`) the other scripts could not find the
  tools and failed with "No such file or directory".
- `tests/test_cpc.sh` additionally covers the portable worker termination and
  the failure propagation of `scripts/generic_ipc.sh`.
- The CLI11 compatibility shim now honours `--`: a bare `--` ends option
  parsing and every later argument is treated as positional, matching
  Boost.Program_options. Invocations such as `md5_fastcoll -- -x` no longer
  fail with "The following arguments were not expected"; regression coverage
  lives in `tests/test_cli11_options.cpp`.
- The 25 remaining bare `throw;` statements outside a `catch` handler are now
  explicit exceptions carrying a message: the SHA-1 connect tool
  (`connect.cpp`, 17 sites in the bit-connection and bit-data passes), the MD5
  connect tool (`md5_connect_bits`), `sha1_diffpathbackward` (inconsistent MSB
  backward conditions), `sha1_diffpathhelper` (unknown bitcondition in the
  filter path), `sha1_attackgenerator` (`index_okpaths`, `loop_data::add`,
  `optimized_mecond_to_string`, `generate_program`) and the generated
  `collfind.cpp`'s `loop_data::add`. A `throw;` without an active handler
  calls `std::terminate`, so these paths aborted the tool instead of reporting
  the error; the `generate_program` message-difference guard is now covered by
  `tests/test_program_generator.cpp`.
- `hashclash::run_workload` stores its callable once and copies it into every
  worker thread instead of forwarding it into each `std::jthread`. An rvalue
  callable was moved-from after the first thread, so the remaining workers ran
  on an empty payload; move-only callables are now rejected with a
  `static_assert` explaining the per-thread copy contract, and
  `tests/test_workload.cpp` covers the rvalue, lvalue and per-thread mutation
  behaviour.
- `md5_birthdaysearch` reported `Loading 'singleblockdata.bin'` for the
  one-block path while it actually loads `singleblockdata.bin.bz2`, the name
  `save_bz2` writes and `load_bz2` reads; the log line now names the archive on
  disk.
- `ctest --preset release` can no longer masquerade as a successful test run.
  The `release` preset builds optimized tools and
  sets `HASHCLASH_BUILD_TESTS=OFF`, so the test preset ran nothing, printed
  `No tests were found!!!` and still exited 0; the empty test preset is gone
  and the command now fails with `CMake Error: No such test preset ...
  "release"` (exit 1).
- `md5_birthdaysearch` now probes AVX2 at runtime under MSVC too. The probe's
  `#elif defined(HASHCLASH_HAVE_AVX2)` arm assumed AVX2 unconditionally, so a
  Windows x86-64 build (`HASHCLASH_ENABLE_AVX2=ON`, the default on x86)
  executed AVX2 instructions on a CPU without AVX2 and died with SIGILL. The
  MSVC path checks CPUID leaf 7 EBX bit 5, OSXSAVE and the saved XMM/YMM state
  in XCR0, the same predicate as GCC/Clang's `__builtin_cpu_supports("avx2")`,
  and is now shared with the GCC/Clang path in the new
  `include/hashclash/cpu_avx2.hpp`, whose decision rule is covered by
  `tests/test_cpuperformance.cpp`. CPUs without AVX2 (and builds with
  `HASHCLASH_ENABLE_AVX2=OFF`, or non-x86 targets) keep using the scalar trail
  generator.

[Unreleased]: https://github.com/Oswald-H/hashclash/compare/master...dev
