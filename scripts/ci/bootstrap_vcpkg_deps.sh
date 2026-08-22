#!/usr/bin/env bash
set -euo pipefail

ROOT="${TEMPIFY_ROOT:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
SANITIZER_KIND="${1:-asan}"
shift || true

OVERLAY_TRIPLETS="${VCPKG_OVERLAY_TRIPLETS:-$ROOT/cmake/vcpkg/triplets}"
VCPKG_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"

case "$SANITIZER_KIND" in
    msan)
        export CC=clang
        export CXX=clang++
        VCPKG_TRIPLET=x64-linux-msan
        VCPKG_ROOT="$ROOT/build-cmake/.vcpkg-msan"
        MSAN_LIBCXX_PREFIX="${TEMPIFY_MSAN_LIBCXX_PREFIX:-${XDG_CACHE_HOME:-$HOME/.cache}/tempify-msan-libcxx}"
        if [[ ! -f "$MSAN_LIBCXX_PREFIX/lib/libc++.so" ]]; then
            chmod +x "$ROOT/scripts/ci/bootstrap_msan_libcxx.sh"
            TEMPIFY_MSAN_LIBCXX_PREFIX="$MSAN_LIBCXX_PREFIX" "$ROOT/scripts/ci/bootstrap_msan_libcxx.sh"
        fi
        export CXXFLAGS="-stdlib=libc++ -nostdinc++ -isystem ${MSAN_LIBCXX_PREFIX}/include/c++/v1 -fsanitize=memory -fno-omit-frame-pointer -g -fsanitize-memory-track-origins=1"
        export CFLAGS="-fsanitize=memory -fno-omit-frame-pointer -g -fsanitize-memory-track-origins=1"
        export LDFLAGS="-stdlib=libc++ -L${MSAN_LIBCXX_PREFIX}/lib -lc++ -lc++abi -fsanitize=memory -Wl,-rpath,${MSAN_LIBCXX_PREFIX}/lib"
        ;;
    asan|tsan|fuzz|coverage)
        VCPKG_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux}"
        VCPKG_ROOT="${VCPKG_ROOT:-$ROOT/build-cmake/.vcpkg-ci}"
        ;;
    *)
        printf 'Unsupported sanitizer kind for vcpkg bootstrap: %s\n' "$SANITIZER_KIND" >&2
        exit 1
        ;;
esac

if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT" >&2
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics >&2
fi

"$VCPKG_ROOT/vcpkg" install \
    "lua:${VCPKG_TRIPLET}" \
    "cli11:${VCPKG_TRIPLET}" \
    --overlay-triplets="$OVERLAY_TRIPLETS" \
    --binarysource=clear \
    "$@" >&2

ENV_FILE="$ROOT/build-cmake/.vcpkg-${SANITIZER_KIND}.env"
mkdir -p "$(dirname "$ENV_FILE")"
{
    printf 'export VCPKG_ROOT=%q\n' "$VCPKG_ROOT"
    printf 'export VCPKG_TARGET_TRIPLET=%q\n' "$VCPKG_TRIPLET"
} >"$ENV_FILE"
