#ifndef LEXBORPP_NTTP_MATCH_HPP_
#define LEXBORPP_NTTP_MATCH_HPP_

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"
#include "lexborpp/document_id_index.hpp"

namespace lexborpp {

namespace detail {

template <detail::fixed_string Selector>
inline constexpr auto compiled_selector_v = parse_selector_spec<Selector>();

// --- Match a single simple selector (compile-time kind dispatch) ---
template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI, std::size_t SimpleI>
[[nodiscard]] constexpr auto match_simple(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto& spec = compiled_selector_v<Selector>;
  constexpr auto& g = spec.groups[GI];
  constexpr auto& c = spec.compounds[g.compound_start + CompoundI];
  constexpr auto& s = spec.simples[c.simple_start + SimpleI];
  using kind = selector_simple_kind;

  if constexpr (s.kind == kind::universal) {
    return not is_non_element_node(node);
  } else {
    if (node == nullptr || is_non_element_node(node)) {
      return false;
    }

    if constexpr (s.kind == kind::type) {
      if constexpr (s.tag_id != LXB_TAG__UNDEF) {
        return lxb_dom_node_tag_id(const_cast<lxb_dom_node_t*>(node)) == s.tag_id;
      } else {
        return iequals(node_qualified_name(node), s.value);
      }
    } else if constexpr (s.kind == kind::id) {
      return get_attr_value(node, "id") == s.value;
    } else if constexpr (s.kind == kind::class_name) {
      return has_class(node, s.value);
    } else if constexpr (s.kind == kind::attribute) {
      auto const value = get_attr_value(node, s.name);
      if (not value.has_value()) {
        return false;
      }
      return match_attribute(*value, s.value, s.attribute_match);
    }
  }
}

// --- Match all simple selectors in compound I (AND via fold) ---
template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI, std::size_t... SimpleIs>
[[nodiscard]] constexpr auto match_compound_impl(
  lxb_dom_node_t* node,
  std::index_sequence<SimpleIs...>) noexcept -> bool {
  return (... && match_simple<Selector, GI, CompoundI, SimpleIs>(node));
}

template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI>
[[nodiscard]] constexpr auto match_compound(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto& spec = compiled_selector_v<Selector>;
  constexpr auto& g = spec.groups[GI];
  constexpr auto& c = spec.compounds[g.compound_start + CompoundI];
  constexpr auto simple_count = c.simple_count;
  return match_compound_impl<Selector, GI, CompoundI>(
    node, std::make_index_sequence<simple_count>{});
}

// --- Recursive right-to-left chain matching ---
// Base case: I == 0, just match this compound
template <detail::fixed_string Selector, std::size_t GI, std::size_t I = 0>
[[nodiscard]] constexpr auto match_compound_chain(
  lxb_dom_node_t* node) noexcept -> bool {
  return match_compound<Selector, GI, I>(node);
}

// Recursive case: match current, then follow combinator to parent/sibling
template <detail::fixed_string Selector, std::size_t GI, std::size_t I>
  requires (I > 0)
[[nodiscard]] constexpr auto match_compound_chain(
  lxb_dom_node_t* node) noexcept -> bool {
  if (not match_compound<Selector, GI, I>(node)) {
    return false;
  }

  constexpr auto& spec = compiled_selector_v<Selector>;
  constexpr auto& g = spec.groups[GI];
  constexpr auto& c = spec.compounds[g.compound_start + I];
  constexpr auto relation = c.relation;

  if constexpr (relation == selector_combinator::child) {
    return match_compound_chain<Selector, GI, I - 1>(parent_element(node));
  } else if constexpr (relation == selector_combinator::adjacent_sibling) {
    return match_compound_chain<Selector, GI, I - 1>(prev_element_sibling(node));
  } else if constexpr (relation == selector_combinator::following_sibling) {
    for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
      if (match_compound_chain<Selector, GI, I - 1>(prev)) {
        return true;
      }
    }
    return false;
  } else {
    // descendant: walk up ancestors
    for (auto* ancestor = parent_element(node); ancestor != nullptr; ancestor = parent_element(ancestor)) {
      if (match_compound_chain<Selector, GI, I - 1>(ancestor)) {
        return true;
      }
    }
    return false;
  }
}

// --- Match a selector group ---
template <detail::fixed_string Selector, std::size_t GI>
[[nodiscard]] constexpr auto match_group(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto& g = compiled_selector_v<Selector>.groups[GI];
  constexpr auto compound_count = g.compound_count;
  if constexpr (compound_count == 0) {
    return false;
  } else {
    return match_compound_chain<Selector, GI, compound_count - 1>(node);
  }
}

// --- Match all selector groups (comma-separated) ---
template <detail::fixed_string Selector, std::size_t... GIs>
[[nodiscard]] constexpr auto match_all_groups(
  lxb_dom_node_t* node,
  std::index_sequence<GIs...>) noexcept -> bool {
  return (... || match_group<Selector, GIs>(node));
}

// --- Public match entry point ---
template <detail::fixed_string Selector>
[[nodiscard]] constexpr auto match_selector_compiled(
  lxb_dom_node_t* node) noexcept -> bool {
  if (node == nullptr) {
    return false;
  }
  constexpr auto group_count = compiled_selector_v<Selector>.group_count;
  return match_all_groups<Selector>(
    node, std::make_index_sequence<group_count>{});
}

// --- Query implementations (walk DOM, test each node) ---
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_impl(lxb_dom_node_t* node) -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current)) {
      return current;
    }
  }
  return nullptr;
}

