#!/bin/sh

set -eu

SOURCE_DIR=$1
TEST_DIR=$2
VCPKG_ROOT_INPUT=$3

case $TEST_DIR in
  /*) ;;
  *) TEST_DIR=$(CDPATH='' cd "$(dirname "$TEST_DIR")" && pwd -P)/$(basename "$TEST_DIR") ;;
esac

rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR/project"
cp "$SOURCE_DIR/build.sh" "$TEST_DIR/project/build.sh"
chmod +x "$TEST_DIR/project/build.sh"
cat > "$TEST_DIR/project/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.25)
project(lexborpp_build_script_test NONE)
EOF

mkdir "$TEST_DIR/project/build_static"
printf '%s\n' \
  'VCPKG_OVERLAY_PORTS:FILEPATH=/missing/lexborpp-ports' \
  'VCPKG_OVERLAY_TRIPLETS:FILEPATH=/missing/lexborpp-triplets' \
  > "$TEST_DIR/project/build_static/CMakeCache.txt"

(
  cd "$TEST_DIR/project"
  env -u VCPKG_OVERLAY_PORTS -u VCPKG_OVERLAY_TRIPLETS \
    VCPKG_ROOT="$VCPKG_ROOT_INPUT" ./build.sh static
)
cached_ports=$(
  sed -n 's/^VCPKG_OVERLAY_PORTS:[^=]*=//p' \
    "$TEST_DIR/project/build_static/CMakeCache.txt"
)
[ "$cached_ports" = "" ]
cached_triplets=$(
  sed -n 's/^VCPKG_OVERLAY_TRIPLETS:[^=]*=//p' \
    "$TEST_DIR/project/build_static/CMakeCache.txt"
)
[ "$cached_triplets" = "" ]

mkdir -p "$TEST_DIR/overlay-triplets-a" "$TEST_DIR/overlay-triplets-b"
mkdir -p "$TEST_DIR/overlay-ports-a" "$TEST_DIR/overlay-ports-b"
overlay_ports="$TEST_DIR/overlay-ports-a;$TEST_DIR/overlay-ports-b"
overlay_triplets="$TEST_DIR/overlay-triplets-a;$TEST_DIR/overlay-triplets-b"
(
  cd "$TEST_DIR/project"
  VCPKG_ROOT="$VCPKG_ROOT_INPUT" \
    VCPKG_OVERLAY_PORTS="$overlay_ports" \
    VCPKG_OVERLAY_TRIPLETS="$overlay_triplets" ./build.sh static
)
cached_ports=$(
  sed -n 's/^VCPKG_OVERLAY_PORTS:[^=]*=//p' \
    "$TEST_DIR/project/build_static/CMakeCache.txt"
)
[ "$cached_ports" = "$overlay_ports" ]
cached_triplets=$(
  sed -n 's/^VCPKG_OVERLAY_TRIPLETS:[^=]*=//p' \
    "$TEST_DIR/project/build_static/CMakeCache.txt"
)
[ "$cached_triplets" = "$overlay_triplets" ]

rm -rf "$TEST_DIR"
