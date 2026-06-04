#ifndef LEXBORPP_NTTP_MATCH_HPP_
#define LEXBORPP_NTTP_MATCH_HPP_

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {

namespace detail {

template <detail::fixed_string Selector>
inline constexpr auto selector_storage_capacity_v = Selector.view().empty() ? std::size_t{1} : Selector.view().size();

template <detail::fixed_string Selector>
inline constexpr auto compiled_selector_v = parse_selector_spec<Selector>();

// --- Match a single simple selector (compile-time kind dispatch) ---
template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI, std::size_t SimpleI>
[[nodiscard]] constexpr auto match_simple(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto& spec = compiled_selector_v<Selector>.groups[GI].compounds[CompoundI].simples[SimpleI];
  using kind = selector_simple_kind;

  if constexpr (spec.kind == kind::universal) {
    return not is_non_element_node(node);
  } else {
    if (node == nullptr || is_non_element_node(node)) {
      return false;
    }

    if constexpr (spec.kind == kind::type) {
      return iequals(node_qualified_name(node), spec.value);
    } else if constexpr (spec.kind == kind::id) {
      return get_attr_value(node, "id") == spec.value;
    } else if constexpr (spec.kind == kind::class_name) {
      return has_class(node, spec.value);
    } else if constexpr (spec.kind == kind::attribute) {
      auto const value = get_attr_value(node, spec.name);
      if (not value.has_value()) {
        return false;
      }
      return match_attribute(*value, spec.value, spec.attribute_match);
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
  constexpr auto simple_count = compiled_selector_v<Selector>.groups[GI].compounds[CompoundI].simple_count;
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

  constexpr auto relation = compiled_selector_v<Selector>.groups[GI].compounds[I].relation;

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
  constexpr auto compound_count = compiled_selector_v<Selector>.groups[GI].compound_count;
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

}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_MATCH_HPP_
