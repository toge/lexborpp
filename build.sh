#! /bin/sh

set -e

BUILD_DIR=build
if [ $# -ne 0 ] && [ "$1" = "static" ]; then
    BUILD_DIR=build_static
fi

export VCPKG_ROOT=$(readlink -f ~/vm/vcpkg)
if [ ! -d "$VCPKG_ROOT" ]; then
    echo "error: vcpkg が見つかりません: ~/vm/vcpkg (解決結果: '$VCPKG_ROOT')" >&2
    exit 1
fi
export VCPKG_OVERLAY_PORTS="${VCPKG_ROOT}/ports"
export VCPKG_TARGET_TRIPLET=x64-linux-static

# 最適化フラグは CMake 側の LEXBORPP_NATIVE_ARCH オプションに一本化する。
# (従来の -Ofast export は fast-math を持ち込むため廃止)
cmake -B ${BUILD_DIR} \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -DLEXBORPP_NATIVE_ARCH=ON \
    -DCMAKE_BUILD_TYPE=Release -S .

cmake --build ${BUILD_DIR} --verbose --parallel 4
