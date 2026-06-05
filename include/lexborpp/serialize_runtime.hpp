#ifndef LEXBORPP_SERIALIZE_RUNTIME_HPP_
#define LEXBORPP_SERIALIZE_RUNTIME_HPP_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "lexbor/html/serialize.h"
#include "lexbor/dom/dom.h"

#include "lexborpp/runtime_css_match.hpp"

namespace lexborpp {
namespace detail {

/**
 * @brief HTML シリアライズ結果を文字列へ追記するコールバックです。
 *
 * @param data 追記する文字列データです。
 * @param len データ長です。
 * @param ctx `std::string*` を指すコンテキストです。
 * @return lxb_status_t 常に `LXB_STATUS_OK` を返します。
 */
inline auto serialize_callback(const lxb_char_t* data, size_t len, void* ctx) noexcept -> lxb_status_t {
  auto* const str = static_cast<std::string*>(ctx);
  str->append(reinterpret_cast<const char*>(data), len);
  return LXB_STATUS_OK;
}

} // namespace detail

/**
 * @brief ノードの外部 HTML（自身を含む）を取得します。
 */
[[nodiscard]] auto inline outer_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(128);
  lxb_html_serialize_tree_cb(const_cast<lxb_dom_node_t*>(node), detail::serialize_callback, &result);
  return result;
}

/**
 * @brief ノードの内部 HTML（子ノード群）を取得します。
 */
[[nodiscard]] auto inline inner_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(128);
  for (auto* child = lxb_dom_node_first_child(const_cast<lxb_dom_node_t*>(node)); child != nullptr; child = lxb_dom_node_next(child)) {
    lxb_html_serialize_tree_cb(child, detail::serialize_callback, &result);
  }
  return result;
}

/**
 * @brief CSS セレクタにマッチする最初の要素を返します。
 */
[[nodiscard]] auto inline query_selector(lxb_dom_node_t* node, std::string_view selector) -> lxb_dom_node_t* {
  return detail::query_selector_runtime(node, selector);
}

/**
 * @brief CSS セレクタにマッチするすべての要素を返します。
 */
[[nodiscard]] auto inline query_selector_all(lxb_dom_node_t* node, std::string_view selector) -> std::vector<lxb_dom_node_t*> {
  return detail::query_selector_all_runtime(node, selector);
}
}  // namespace lexborpp

#endif  // LEXBORPP_SERIALIZE_RUNTIME_HPP_
