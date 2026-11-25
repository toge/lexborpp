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
#include <utility>

/**
 * @brief consteval経路の失敗通知。
 *
 * 例外ありでは `throw msg`（不正入力はコンパイルエラーになり診断メッセージが残る）。
 * `-fno-exceptions`（`__cpp_exceptions` 未定義）では非constexprな [[noreturn]]
 * 関数の呼び出しになり、定数評価に触れるとコンパイルエラー、実行時に触れると
 * std::abort() する。
 */
namespace lexborpp::detail {
[[noreturn]] inline void consteval_fail() noexcept {
  std::abort();
}
} // namespace lexborpp::detail

#ifdef __cpp_exceptions
#include <stdexcept>
#define LEXBORPP_CONSTEVAL_FAIL(msg) throw(msg)
#else
// msg は定数評価の診断用。-fno-exceptions では abort に置き換わるため破棄する。
#define LEXBORPP_CONSTEVAL_FAIL(msg) ::lexborpp::detail::consteval_fail()
#endif

namespace lexborpp::detail {

/**
 * @brief noexcept 境界から bad_alloc を漏らさないためのガードです。
 *
 * `f()` を実行し、`std::bad_alloc` のみ `on_oom()` の戻り値へ変換します。
 * -fno-exceptions では try/catch を書けないため直接実行します
 * (割り当て失敗は libstdc++ / libc++ が abort します)。
 */
#ifdef __cpp_exceptions
template <typename F, typename OnOOM>
constexpr auto with_bad_alloc_guard(F&& f, OnOOM&& on_oom) -> decltype(f()) {
  try {
    return std::forward<F>(f)();
  } catch (std::bad_alloc const&) {
    return std::forward<OnOOM>(on_oom)();
  }
}
#else
template <typename F, typename OnOOM>
constexpr auto with_bad_alloc_guard(F&& f, OnOOM&&) -> decltype(f()) {
  return std::forward<F>(f)();
}
#endif

} // namespace lexborpp::detail
