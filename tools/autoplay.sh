#!/bin/bash
# Autoplay test matrix for Race into Space (see doc/fork/autoplay.md).
#
# Builds the game (optionally with AddressSanitizer + UBSan) and runs
# headless AI vs. AI games (raceintospace --autoplay) over a matrix of
# seeds, start years, hardware models and sandbox on/off, plus the
# test_extra scenario.  Prints a summary table; exit code 1 if any run
# failed.
#
#   tools/autoplay.sh                 # ASan build, 40 seeds x 9 combinations + long games, ~6 min
#   tools/autoplay.sh --no-asan       # plain Debug build in build/
#   tools/autoplay.sh --seeds 100 --sampled   # one combination per seed
#   tools/autoplay.sh --no-build --binary build/debug/raceintospace   # existing binary
#
# Options:
#   --asan | --no-asan   sanitizer build in build-asan (default) / plain build
#   --no-build           do not build, use the existing binary
#   --binary PATH        binary to run (default: build-asan/src/game/raceintospace
#                        or build/src/game/raceintospace)
#   --seeds N            seeds FROM..N (default 40)
#   --from S             first seed (default 1)
#   --full               (default) every combination per seed: 3 start years x
#                        3 models = 9 runs per seed (sandbox alternates by seed)
#   --sampled            only one combination per seed (cycling through all 18)
#   --jobs J             parallel runs (default: number of CPUs)
#   --timeout S          seconds per run before it counts as HANG (default 300)
#   --results DIR        result directory (default autoplay-results/<timestamp>)
#   --smoke              short smoke run: seeds 1-2, classic, basic, no sanitizer
#                        (used by ctest, < 2 min)

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 2

ASAN=1
BUILD=1
BINARY=""
SEEDS=40
FROM=1
FULL=1
LONGYEARS=20
SMOKE=0
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)"
TIMEOUT=300
RESULTS=""

while [ $# -gt 0 ]; do
    case "$1" in
        --asan) ASAN=1 ;;
        --no-asan) ASAN=0 ;;
        --no-build) BUILD=0 ;;
        --binary) BINARY="$2"; shift ;;
        --seeds) SEEDS="$2"; shift ;;
        --from) FROM="$2"; shift ;;
        --full) FULL=1 ;;
        --sampled) FULL=0 ;;
        --jobs) JOBS="$2"; shift ;;
        --timeout) TIMEOUT="$2"; shift ;;
        --results) RESULTS="$2"; shift ;;
        --smoke) SMOKE=1; ASAN=0 ;;
        -h|--help) sed -n '2,36p' "$0"; exit 0 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done

if [ "$ASAN" = 1 ]; then
    BUILD_DIR=build-asan
else
    BUILD_DIR=build
fi

if [ "$BUILD" = 1 ]; then
    if [ "$ASAN" = 1 ]; then
        # -fno-sanitize=alignment: data.h holds std::string/std::vector inside
        # '#pragma pack(1)' structs (known, harmless on x86/arm64, very noisy).
        SAN="-fsanitize=address,undefined -fno-sanitize=alignment -fno-omit-frame-pointer"
        cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="$SAN" -DCMAKE_C_FLAGS="$SAN" \
            -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" || exit 2
    else
        cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Debug || exit 2
    fi
    cmake --build "$BUILD_DIR" -j"$JOBS" --target raceintospace || exit 2
fi

if [ -z "$BINARY" ]; then
    for c in "$BUILD_DIR/src/game/raceintospace" \
             "$BUILD_DIR/debug/raceintospace.app/Contents/MacOS/raceintospace" \
             "$BUILD_DIR/src/game/raceintospace.app/Contents/MacOS/raceintospace"; do
        [ -x "$c" ] && BINARY="$c" && break
    done
fi
if [ -z "$BINARY" ] || [ ! -x "$BINARY" ]; then
    echo "binary not found (build it or pass --binary)" >&2
    exit 2
fi
BINARY="$(cd "$(dirname "$BINARY")" && pwd)/$(basename "$BINARY")"

[ -z "$RESULTS" ] && RESULTS="autoplay-results/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RESULTS"
RESULTS="$(cd "$RESULTS" && pwd)"

# ---- build the run list: "name|args" per line -------------------------
YEARS=(55 56 57)
MODELS=(basic historical random)
LIST="$RESULTS/runs.txt"
: > "$LIST"

add_run() { # seed year model sandbox scenario [years]
    local yrs="${6:-0}"
    local name="s$1-y$2-$3-sb$4-$5"
    local args="--seed $1 --start-year $2 --model $3 --scenario $5"
    [ "$4" = 1 ] && args="$args --sandbox"
    if [ "$yrs" != 0 ]; then
        name="$name-L$yrs"
        args="$args --years $yrs"
    fi
    echo "$name|$args" >> "$LIST"
}

