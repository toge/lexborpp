#ifndef LEXBORPP_SERIALIZE_RUNTIME_HPP_
#define LEXBORPP_SERIALIZE_RUNTIME_HPP_

#include <cstddef>
#include <string>

#include "lexbor/html/serialize.h"
#include "lexbor/dom/dom.h"

#include "lexborpp/config.hpp"

namespace lexborpp {
namespace detail {

/**
 * @brief HTML シリアライズ結果を文字列へ追記するコールバックです。
 *
 * noexcept 境界から例外を漏らさないため、OOM はエラーステータスで報告します。
 */
inline auto serialize_callback(const lxb_char_t* data, size_t len, void* ctx) noexcept -> lxb_status_t {
  auto* const str = static_cast<std::string*>(ctx);
  return detail::with_bad_alloc_guard(
    [&] {
      str->append(reinterpret_cast<const char*>(data), len);
      return LXB_STATUS_OK;
    },
    [] { return LXB_STATUS_ERROR_MEMORY_ALLOCATION; });
}

[[noreturn]] inline auto raise_serialization_error(lxb_status_t status) -> void {
#ifdef __cpp_exceptions
  if (status == LXB_STATUS_ERROR_MEMORY_ALLOCATION) {
    throw std::bad_alloc{};
  }
  throw std::runtime_error{"Lexbor serialization failed"};
#else
  (void)status;
  std::abort();
#endif
}

} // namespace detail

/**
 * @brief ノードの外部 HTML（自身を含む）を取得します。
 */
[[nodiscard]] auto inline outer_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(512); // ponytail: 128では2.5KB要素で再確保、512で1回に
  auto const status =
    lxb_html_serialize_tree_cb(const_cast<lxb_dom_node_t*>(node), detail::serialize_callback, &result);
  if (status != LXB_STATUS_OK) {
    detail::raise_serialization_error(status);
  }
  return result;
}

/**
 * @brief ノードの内部 HTML（子ノード群）を取得します。
 */
[[nodiscard]] auto inline inner_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(512);
  for (auto* child = lxb_dom_node_first_child(const_cast<lxb_dom_node_t*>(node)); child != nullptr; child = lxb_dom_node_next(child)) {
    auto const status = lxb_html_serialize_tree_cb(child, detail::serialize_callback, &result);
    if (status != LXB_STATUS_OK) {
      detail::raise_serialization_error(status);
    }
  }
  return result;
}

}  // namespace lexborpp

#endif  // LEXBORPP_SERIALIZE_RUNTIME_HPP_
