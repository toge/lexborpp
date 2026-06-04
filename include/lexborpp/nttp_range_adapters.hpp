#ifndef LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_
#define LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_

#include <ranges>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {
/**
 * @brief 指定したタグ名のいずれかに一致するノードのみを通過させる Range アダプタです。
 *
 * @tparam Tags 対象タグです (例: LXB_TAG_TABLE, LXB_TAG_TR)。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::tag<LXB_TAG_TABLE>
 * lexborpp::node_walker(root) | lexborpp::tag<LXB_TAG_TABLE, LXB_TAG_TBODY>
 * @endcode
 */
template <lxb_tag_id_t... Tags>
inline constexpr auto tag = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return node != nullptr && ((node->local_name == Tags) || ...);
});

/**
 * @brief 指定した id 属性値を持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Id 対象の id 属性値です (例: "forecast-point-3h-today")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::id<"forecast-point-3h-today">
 * @endcode
 */
template <detail::fixed_string Id>
inline constexpr auto id = std::views::filter(
    [](lxb_dom_node_t* node) noexcept { return lexborpp::get_attr_value(node, "id") == Id.view(); });

/**
 * @brief 指定した class 属性値のいずれかを持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Classes 対象の class 属性値です (例: "section-wrap")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::clazz<"section-wrap">
 * lexborpp::node_walker(root) | lexborpp::clazz<"section-wrap", "section-wrap active">
 * @endcode
 */
template <detail::fixed_string... Classes>
inline constexpr auto clazz = std::views::filter([](lxb_dom_node_t* node) noexcept {
  auto const class_value = lexborpp::get_attr_value(node, "class");
  return class_value.has_value() && ((*class_value == Classes.view()) || ...);
});

/**
 * @brief 指定した属性名・属性値を持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Attr 属性名です (例: "class")。
 * @tparam Value 属性値です (例: "section-wrap")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::attr<"class", "section-wrap">
 * @endcode
 */
template <detail::fixed_string Attr, detail::fixed_string Value>
inline constexpr auto attr = std::views::filter(
    [](lxb_dom_node_t* node) noexcept { return lexborpp::get_attr_value(node, Attr.view()) == Value.view(); });
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_