# $1 = seed: pick one of the 18 combinations (cycling with the seed)
sample_run() { # seed scenario [years]
    local s="$1"
    add_run "$s" "${YEARS[$((s % 3))]}" "${MODELS[$(((s / 3) % 3))]}" \
        "$(((s / 9) % 2))" "$2" "${3:-0}"
}

if [ "$SMOKE" = 1 ]; then
    for s in 1 2; do add_run "$s" 57 basic 0 classic; done
else
    for s in $(seq "$FROM" "$SEEDS"); do
        if [ "$FULL" = 1 ]; then
            for y in "${YEARS[@]}"; do
                for m in "${MODELS[@]}"; do
                    # Sandbox only changes the game for human players, so with
                    # two AI players it is just alternated with the seed.
                    add_run "$s" "$y" "$m" "$((s % 2))" classic
                done
            done
        else
            sample_run "$s" classic
        fi
        # a long game: runs on after 1963 (up to 20 years), usually until
        # the first lunar landing (covers the landing end screens and the
        # later missions/events the 1963 end never reaches)
        sample_run "$((s + 1))" classic "$LONGYEARS"
    done
    # the extended-hardware scenario
    for s in 1 2 3 4; do
        sample_run "$s" test_extra
        sample_run "$s" test_extra "$LONGYEARS"
    done
fi

TOTAL=$(wc -l < "$LIST")
echo "autoplay: $TOTAL runs, $JOBS parallel, timeout ${TIMEOUT}s, binary $BINARY"
echo "autoplay: results in $RESULTS"

# ---- run one --------------------------------------------------------
run_one() {
    local name="${1%%|*}" args="${1#*|}"
    local dir="$RESULTS/$name"
    mkdir -p "$dir/save"
    export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
    export BARIS_DATA="$ROOT/data" BARIS_SAVE="$dir/save"
    export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0:symbolize=1"
    export UBSAN_OPTIONS="print_stacktrace=1"
    local start=$SECONDS
    # shellcheck disable=SC2086
    timeout "$TIMEOUT" "$BINARY" --autoplay $args > "$dir/log.txt" 2>&1
    echo $? > "$dir/exit"
    echo $((SECONDS - start)) > "$dir/secs"
    rm -rf "$dir/save"
}
export -f run_one
export RESULTS ROOT BINARY TIMEOUT

START=$SECONDS
tr '\n' '\0' < "$LIST" | xargs -0 -P "$JOBS" -I{} bash -c 'run_one "$1"' _ {}
ELAPSED=$((SECONDS - START))

# ---- summary --------------------------------------------------------
SUMMARY="$RESULTS/summary.txt"
FAILS=0
{
    printf '%-34s %-7s %4s %5s %-14s %s\n' RUN RESULT EXIT SECS "LAST TURN" "FIRST ERROR"
    while IFS='|' read -r name args; do
        dir="$RESULTS/$name"
        code=$(cat "$dir/exit" 2>/dev/null || echo "?")
        secs=$(cat "$dir/secs" 2>/dev/null || echo "?")
        log="$dir/log.txt"
        lastturn=$(grep -a '^\[autoplay\] turn' "$log" | tail -1 | sed -E 's/^\[autoplay\] turn ([0-9]+): ([0-9]+ [A-Za-z]+).*/\1 \2/')
        err=$(grep -a -m1 -E 'ERROR: AddressSanitizer|runtime error:|Assertion|assertion|terminate called|unhandled exception|SIGSEGV|Segmentation|FATAL|CRITICAL' "$log" | cut -c1-160)
        loc=$(grep -a -m1 -E '^SUMMARY: ' "$log" | sed -E 's|/[^ ]*/src/game/|src/game/|g' | cut -c1-160)
        if [ "$code" = 124 ]; then
            res=HANG
        elif [ "$code" != 0 ]; then
            res=CRASH
        elif grep -aq 'runtime error:' "$log"; then
            res=UBSAN
        elif ! grep -aq 'RESULT OK' "$log"; then
            res=NOEND
        else
            res=OK
        fi
        [ "$res" != OK ] && FAILS=$((FAILS + 1))
        [ -n "$loc" ] && err="$loc"
        err=$(echo "$err" | sed -E 's|/[^ ]*/src/game/|src/game/|g')
        printf '%-34s %-7s %4s %5s %-14s %s\n' "$name" "$res" "$code" "$secs" "$lastturn" "$err"
    done < "$LIST"
    echo
    echo "runs: $TOTAL  failed: $FAILS  elapsed: ${ELAPSED}s"
} | tee "$SUMMARY"

echo "logs: $RESULTS/<run>/log.txt"
[ "$FAILS" = 0 ] || exit 1
exit 0
