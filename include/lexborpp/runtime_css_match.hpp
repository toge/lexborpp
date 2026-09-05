#ifndef LEXBORPP_RUNTIME_CSS_MATCH_HPP_
#define LEXBORPP_RUNTIME_CSS_MATCH_HPP_

#include <expected>
#include <string_view>
#include <system_error>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"
#include "lexborpp/runtime_css_parser.hpp"
#include "lexborpp/document_id_index.hpp"

namespace lexborpp {
namespace detail {

// Match one simple selector against a node (flat)
[[nodiscard]] inline auto match_runtime_simple(
  lxb_dom_node_t* node,
  selector_simple_spec const& simple) noexcept -> bool {
  if (simple.kind == selector_simple_kind::universal) {
    return not is_non_element_node(node);
  }
  if (node == nullptr || is_non_element_node(node)) return false;
  switch (simple.kind) {
  case selector_simple_kind::type:
    if (simple.tag_id != LXB_TAG__UNDEF) return lxb_dom_node_tag_id(const_cast<lxb_dom_node_t*>(node)) == simple.tag_id;
    return iequals(node_qualified_name(node), simple.value);
  case selector_simple_kind::id:
    return get_attr_value(node, "id") == simple.value;
  case selector_simple_kind::class_name:
    return has_class(node, simple.value);
  case selector_simple_kind::attribute: {
    auto const v = get_attr_value(node, simple.name);
    if (!v.has_value()) return false;
    return match_attribute(*v, simple.value, simple.attribute_match);
  }
  default: return false;
  }
}

// Match all simples in a compound (AND)
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_compound(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec,
  std::size_t compound_idx) noexcept -> bool {
  if (node == nullptr || is_non_element_node(node)) return false;
  auto const& c = spec.compounds[compound_idx];
  if (c.simple_count == 0) return false;
  for (auto i = std::size_t{0}; i < c.simple_count; ++i) {
    if (!match_runtime_simple(node, spec.simples[c.simple_start + i])) return false;
  }
  return true;
}

// Recursive right-to-left chain matching (flat)
template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_chain(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec,
  std::size_t group_idx,
  std::size_t compound_pos) noexcept -> bool {
  if (node == nullptr) return false;
  auto const& g = spec.groups[group_idx];
  if (compound_pos >= g.compound_count) return false;
  auto const compound_idx = g.compound_start + compound_pos;
  if (!match_runtime_compound(node, spec, compound_idx)) return false;
  if (compound_pos == 0) return true;
  auto const relation = spec.compounds[compound_idx].relation;
  switch (relation) {
  case selector_combinator::child:
    return match_runtime_chain(parent_element(node), spec, group_idx, compound_pos - 1);
  case selector_combinator::adjacent_sibling:
    return match_runtime_chain(prev_element_sibling(node), spec, group_idx, compound_pos - 1);
  case selector_combinator::following_sibling:
    for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
      if (match_runtime_chain(prev, spec, group_idx, compound_pos - 1)) return true;
    }
    return false;
  case selector_combinator::descendant:
    for (auto* anc = parent_element(node); anc != nullptr; anc = parent_element(anc)) {
      if (match_runtime_chain(anc, spec, group_idx, compound_pos - 1)) return true;
    }
    return false;
  }
  return false;
}

template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_group(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec,
  std::size_t group_idx) noexcept -> bool {
  auto const& g = spec.groups[group_idx];
  if (g.compound_count == 0) return false;
  return match_runtime_chain(node, spec, group_idx, g.compound_count - 1);
}

template <std::size_t Max>
[[nodiscard]] inline auto match_runtime_selector(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec) noexcept -> bool {
  if (node == nullptr) return false;
  for (auto i = std::size_t{0}; i < spec.group_count; ++i) {
    if (match_runtime_group(node, spec, i)) return true;
  }
  return false;
}

// --- Spec-based scan cores ---

template <std::size_t Max>
[[nodiscard]] inline auto query_selector_spec_first(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) return nullptr;
  for (auto* cur : node_walker{node}) {
    if (is_non_element_node(cur)) continue;
    if (match_runtime_selector(cur, spec)) return cur;
  }
  return nullptr;
}

template <std::size_t Max>
[[nodiscard]] inline auto query_selector_spec_all(
  lxb_dom_node_t* node,
  selector_spec<Max> const& spec) noexcept -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr) return result;
  result.reserve(16);
  for (auto* cur : node_walker{node}) {
    if (is_non_element_node(cur)) continue;
    if (match_runtime_selector(cur, spec)) result.push_back(cur);
  }
  return result;
}

// Public API: query_selector (runtime) - returns std::expected
[[nodiscard]] inline auto query_selector_runtime(
  lxb_dom_node_t* node,
  std::string_view selector) noexcept -> std::expected<lxb_dom_node_t*, std::errc> {
  if (node == nullptr || selector.empty()) return std::unexpected(std::errc::invalid_argument);
  auto spec = parse_runtime_selector_auto(selector);
  if (!spec) return std::unexpected(spec.error());
  return query_selector_spec_first(node, *spec);
}

