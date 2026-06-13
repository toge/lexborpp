#ifndef LEXBORPP_RUNTIME_CSS_MATCH_HPP_
#define LEXBORPP_RUNTIME_CSS_MATCH_HPP_

#include <string_view>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"
#include "lexborpp/runtime_css_parser.hpp"

namespace lexborpp {
namespace detail {

// Match one simple selector against a node
[[nodiscard]] inline auto match_runtime_simple(
  lxb_dom_node_t* node,
  runtime_simple_spec const& simple) noexcept -> bool {
  if (simple.kind == selector_simple_kind::universal) {
    return not is_non_element_node(node);
  }
  if (node == nullptr || is_non_element_node(node)) {
    return false;
  }
  switch (simple.kind) {
  case selector_simple_kind::type:
    return iequals(node_qualified_name(node), simple.value);
  case selector_simple_kind::id:
    return get_attr_value(node, "id") == simple.value;
  case selector_simple_kind::class_name:
    return has_class(node, simple.value);
  case selector_simple_kind::attribute: {
    auto const value = get_attr_value(node, simple.name);
    if (not value.has_value()) return false;
    return match_attribute(*value, simple.value, simple.attribute_match);
  }
  case selector_simple_kind::universal:
    return true;
  }
  return false;
}

// Match all simples in a compound (AND)
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_compound(
  lxb_dom_node_t* node,
  runtime_compound_spec<Max> const& compound) noexcept -> bool {
  if (node == nullptr || is_non_element_node(node)) return false;
  for (auto i = std::size_t{0}; i < compound.simple_count; ++i) {
    if (not match_runtime_simple(node, compound.simples[i])) return false;
  }
  return true;
}

// Recursive right-to-left chain matching (loop-based)
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_chain(
  lxb_dom_node_t* node,
  runtime_group_spec<Max> const& group,
  std::size_t index) noexcept -> bool {
  if (node == nullptr || index >= group.compound_count) return false;

  if (not match_runtime_compound(node, group.compounds[index])) return false;

  if (index == 0) return true;

  auto const relation = group.compounds[index].relation;
  switch (relation) {
  case selector_combinator::child:
    return match_runtime_chain(parent_element(node), group, index - 1);
  case selector_combinator::adjacent_sibling:
    return match_runtime_chain(prev_element_sibling(node), group, index - 1);
  case selector_combinator::following_sibling:
    for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
      if (match_runtime_chain(prev, group, index - 1)) return true;
    }
    return false;
  case selector_combinator::descendant:
    for (auto* ancestor = parent_element(node); ancestor != nullptr; ancestor = parent_element(ancestor)) {
      if (match_runtime_chain(ancestor, group, index - 1)) return true;
    }
    return false;
  }
  return false;
}

// Match one group
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_group(
  lxb_dom_node_t* node,
  runtime_group_spec<Max> const& group) noexcept -> bool {
  if (group.compound_count == 0) return false;
  return match_runtime_chain(node, group, group.compound_count - 1);
}

// Match all groups (OR)
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_selector(
  lxb_dom_node_t* node,
  runtime_selector_spec<Max> const& selector) noexcept -> bool {
  if (node == nullptr) return false;
  for (auto i = std::size_t{0}; i < selector.group_count; ++i) {
    if (match_runtime_group(node, selector.groups[i])) return true;
  }
  return false;
}

// Public API: query_selector (runtime)
[[nodiscard]] inline auto query_selector_runtime(
  lxb_dom_node_t* node,
  std::string_view selector) -> lxb_dom_node_t* {
  if (node == nullptr || selector.empty()) return nullptr;

  auto spec = parse_runtime_selector_auto(selector);

  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) continue;
    if (match_runtime_selector(current, spec)) return current;
  }
  return nullptr;
}

// Public API: query_selector_all (runtime)
[[nodiscard]] inline auto query_selector_all_runtime(
  lxb_dom_node_t* node,
  std::string_view selector) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr || selector.empty()) return result;

  auto spec = parse_runtime_selector_auto(selector);
  result.reserve(16);

  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) continue;
    if (match_runtime_selector(current, spec)) result.push_back(current);
  }
  return result;
}

}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_RUNTIME_CSS_MATCH_HPP_
