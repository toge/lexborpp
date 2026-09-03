#pragma once

/**
 * @file lexborpp/config.hpp
 * @brief ビルドモード設定。
 *
 * LEXBORPP_WASI_MINIMAL が定義されると、ライブラリ内の全ての例外送出
 * (LEXBORPP_THROW) が std::abort() に置き換わり、-fno-exceptions でも
 * ビルドできる「例外なしモード」になる。コンパイル時評価での不正入力は
 * 従来どおりコンパイルエラーになる。wasm32-wasip1 / wasm32-emscripten は
 * WASI/hosted とみなすため自動では有効にならず、WASI 上で
 * 最小構成を検証する場合は手動で `-DLEXBORPP_WASI_MINIMAL` を指定する。
 * 本ライブラリの WASI 対応は wasi-sdk sysroot を用いた wasm32-wasip1 でのビルドを
 * 想定（wasmedge 等で実行可能）。`<iostream>` は wasip1/wasip2 では WASI 経由で
 * 利用可能なため無効化しない。
 *
 * 例: clang++ --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot
 *       -fno-exceptions -DLEXBORPP_WASI_MINIMAL=1 -I include -c src.cpp -o src.o
 */
#if !defined(LEXBORPP_WASI_MINIMAL) && defined(__wasm__) && !defined(__wasi__) && !defined(__EMSCRIPTEN__)
#  define LEXBORPP_WASI_MINIMAL 1
#endif

/**
 * @brief 例外送出の統一マクロ。
 *
 * hosted (既定) では `throw expr` に展開する。LEXBORPP_WASI_MINIMAL 定義時は
 * expr を評価せず `detail::fail()` を呼ぶ。fail() は非 constexpr のため
 * コンパイル時評価では従来どおりコンパイルエラーになり、実行時は std::abort() する。
 * これにより -fno-exceptions でもライブラリ全体がビルドできる。
 */
#ifndef LEXBORPP_WASI_MINIMAL
#  include <stdexcept>
#  define LEXBORPP_THROW(expr) throw expr
#else
#  include <cstdlib>
namespace lexborpp::detail {
[[noreturn]] inline void fail() noexcept { std::abort(); }
} // namespace lexborpp::detail
#  define LEXBORPP_THROW(expr) ::lexborpp::detail::fail()
#endif
