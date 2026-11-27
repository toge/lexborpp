#! /bin/sh

# 使い方: ./test.sh [ビルドディレクトリ] (既定: build)
BUILD_DIR=${1:-build}

cd "$(dirname "$0")/$BUILD_DIR" || exit 1
ctest -V --output-on-failure
