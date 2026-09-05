#pragma once

/**
 * @file lexborpp/config.hpp
 * @brief ビルドモード設定。
 *
 * lexborpp は既定で例外を許容するが、ランタイム時の例外送出は禁止する。
 * コンパイル時評価での不正入力は LEXBORPP_CONSTEVAL_FAIL で通知する。
 * (-fno-exceptions 時は std::abort() になる)
 * ランタイム API の失敗は std::expected<T, std::errc> で返す。
 */

#include <cstdlib>

/**
 * @brief consteval経路の失敗通知。
 *
 * 例外ありでは `throw msg`（不正入力はコンパイルエラーになり診断メッセージが残る）。
 * `-fno-exceptions`（`__cpp_exceptions` 未定義）では非constexprな [[noreturn]]
 * 関数の呼び出しになり、定数評価に触れるとコンパイルエラー、実行時に触れると
 * std::abort() する。
 */
namespace lexborpp::detail {
[[noreturn]] inline void consteval_fail(char const* msg) noexcept {
  (void)msg;
  std::abort();
}
} // namespace lexborpp::detail

#ifdef __cpp_exceptions
#include <stdexcept>
#define LEXBORPP_CONSTEVAL_FAIL(msg) throw(msg)
#else
#define LEXBORPP_CONSTEVAL_FAIL(msg) ::lexborpp::detail::consteval_fail(msg)
#endif

// 後方互換性のため残すが、新コードでは LEXBORPP_CONSTEVAL_FAIL を使用すること。
#ifndef LEXBORPP_WASI_MINIMAL
#include <stdexcept>
#define LEXBORPP_THROW(expr) throw expr
#else
#include <cstdlib>
namespace lexborpp::detail {
[[noreturn]] inline void fail() noexcept { std::abort(); }
} // namespace lexborpp::detail
#define LEXBORPP_THROW(expr) ::lexborpp::detail::fail()
#endif