template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all_impl(lxb_dom_node_t* node) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr) {
    return result;
  }
  result.reserve(16);
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current)) {
      result.push_back(current);
    }
  }
  return result;
}

// --- Compile-time id prefilter extraction ---
template <detail::fixed_string Selector>
constexpr auto compiled_id_prefilter() -> std::string_view {
  constexpr auto& spec = compiled_selector_v<Selector>;
  if constexpr (spec.group_count == 0) {
    return {};
  } else {
    constexpr auto& g = spec.groups[0];
    if constexpr (g.compound_count == 0) {
      return {};
    } else {
      constexpr auto& c = spec.compounds[g.compound_start + g.compound_count - 1];
      for (auto i = std::size_t{}; i < c.simple_count; ++i) {
        if (spec.simples[c.simple_start + i].kind == selector_simple_kind::id) {
          return spec.simples[c.simple_start + i].value;
        }
      }
      return {};
    }
  }
}

struct nttp_id_info {
  std::string_view value{};
  std::size_t compound_idx{};
  bool found{false};
  bool is_last{false};
};

template <detail::fixed_string Selector>
constexpr auto compiled_id_info() -> nttp_id_info {
  constexpr auto& spec = compiled_selector_v<Selector>;
  if constexpr (spec.group_count != 1) {
    return {};
  } else {
    constexpr auto& g = spec.groups[0];
    for (auto ci = g.compound_count; ci-- > 0;) {
      auto const& c = spec.compounds[g.compound_start + ci];
      for (auto si = std::size_t{0}; si < c.simple_count; ++si) {
        if (spec.simples[c.simple_start + si].kind == selector_simple_kind::id) {
          return {spec.simples[c.simple_start + si].value, ci, true, ci == g.compound_count - 1};
        }
      }
    }
    return {};
  }
}

// --- Query implementations with document_id_index ---
// NOTE: インデックスは構築時のスナップショットです。構築後に DOM を編集した場合、
//       結果は古いノードを指す可能性があります。検索の直前に rebuild してください。
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_impl(
  lxb_dom_node_t* node,
  document_id_index const& index) -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }
  constexpr auto info = compiled_id_info<Selector>();
  if constexpr (info.found) {
    auto* found = index.find(info.value);
    if (found == nullptr || !is_descendant_of(found, node)) return nullptr;
    if constexpr (info.is_last) {
      if (match_selector_compiled<Selector>(found)) return found;
      return nullptr;
    } else {
      // id is not in last compound: limit search to subtree of found
      for (auto* cur : node_walker{found}) {
        if (is_non_element_node(cur)) continue;
        if (match_selector_compiled<Selector>(cur)) return cur;
      }
      return nullptr;
    }
  } else {
    return query_selector_impl<Selector>(node);
  }
}

template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all_impl(
  lxb_dom_node_t* node,
  document_id_index const& index) -> std::vector<lxb_dom_node_t*> {
  if (node == nullptr) {
    return {};
  }
  constexpr auto info = compiled_id_info<Selector>();
  if constexpr (info.found) {
    auto* found = index.find(info.value);
    if (found == nullptr || !is_descendant_of(found, node)) return {};
    if constexpr (info.is_last) {
      if constexpr (compiled_selector_v<Selector>.groups[0].compound_count == 1) {
        if (match_selector_compiled<Selector>(found)) return {found};
        return {};
      } else {
        if (match_selector_compiled<Selector>(found)) return {found};
        return {};
      }
    } else {
      auto result = std::vector<lxb_dom_node_t*>{};
      result.reserve(16);
      for (auto* cur : node_walker{found}) {
        if (is_non_element_node(cur)) continue;
        if (match_selector_compiled<Selector>(cur)) result.push_back(cur);
      }
      return result;
    }
  } else {
    return query_selector_all_impl<Selector>(node);
  }
}

}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_MATCH_HPP_
