#!/usr/bin/env bash
set -euo pipefail

ROOT="${CLANG_FORMAT_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

resolve_clang_format() {
    if command -v clang-format >/dev/null 2>&1; then
        printf '%s\n' clang-format
        return 0
    fi

    local candidate
    for candidate in clang-format-22 clang-format-21 clang-format-20 clang-format-19 clang-format-18; do
        if command -v "$candidate" >/dev/null 2>&1; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    printf 'clang-format not found; install clang-format\n' >&2
    return 1
}

collect_source_files() {
    if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git -C "$ROOT" ls-files -z -- \
            'src/main/cpp/*.cpp' \
            'src/main/include/*.h' \
            'tests/support/*.cpp' \
            'tests/support/*.h' \
            'tests/tempify/*.cpp'
        return 0
    fi

    find "$ROOT/src/main/cpp" "$ROOT/src/main/include" "$ROOT/tests/support" "$ROOT/tests/tempify" \
        \( -name '*.cpp' -o -name '*.h' \) -print0 2>/dev/null || true
}

usage() {
    cat <<EOF
Usage: $(basename "$0") <format|check>

Commands:
  format  Apply clang-format to project sources
  check   Verify formatting without modifying files
EOF
}

main() {
    local mode=${1:-}
    if [[ -z "$mode" ]]; then
        usage >&2
        return 2
    fi

    local formatter
    formatter=$(resolve_clang_format)

    local -a files=()
    while IFS= read -r -d '' file; do
        files+=("$file")
    done < <(collect_source_files)

    if [[ ${#files[@]} -eq 0 ]]; then
        printf 'No source files found for clang-format\n' >&2
        return 1
    fi

    case "$mode" in
        format)
            printf 'Formatting %d files with %s\n' "${#files[@]}" "$formatter"
            "$formatter" -i "${files[@]}"
            ;;
        check)
            printf 'Checking format of %d files with %s\n' "${#files[@]}" "$formatter"
            "$formatter" --dry-run --Werror "${files[@]}"
            ;;
        *)
            usage >&2
            return 2
            ;;
    esac
}

main "$@"
