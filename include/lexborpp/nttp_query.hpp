#ifndef LEXBORPP_NTTP_QUERY_HPP_
#define LEXBORPP_NTTP_QUERY_HPP_

#include <string_view>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_match.hpp"
#include "lexborpp/nttp_parser.hpp"
#include "lexborpp/document_id_index.hpp"

namespace lexborpp {
/**
 * @brief NTTP で渡された CSS セレクタにマッチする最初の要素を返します。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector(lxb_dom_node_t* node) -> lxb_dom_node_t* {
  if constexpr (Selector.view().empty()) {
    return nullptr;
  } else {
    return detail::query_selector_impl<Selector>(node);
  }
}

/**
 * @brief NTTP で渡された CSS セレクタにマッチするすべての要素を返します。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all(lxb_dom_node_t* node) -> std::vector<lxb_dom_node_t*> {
  if constexpr (Selector.view().empty()) {
    return {};
  } else {
    return detail::query_selector_all_impl<Selector>(node);
  }
}

/**
 * @brief ID 逆引きインデックスを利用して CSS セレクタにマッチする最初の要素を返します。
 *
 * 右端の compound に id selector が含まれる場合、インデックス経由で O(1) ルックアップします。
 * それ以外の selector は通常のツリー走査にフォールバックします。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector(
  lxb_dom_node_t* node,
  document_id_index const& index) -> lxb_dom_node_t* {
  if constexpr (Selector.view().empty()) {
    return nullptr;
  } else {
    return detail::query_selector_impl<Selector>(node, index);
  }
}

/**
 * @brief ID 逆引きインデックスを利用して CSS セレクタにマッチする全要素を返します。
 *
 * 単一 compound かつ id selector を含む場合のみインデックス経由でルックアップします。
 * それ以外は通常のツリー走査にフォールバックします。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all(
  lxb_dom_node_t* node,
  document_id_index const& index) -> std::vector<lxb_dom_node_t*> {
  if constexpr (Selector.view().empty()) {
    return {};
  } else {
    return detail::query_selector_all_impl<Selector>(node, index);
  }
}
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_QUERY_HPP_
