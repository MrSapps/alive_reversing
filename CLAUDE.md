# R.E.L.I.V.E. — build & run notes

Open-source engine replacement for Oddworld: Abe's Oddysee (AO) and Abe's Exoddus (AE).
C++17, CMake, SDL3; the editor uses Qt 5.

## Build (Linux)

Use the existing `build/` directory. It **must be a Debug build**: Release builds
(`-O3`) are far too slow to compile for iteration. `build-release/` holds a separate
Release config; only use it when asked.

```sh
# Configure (only needed once, or after CMakeLists changes)
# SDL3 is a local build, not a system package, so a fresh build dir needs SDL3_DIR.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
      -DSDL3_DIR=/home/snake/dev/SDL3-3.4.14/build/install/lib/cmake/SDL3/

# Build everything
cmake --build build -j5

# Build a single target
cmake --build build -j5 --target relive           # engine
cmake --build build -j5 --target relive-editor    # Qt editor
cmake --build build -j5 --target relive_lib_tests # unit tests
```

- Build with `-j5`. Higher parallelism runs out of memory.
- On Linux, Debug builds have ASan and UBSan on (see top-level `CMakeLists.txt`). An ASan
  leak report when the game quits is expected and does not mean the change is broken.
  UBSan's `vptr`, `alignment`, `null` and `pointer-overflow` checks are off because they
  made Debug builds several times slower. Configure with `-DRELIVE_FULL_UBSAN=ON` to turn
  them back on.
- Editor translations: the build only compiles the `.ts` files, it never changes them.
  After adding or removing `tr()` strings, run
  `cmake --build build --target update_translations` and commit the updated `.ts` files.
- Flatpak builds: `flatpak-builder` ignores the job-count env var. Pass `--jobs=5`
  and run it under a `systemd-run` memory cap to avoid OOM.

## Build time

Keep template-heavy code out of widely included headers. The worst offenders were the
nlohmann json (de)serializers and the per-TLV editor types: every file that included them
instantiated them again. Put that code in one `.cpp`, and give callers a small header that
only declares what they call (e.g. `data_conversion/AEQuicksaveJson.hpp`). Don't reach for
pimpl.

UBSan was the biggest single cost: on `Abe.cpp`, full UBSan + ASan took 7.7 s to compile
against 2.0 s for ASan alone, and made `relive` ~475 MB (mostly check data and its
relocations). Measure before re-enabling expensive sanitizer checks.

The build is throughput-bound at `-j5`: wall time is roughly total compile time / 5, so only
cutting total CPU helps a clean build. Splitting a big file only helps incremental builds of
that file. PCH findings (measured per file, with and without): `relive_lib`'s PCH saves ~40%,
the editor's and the editor tests' are small net wins, and a target with one or two files
shouldn't have one.

To find hot spots: build in a separate dir with
`CC=clang-14 CXX=clang++-14 ... -DCMAKE_CXX_FLAGS=-ftime-trace`. Clang writes a `<obj>.json`
trace next to each object file. Sum the `Source` (header) and `InstantiateClass`/
`InstantiateFunction` events across all files.

## Windows / MSVC CRT rule

The engine uses the static CRT. The editor uses the dynamic CRT, because Qt does.
MSVC can't mix the two in one binary:
- Every target defaults to the static CRT (`CMAKE_MSVC_RUNTIME_LIBRARY`, set in the
  top-level `CMakeLists.txt`).
- `Source/Tools/editor` switches its whole directory to `RELIVE_DYNAMIC_CRT`. Its targets
  may only link `*_dynamic_crt` libs (or DLL/INTERFACE ones such as SDL3 and Qt), never
  `relive_lib`.
- Code that both sides need is compiled twice. In `3rdParty/CMakeLists.txt` each lib's
  sources go in a variable, with an explicit `<lib>_dynamic_crt` copy. Engine code the
  editor needs goes in `editor_glue_dynamic_crt`.
- Never add `/MT`, `/MD` or `/MDd` compile options by hand.

These rules can't be checked on Linux. The Windows CI jobs are the only check.

## Test

**Every change must at least be verified by building and running all the tests.** Report
failures with their output. Don't call a change done until the tests pass.

```sh
# All tests, headless (EditorAutomationTests launches the real editor)
QT_QPA_PLATFORM=offscreen ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build --output-on-failure
# or run a test binary directly
build/Source/relive_lib/relive_lib_tests
build/Source/Tools/editor/GridPlacementTests
build/Source/Tools/editor/EditorAutomationTests
```

## Run

Binaries:
- Engine: `build/Source/relive/relive`
- Editor: `build/Source/Tools/editor/relive-editor`

The engine loads game data from the **current working directory**, so run it from a
directory that holds the original game files:

```sh
cd /path/to/AE/game/data && /home/snake/dev/alive_reversing/build/Source/relive/relive       # Abe's Exoddus (default)
cd /path/to/AO/game/data && /home/snake/dev/alive_reversing/build/Source/relive/relive -AO   # Abe's Oddysee
```

AE needs `st.lvl` and `mi.lvl` in that directory. AO needs `s1.lvl` and `r1.lvl`.
If they are missing, the game shows an error and logs a listing of the directory.

## Source layout

- `Source/relive` — engine executable (`Exe.cpp` holds `main`)
- `Source/relive_lib` — shared engine code and `relive_lib_tests`
- `Source/AliveLibAO`, `Source/AliveLibAE` — game logic for each game
- `Source/Tools/editor` — Qt level editor
- `options.cmake` — gameplay and behaviour feature toggles
