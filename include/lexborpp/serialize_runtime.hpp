#ifndef LEXBORPP_SERIALIZE_RUNTIME_HPP_
#define LEXBORPP_SERIALIZE_RUNTIME_HPP_

#include <cstddef>
#include <string>

#include "lexbor/html/serialize.h"
#include "lexbor/dom/dom.h"

namespace lexborpp {
namespace detail {

/**
 * @brief HTML シリアライズ結果を文字列へ追記するコールバックです。
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

}  // namespace lexborpp

#endif  // LEXBORPP_SERIALIZE_RUNTIME_HPP_
