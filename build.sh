#! /bin/sh

set -e

BUILD_DIR=build
if [ $# -ne 0 ] && [ "$1" = "static" ]; then
    BUILD_DIR=build_static
fi

: "${VCPKG_ROOT:=$HOME/vm/vcpkg}"
: "${VCPKG_TARGET_TRIPLET:=x64-linux-static}"

VCPKG_ROOT_INPUT=$VCPKG_ROOT
if [ ! -d "$VCPKG_ROOT_INPUT" ]; then
    echo "error: VCPKG_ROOT is not a directory: $VCPKG_ROOT_INPUT" >&2
    exit 1
fi
case $VCPKG_ROOT_INPUT in
    /*) VCPKG_ROOT_PATH=$VCPKG_ROOT_INPUT ;;
    *) VCPKG_ROOT_PATH=./$VCPKG_ROOT_INPUT ;;
esac
if ! VCPKG_ROOT=$(CDPATH='' cd "$VCPKG_ROOT_PATH" && pwd -P); then
    echo "error: cannot resolve VCPKG_ROOT: $VCPKG_ROOT_INPUT" >&2
    exit 1
fi
VCPKG_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
if [ ! -f "$VCPKG_TOOLCHAIN_FILE" ]; then
    echo "error: VCPKG_ROOT is not a vcpkg checkout: $VCPKG_ROOT" >&2
    exit 1
fi
export VCPKG_ROOT VCPKG_TARGET_TRIPLET
if [ -n "${VCPKG_OVERLAY_PORTS:-}" ]; then
    export VCPKG_OVERLAY_PORTS
else
    unset VCPKG_OVERLAY_PORTS
fi

normalize_path_string()
(
    path=$1
    base=$2
    case $path in
        /*) ;;
        *) path=$base/$path ;;
    esac

    normalized=
    while [ -n "$path" ]; do
        path=${path#/}
        component=${path%%/*}
        if [ "$component" = "$path" ]; then
            path=
        else
            path=${path#*/}
        fi

        case $component in
            ""|.) ;;
            ..) normalized=${normalized%/*} ;;
            *) normalized=$normalized/$component ;;
        esac
    done

    if [ -z "$normalized" ]; then
        normalized=/
    fi
    printf '%s\n' "$normalized"
)

CMAKE_CACHE=$BUILD_DIR/CMakeCache.txt
if [ -f "$CMAKE_CACHE" ]; then
    CACHED_TOOLCHAIN_FILE=
    CACHED_TOOLCHAIN_FILE_FOUND=0
    while IFS= read -r cache_entry || [ -n "$cache_entry" ]; do
        case $cache_entry in
            CMAKE_TOOLCHAIN_FILE:*=*)
                CACHED_TOOLCHAIN_FILE=${cache_entry#*=}
                CACHED_TOOLCHAIN_FILE_FOUND=1
                ;;
        esac
    done < "$CMAKE_CACHE"

    if [ "$CACHED_TOOLCHAIN_FILE_FOUND" -eq 1 ]; then
        SOURCE_DIR=$(pwd -P)
        CACHED_TOOLCHAIN_FILE=$(normalize_path_string \
            "$CACHED_TOOLCHAIN_FILE" "$SOURCE_DIR/$BUILD_DIR")
        NORMALIZED_TOOLCHAIN_FILE=$(normalize_path_string \
            "$VCPKG_TOOLCHAIN_FILE" "$SOURCE_DIR")
        if [ "$CACHED_TOOLCHAIN_FILE" != "$NORMALIZED_TOOLCHAIN_FILE" ]; then
            echo "error: CMake cache toolchain mismatch in $CMAKE_CACHE" >&2
            echo "  cached: $CACHED_TOOLCHAIN_FILE" >&2
            echo "  requested: $NORMALIZED_TOOLCHAIN_FILE" >&2
            echo "error: use a clean/new build directory before changing VCPKG_ROOT" >&2
            exit 1
        fi
    fi
fi

# 最適化フラグは CMake 側の LEXBORPP_NATIVE_ARCH オプションに一本化する。
# (従来の -Ofast export は fast-math を持ち込むため廃止)
cmake -B "$BUILD_DIR" \
    "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_TOOLCHAIN_FILE" \
    "-DVCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET" \
    "-DVCPKG_OVERLAY_PORTS=${VCPKG_OVERLAY_PORTS:-}" \
    -DLEXBORPP_NATIVE_ARCH=ON \
    -DCMAKE_BUILD_TYPE=Release -S .

cmake --build "$BUILD_DIR" --verbose --parallel 4
