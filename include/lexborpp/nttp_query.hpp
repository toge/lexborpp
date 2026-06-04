#ifndef LEXBORPP_NTTP_QUERY_HPP_
#define LEXBORPP_NTTP_QUERY_HPP_

#include <string_view>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_match.hpp"
#include "lexborpp/nttp_parser.hpp"

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
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_QUERY_HPP_
