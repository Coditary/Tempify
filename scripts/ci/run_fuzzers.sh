#!/usr/bin/env bash
set -euo pipefail

ROOT="${FUZZ_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
BUILD_DIR="${FUZZ_BUILD_DIR:-$ROOT/build-cmake/fuzz}"
CMAKE_PRESET="${FUZZ_PRESET:-fuzz}"
BUILD_PRESET="${FUZZ_BUILD_PRESET:-fuzz}"
MAX_TOTAL_TIME="${TEMPIFY_FUZZ_MAX_TOTAL_TIME:-60}"

FUZZ_TARGETS=(
    fuzz_slugify
    fuzz_answer_json
    fuzz_cli_parser
)

seed_dir_for_target() {
    case "$1" in
        fuzz_slugify)
            printf '%s/tests/fault_tolerance/fuzz/seeds/slug\n' "$ROOT"
            ;;
        fuzz_answer_json)
            printf '%s/tests/fault_tolerance/fuzz/seeds/answer_json\n' "$ROOT"
            ;;
        fuzz_cli_parser)
            printf '%s/tests/fault_tolerance/fuzz/seeds/cli_parser\n' "$ROOT"
            ;;
        *)
            printf 'Unknown fuzz target: %s\n' "$1" >&2
            return 1
            ;;
    esac
}

bootstrap_corpus() {
    local target=$1
    local corpus="$ROOT/tests/fault_tolerance/fuzz/corpus/$target"
    local seeds_dir
    seeds_dir=$(seed_dir_for_target "$target")

    printf 'Bootstrapping corpus for %s\n' "$target"
    mkdir -p "$corpus"

    shopt -s nullglob
    for seed_file in "$seeds_dir"/*; do
        [[ -f "$seed_file" ]] || continue
        cp -n "$seed_file" "$corpus/$(basename "$seed_file")"
    done
}

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake --preset "$CMAKE_PRESET" "$@"
fi

cmake --build --preset "$BUILD_PRESET" --parallel

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

for target in "${FUZZ_TARGETS[@]}"; do
    bootstrap_corpus "$target"
    corpus="$ROOT/tests/fault_tolerance/fuzz/corpus/$target"
    printf 'Running fuzzer %s for %ss\n' "$target" "$MAX_TOTAL_TIME"
    "$BUILD_DIR/$target" \
        -max_total_time="$MAX_TOTAL_TIME" \
        -close_fd_mask=3 \
        "$corpus"
done

chmod +x "$ROOT/scripts/ci/run_fuzz_regression.sh"
printf 'Running fuzz regression replay\n'
"$ROOT/scripts/ci/run_fuzz_regression.sh"