[[nodiscard]] inline auto query_selector_all_runtime(
  lxb_dom_node_t* node,
  std::string_view selector) noexcept -> std::expected<std::vector<lxb_dom_node_t*>, std::errc> {
  if (node == nullptr || selector.empty()) return std::unexpected(std::errc::invalid_argument);
  auto spec = parse_runtime_selector_auto(selector);
  if (!spec) return std::unexpected(spec.error());
  return query_selector_spec_all(node, *spec);
}

// --- Runtime id prefilter helpers (expanded) ---
struct runtime_id_prefilter {
  std::string_view value{};
  std::size_t compound_idx{}; // index within group (0-based)
  std::size_t group_idx{};
  bool found{false};
  bool is_last{false};
};

template <std::size_t Max>
[[nodiscard]] inline auto runtime_get_id_prefilter(
  selector_spec<Max> const& spec) noexcept -> runtime_id_prefilter {
  if (spec.group_count != 1) return {};
  auto const& g = spec.groups[0];
  // search from rightmost compound to leftmost for id
  for (auto ci = g.compound_count; ci-- > 0; ) {
    auto const cidx = g.compound_start + ci;
    auto const& c = spec.compounds[cidx];
    for (auto si = std::size_t{0}; si < c.simple_count; ++si) {
      auto const& s = spec.simples[c.simple_start + si];
      if (s.kind == selector_simple_kind::id) {
        return {s.value, ci, 0, true, ci == g.compound_count - 1};
      }
    }
  }
  return {};
}

// query_selector_runtime with index (expanded: supports id in any compound)
[[nodiscard]] inline auto query_selector_runtime(
  lxb_dom_node_t* node,
  std::string_view selector,
  document_id_index const& index) noexcept -> std::expected<lxb_dom_node_t*, std::errc> {
  if (node == nullptr || selector.empty()) return std::unexpected(std::errc::invalid_argument);
  auto spec = parse_runtime_selector_auto(selector);
  if (!spec) return std::unexpected(spec.error());
  if (spec->group_count == 0) return std::unexpected(std::errc::invalid_argument);
  auto const pf = runtime_get_id_prefilter(*spec);
  if (pf.found) {
    auto* found = index.find(pf.value);
    if (found == nullptr || !is_descendant_of(found, node)) return nullptr;
    if (pf.is_last) {
      if (match_runtime_selector(found, *spec)) return found;
      return nullptr;
    } else {
      // id is not in rightmost compound: limit search to subtree of found
      return query_selector_spec_first(found, *spec);
    }
  }
  return query_selector_spec_first(node, *spec);
}

[[nodiscard]] inline auto query_selector_all_runtime(
  lxb_dom_node_t* node,
  std::string_view selector,
  document_id_index const& index) noexcept -> std::expected<std::vector<lxb_dom_node_t*>, std::errc> {
  if (node == nullptr || selector.empty()) return std::unexpected(std::errc::invalid_argument);
  auto spec = parse_runtime_selector_auto(selector);
  if (!spec) return std::unexpected(spec.error());
  if (spec->group_count == 0) return std::unexpected(std::errc::invalid_argument);
  auto const pf = runtime_get_id_prefilter(*spec);
  if (pf.found) {
    auto* found = index.find(pf.value);
    if (found == nullptr || !is_descendant_of(found, node)) return std::vector<lxb_dom_node_t*>{};
    if (pf.is_last && spec->groups[0].compound_count == 1) {
      if (match_runtime_selector(found, *spec)) return std::vector<lxb_dom_node_t*>{found};
      return std::vector<lxb_dom_node_t*>{};
    }
    if (pf.is_last) {
      // last compound has id but group has multiple compounds: still single-node check suffices
      // because rightmost compound is the id; if that node matches whole chain, it's unique.
      if (match_runtime_selector(found, *spec)) return std::vector<lxb_dom_node_t*>{found};
      return std::vector<lxb_dom_node_t*>{};
    } else {
      return query_selector_spec_all(found, *spec);
    }
  }
  return query_selector_spec_all(node, *spec);
}

}  // namespace detail

// --- Public runtime query API ---

[[nodiscard]] auto inline query_selector(lxb_dom_node_t* node, std::string_view selector) noexcept -> std::expected<lxb_dom_node_t*, std::errc> {
  return detail::query_selector_runtime(node, selector);
}
[[nodiscard]] auto inline query_selector_all(lxb_dom_node_t* node, std::string_view selector) noexcept -> std::expected<std::vector<lxb_dom_node_t*>, std::errc> {
  return detail::query_selector_all_runtime(node, selector);
}
[[nodiscard]] auto inline query_selector(
  lxb_dom_node_t* node,
  std::string_view selector,
  document_id_index const& index) noexcept -> std::expected<lxb_dom_node_t*, std::errc> {
  return detail::query_selector_runtime(node, selector, index);
}
[[nodiscard]] auto inline query_selector_all(
  lxb_dom_node_t* node,
  std::string_view selector,
  document_id_index const& index) noexcept -> std::expected<std::vector<lxb_dom_node_t*>, std::errc> {
  return detail::query_selector_all_runtime(node, selector, index);
}

}  // namespace lexborpp

#endif  // LEXBORPP_RUNTIME_CSS_MATCH_HPP_
