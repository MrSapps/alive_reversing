#!/usr/bin/env bash
# Checks the real game data sounds the same with the SF2/MIDI sound data as it did with the
# games' own VH/VB/sounds.dat/SEQ files (see Source/relive_lib/Sound/SOUND_FORMATS.md).
#
# Builds relive_sound_gold twice, in git worktrees: at the last commit before the switch and at
# --after (default HEAD). Each converts the game data with the engine's own conversion and plays
# every tone and SEQ through the sound code, then the two results are compared. The game dirs
# are only read: each run converts into its own scratch copy (the game files symlinked).
#
# Usage: compare_sound_formats.sh [--ae <AE game dir>] [--ao <AO game dir>] [options]
#   --work <dir>     Where the worktrees, builds and results go (default: <repo>/build-sound-compare)
#   --after <ref>    The commit to check (default HEAD)
#   --sdl3 <dir>     SDL3_DIR for cmake (default: the one in <repo>/build/CMakeCache.txt)
#   --jobs <n>       Build jobs (default 5)
#
# Exit code 0 if everything is the same. Otherwise the differing traces/WAVs are listed, and are
# in <work>/out_<game>_before and <work>/out_<game>_after.

set -euo pipefail
# So a failed build in $(build_tool ...) stops the script too
shopt -s inherit_errexit

REPO="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
AE_DIR=""
AO_DIR=""
WORK="$REPO/build-sound-compare"
AFTER_REF="HEAD"
SDL3=""
JOBS=5

while [ $# -gt 0 ]; do
    case "$1" in
        --ae) AE_DIR="$(realpath "$2")"; shift 2 ;;
        --ao) AO_DIR="$(realpath "$2")"; shift 2 ;;
        --work) WORK="$(realpath -m "$2")"; shift 2 ;;
        --after) AFTER_REF="$2"; shift 2 ;;
        --sdl3) SDL3="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option $1" >&2; exit 2 ;;
    esac
done

if [ -z "$AE_DIR" ] && [ -z "$AO_DIR" ]; then
    echo "Give --ae and/or --ao game dirs" >&2
    exit 2
fi

# Fail early on a dir that isn't a game dir
check_game_dir() {
    local dir="$1"
    shift
    for lvl in "$@"; do
        if [ -z "$(find "$dir" -maxdepth 1 -iname "$lvl" -print -quit)" ]; then
            echo "$dir has no $lvl, it isn't the game's data dir" >&2
            exit 2
        fi
    done
}
if [ -n "$AE_DIR" ]; then
    check_game_dir "$AE_DIR" st.lvl mi.lvl
fi
if [ -n "$AO_DIR" ]; then
    check_game_dir "$AO_DIR" s1.lvl r1.lvl
fi

AFTER="$(git -C "$REPO" rev-parse "$AFTER_REF")"
# The switch's parent: the last commit that still converts to VH/VB/SEQ
SWITCH="$(git -C "$REPO" log --format=%H -n 1 --grep='^Switch the sound data to SF2 and MIDI files' "$AFTER")"
if [ -z "$SWITCH" ]; then
    echo "$AFTER_REF doesn't have the SF2/MIDI switch" >&2
    exit 2
fi
BEFORE="$(git -C "$REPO" rev-parse "$SWITCH^")"

if [ -z "$SDL3" ] && [ -f "$REPO/build/CMakeCache.txt" ]; then
    SDL3="$(sed -n 's/^SDL3_DIR:[A-Z]*=//p' "$REPO/build/CMakeCache.txt")"
fi

mkdir -p "$WORK"

# Builds relive_sound_gold at a commit, prints the binary's path
build_tool() {
    local name="$1" ref="$2"
    local src="$WORK/src_$name" build="$WORK/build_$name"
    if [ ! -d "$src" ]; then
        git -C "$REPO" worktree add --detach "$src" "$ref" >&2
    else
        git -C "$src" checkout --force --detach "$ref" >&2
    fi
    git -C "$src" submodule update --init --recursive >&2
    if ! grep -q '"-convert"' "$src/Source/Tools/sound_gold/Main.cpp"; then
        if [ "$name" != before ]; then
            echo "relive_sound_gold at $ref has no -convert" >&2
            exit 1
        fi
        # -convert came after the switch, the commit before it gets it from this patch
        git -C "$src" apply "$REPO/Source/Tools/sound_gold/before_switch_convert.patch" >&2
    fi
    local sdl=()
    if [ -n "$SDL3" ]; then
        sdl=(-DSDL3_DIR="$SDL3")
    fi
    CMAKE_POLICY_VERSION_MINIMUM=3.10 cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Debug "${sdl[@]}" >&2
    cmake --build "$build" -j"$JOBS" --target relive_sound_gold >&2
    local tool="$build/Source/Tools/sound_gold/relive_sound_gold"
    if [ ! -x "$tool" ]; then
        echo "Building relive_sound_gold at $ref failed" >&2
        exit 1
    fi
    echo "$tool"
}

# A scratch game dir: the game's files symlinked, its own relive_data
game_copy() {
    local from="$1" to="$2"
    rm -rf "$to"
    mkdir -p "$to"
    for f in "$from"/*; do
        case "$(basename "$f")" in
            relive_data|relive.ini) ;;
            *) ln -s "$f" "$to/" ;;
        esac
    done
}

echo "Before: $BEFORE"
echo "After:  $AFTER"
BEFORE_TOOL="$(build_tool before "$BEFORE")"
AFTER_TOOL="$(build_tool after "$AFTER")"

# Leak reports from the Debug (ASan) builds aren't what this checks
export ASAN_OPTIONS=detect_leaks=0

status=0
run_game() {
    local game="$1" dir="$2" flag="$3"
    echo "=== $game"
    game_copy "$dir" "$WORK/game_${game}_before"
    game_copy "$dir" "$WORK/game_${game}_after"
    rm -rf "$WORK/out_${game}_before" "$WORK/out_${game}_after"
    # The tools log a lot, only their own output is shown
    if ! "$BEFORE_TOOL" -data="$WORK/game_${game}_before" $flag -convert -out="$WORK/out_${game}_before" > "$WORK/log_${game}_before.txt" 2>&1; then
        grep -v '^\[' "$WORK/log_${game}_before.txt" | tail -n 30
        echo "The before run failed, the whole log is $WORK/log_${game}_before.txt" >&2
        exit 1
    fi
    grep -v '^\[' "$WORK/log_${game}_before.txt"

    local rc=0
    "$AFTER_TOOL" -data="$WORK/game_${game}_after" $flag -convert -out="$WORK/out_${game}_after" -baseline="$WORK/out_${game}_before" > "$WORK/log_${game}_after.txt" 2>&1 || rc=$?
    grep -v '^\[' "$WORK/log_${game}_after.txt" | tail -n 200
    if [ $rc -ne 0 ]; then
        # 1 is "differs", anything else a crash
        if ! grep -q "files differ from" "$WORK/log_${game}_after.txt"; then
            echo "The after run failed, the whole log is $WORK/log_${game}_after.txt" >&2
            exit 1
        fi
        status=1
    fi
}

if [ -n "$AE_DIR" ]; then
    run_game ae "$AE_DIR" ""
fi
if [ -n "$AO_DIR" ]; then
    run_game ao "$AO_DIR" "-AO"
fi

if [ $status -eq 0 ]; then
    echo "OK: the sounds are the same before and after"
else
    echo "DIFFERENT: see the list above, and $WORK/out_*_before vs $WORK/out_*_after"
fi
exit $status
