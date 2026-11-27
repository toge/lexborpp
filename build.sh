#! /bin/sh

set -e

BUILD_DIR=build
if [ $# -ne 0 ] && [ "$1" = "static" ]; then
    BUILD_DIR=build_static
fi

: "${VCPKG_ROOT:=$HOME/vm/vcpkg}"
: "${VCPKG_TARGET_TRIPLET:=x64-linux-static}"
: "${VCPKG_OVERLAY_PORTS:=$VCPKG_ROOT/ports}"

VCPKG_ROOT_INPUT=$VCPKG_ROOT
if ! VCPKG_ROOT=$(readlink -f "$VCPKG_ROOT_INPUT"); then
    echo "error: cannot resolve VCPKG_ROOT: $VCPKG_ROOT_INPUT" >&2
    exit 1
fi
if [ ! -d "$VCPKG_ROOT" ] ||
   [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
    echo "error: VCPKG_ROOT is not a vcpkg checkout: $VCPKG_ROOT" >&2
    exit 1
fi
export VCPKG_ROOT VCPKG_TARGET_TRIPLET VCPKG_OVERLAY_PORTS

# 最適化フラグは CMake 側の LEXBORPP_NATIVE_ARCH オプションに一本化する。
# (従来の -Ofast export は fast-math を持ち込むため廃止)
cmake -B "$BUILD_DIR" \
    "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    "-DVCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET" \
    -DLEXBORPP_NATIVE_ARCH=ON \
    -DCMAKE_BUILD_TYPE=Release -S .

cmake --build "$BUILD_DIR" --verbose --parallel 4
