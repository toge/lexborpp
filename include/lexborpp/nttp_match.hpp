#ifndef LEXBORPP_NTTP_MATCH_HPP_
#define LEXBORPP_NTTP_MATCH_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {

namespace detail {
/**
 * @brief selector group の評価結果キャッシュ用キーです。
 */
struct selector_group_cache_key {
  lxb_dom_node_t* node{};
  std::size_t index{};

  /**
   * @brief キー同士が同一か比較します。
   *
   * @param rhs 比較対象です。
   * @return bool ノードとインデックスが一致する場合に true を返します。
   */
  [[nodiscard]] auto operator==(selector_group_cache_key const& rhs) const noexcept -> bool = default;
};

/**
 * @brief `selector_group_cache_key` 用ハッシュ関数です。
 */
struct selector_group_cache_key_hash {
  /**
   * @brief キャッシュキーのハッシュ値を計算します。
   *
  * @param key 対象キーです。
  * @return std::size_t ハッシュ値を返します。
   */
  [[nodiscard]] auto operator()(selector_group_cache_key const& key) const noexcept -> std::size_t {
    auto const node_hash = std::hash<std::uintptr_t>{}(reinterpret_cast<std::uintptr_t>(key.node));
    auto const index_hash = std::hash<std::size_t>{}(key.index);
    return node_hash ^ (index_hash + 0x9e3779b97f4a7c15ULL + (node_hash << 6U) + (node_hash >> 2U));
  }
};

using selector_group_cache = std::unordered_map<selector_group_cache_key, bool, selector_group_cache_key_hash>;

/**
 * @brief selector 評価で使用するキャッシュ群を保持します。
 */
template <std::size_t Max>
struct selector_query_context {
  std::array<selector_group_cache, Max> caches{};
};

template <detail::fixed_string Selector>
inline constexpr auto selector_storage_capacity_v = Selector.view().empty() ? std::size_t{1} : Selector.view().size();

template <detail::fixed_string Selector>
inline constexpr auto compiled_selector_v = parse_selector_spec<Selector>();

/**
 * @brief selector 評価用コンテキストを生成します。
 *
 * @tparam Max 配列容量です。
 * @param selector 対象 selector 情報です。
 * @return selector_query_context<Max> 初期化済みコンテキストを返します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline create_query_context(selector_spec<Max> const& selector) -> selector_query_context<Max> {
  auto context = selector_query_context<Max>{};
  for (auto const group_index : std::views::iota(std::size_t{0}, selector.group_count)) {
    context.caches[group_index].reserve(256);
  }
  return context;
}

/**
 * @brief 1 つの simple selector が対象ノードに一致するか判定します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline match_simple_selector(lxb_dom_node_t* node, selector_simple_spec const& simple) noexcept -> bool {
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
    if (not value.has_value()) {
      return false;
    }
    return match_attribute(*value, simple.value, simple.attribute_match);
  }
  case selector_simple_kind::universal:
    return true;
  }

  return false;
}

/**
 * @brief 1 つの compound selector が対象ノードに一致するか判定します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline match_compound_selector(lxb_dom_node_t* node, selector_compound_spec<Max> const& compound) noexcept -> bool {
  if (node == nullptr || is_non_element_node(node)) {
    return false;
  }

  for (auto const priority : std::views::iota(std::size_t{0}, std::size_t{5})) {
    for (auto const index : std::views::iota(std::size_t{0}, compound.simple_count)) {
      auto const& simple = compound.simples[index];
      if (simple_selector_priority(simple) != priority) {
        continue;
      }
      if (not match_simple_selector<Max>(node, simple)) {
        return false;
      }
    }
  }
  return true;
}

/**
 * @brief prefilter 条件だけで候補ノードを判定します。
 *
 * @tparam Max 配列容量です。
 * @param node 対象ノードです。
 * @param prefilter 絞り込み条件です。
 * @return bool 条件を満たす場合に true を返します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline match_prefilter(lxb_dom_node_t* node, selector_prefilter_spec const& prefilter) noexcept -> bool {
  if (prefilter.kind != selector_prefilter_kind::simple) {
    return true;
  }
  return match_simple_selector<Max>(node, prefilter.simple);
}

/**
 * @brief 1 つの selector group を右から左へ評価します。
 */
template <selector_group_shape Shape, std::size_t Max>
[[nodiscard]] auto inline match_selector_group(
  lxb_dom_node_t* node,
  selector_group_spec<Max> const& group,
  std::size_t index,
  selector_group_cache& cache) noexcept -> bool {
  if (node == nullptr || index >= group.compound_count) {
    return false;
  }

  auto const key = selector_group_cache_key{.node = node, .index = index};
  if (auto const it = cache.find(key); it != cache.end()) {
    return it->second;
  }

  auto matched = false;
  if (match_compound_selector<Max>(node, group.compounds[index])) {
    if (index == 0) {
      matched = true;
  } else if constexpr (Shape == selector_group_shape::simple) {
    matched = false;
  } else if constexpr (Shape == selector_group_shape::child_chain) {
    auto* parent = parent_element(node);
    matched = parent != nullptr && match_selector_group<Shape, Max>(parent, group, index - 1, cache);
  } else if constexpr (Shape == selector_group_shape::adjacent_sibling_chain) {
    auto* prev = prev_element_sibling(node);
    matched = prev != nullptr && match_selector_group<Shape, Max>(prev, group, index - 1, cache);
  } else if constexpr (Shape == selector_group_shape::following_sibling_chain) {
    for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
      if (match_selector_group<Shape, Max>(prev, group, index - 1, cache)) {
        matched = true;
        break;
      }
    }
  } else if constexpr (Shape == selector_group_shape::descendant_chain) {
    for (auto* parent = parent_element(node); parent != nullptr; parent = parent_element(parent)) {
      if (match_selector_group<Shape, Max>(parent, group, index - 1, cache)) {
        matched = true;
        break;
      }
    }
  } else {
    switch (group.compounds[index].relation) {
    case selector_combinator::child: {
      auto* parent = parent_element(node);
      matched = parent != nullptr && match_selector_group<Shape, Max>(parent, group, index - 1, cache);
      break;
    }
    case selector_combinator::adjacent_sibling: {
      auto* prev = prev_element_sibling(node);
      matched = prev != nullptr && match_selector_group<Shape, Max>(prev, group, index - 1, cache);
      break;
    }
    case selector_combinator::following_sibling: {
      for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
        if (match_selector_group<Shape, Max>(prev, group, index - 1, cache)) {
          matched = true;
          break;
        }
      }
      break;
    }
    case selector_combinator::descendant: {
      for (auto* parent = parent_element(node); parent != nullptr; parent = parent_element(parent)) {
        if (match_selector_group<Shape, Max>(parent, group, index - 1, cache)) {
          matched = true;
          break;
        }
      }
      break;
    }
    }
  }
  }

  cache.emplace(key, matched);
  return matched;
}

/**
 * @brief selector group の形状に応じて一致判定を振り分けます。
 *
 * @tparam Max 配列容量です。
 * @param node 対象ノードです。
 * @param group 対象 selector group です。
 * @param cache 評価キャッシュです。
 * @return bool 一致した場合に true を返します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline match_selector_group_by_shape(
  lxb_dom_node_t* node,
  selector_group_spec<Max> const& group,
  selector_group_cache& cache) noexcept -> bool {
  if (group.compound_count == 0) {
  return false;
  }

  switch (group.shape) {
  case selector_group_shape::simple:
  return match_selector_group<selector_group_shape::simple, Max>(node, group, group.compound_count - 1, cache);
  case selector_group_shape::descendant_chain:
  return match_selector_group<selector_group_shape::descendant_chain, Max>(node, group, group.compound_count - 1, cache);
  case selector_group_shape::child_chain:
  return match_selector_group<selector_group_shape::child_chain, Max>(node, group, group.compound_count - 1, cache);
  case selector_group_shape::adjacent_sibling_chain:
  return match_selector_group<selector_group_shape::adjacent_sibling_chain, Max>(node, group, group.compound_count - 1, cache);
  case selector_group_shape::following_sibling_chain:
  return match_selector_group<selector_group_shape::following_sibling_chain, Max>(node, group, group.compound_count - 1, cache);
  case selector_group_shape::mixed:
  return match_selector_group<selector_group_shape::mixed, Max>(node, group, group.compound_count - 1, cache);
  }

  return false;
}

/**
 * @brief 解析済み selector 全体にノードが一致するか判定します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline match_selector(
  lxb_dom_node_t* node,
  selector_spec<Max> const& selector,
  selector_query_context<Max>& context) noexcept -> bool {
  if (node == nullptr) {
    return false;
  }

  for (auto const group_index : std::views::iota(std::size_t{0}, selector.group_count)) {
    auto const& group = selector.groups[group_index];
    if (group.compound_count == 0) {
      continue;
    }

    if (not match_prefilter<Max>(node, group.prefilter)) {
      continue;
    }

    if (match_selector_group_by_shape<Max>(node, group, context.caches[group_index])) {
      return true;
    }
  }
  return false;
}

/**
 * @brief コンパイル済み selector group の一致判定を行います。
 *
 * @tparam Selector コンパイル済み selector 文字列です。
 * @tparam GroupIndex 対象 group のインデックスです。
 * @param node 対象ノードです。
 * @param context 評価コンテキストです。
 * @return bool 一致した場合に true を返します。
 */
template <detail::fixed_string Selector, std::size_t GroupIndex>
[[nodiscard]] auto inline match_selector_group_compiled(
  lxb_dom_node_t* node,
  selector_query_context<selector_storage_capacity_v<Selector>>& context) noexcept -> bool {
  constexpr auto const& group = compiled_selector_v<Selector>.groups[GroupIndex];
  constexpr auto const shape = group.shape;

  if constexpr (group.compound_count == 0) {
    return false;
  } else {
    if (not match_prefilter<selector_storage_capacity_v<Selector>>(node, group.prefilter)) {
      return false;
    }
    return match_selector_group<shape, selector_storage_capacity_v<Selector>>(
      node,
      group,
      group.compound_count - 1,
      context.caches[GroupIndex]);
  }
}

/**
 * @brief すべてのコンパイル済み selector group を順に評価します。
 *
 * @tparam Selector コンパイル済み selector 文字列です。
 * @tparam GroupIndices 評価対象 group インデックス列です。
 * @param node 対象ノードです。
 * @param context 評価コンテキストです。
 * @param indices 不使用の index sequence です。
 * @return bool いずれかの group に一致した場合に true を返します。
 */
template <detail::fixed_string Selector, std::size_t... GroupIndices>
[[nodiscard]] auto inline match_selector_compiled_impl(
  lxb_dom_node_t* node,
  selector_query_context<selector_storage_capacity_v<Selector>>& context,
  [[maybe_unused]] std::index_sequence<GroupIndices...> indices) noexcept -> bool {
  return (... || match_selector_group_compiled<Selector, GroupIndices>(node, context));
}

/**
 * @brief コンパイル済み selector 全体にノードが一致するか判定します。
 *
 * @tparam Selector コンパイル済み selector 文字列です。
 * @param node 対象ノードです。
 * @param context 評価コンテキストです。
 * @return bool 一致した場合に true を返します。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline match_selector_compiled(
  lxb_dom_node_t* node,
  selector_query_context<selector_storage_capacity_v<Selector>>& context) noexcept -> bool {
  if (node == nullptr) {
    return false;
  }
  return match_selector_compiled_impl<Selector>(
    node,
    context,
    std::make_index_sequence<compiled_selector_v<Selector>.group_count>{});
}

/**
 * @brief 解析済み selector を使って最初の一致ノードを探します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline query_selector_impl(lxb_dom_node_t* node, selector_spec<Max> const& selector) -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  auto context = create_query_context(selector);
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector<Max>(current, selector, context)) {
      return current;
    }
  }
  return nullptr;
}

template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_impl(lxb_dom_node_t* node) -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  auto context = create_query_context(compiled_selector_v<Selector>);
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current, context)) {
      return current;
    }
  }
  return nullptr;
}

/**
 * @brief 解析済み selector を使って一致ノードを全件収集します。
 */
template <std::size_t Max>
[[nodiscard]] auto inline query_selector_all_impl(lxb_dom_node_t* node, selector_spec<Max> const& selector) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr) {
    return result;
  }

  auto context = create_query_context(selector);
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector<Max>(current, selector, context)) {
      result.push_back(current);
    }
  }
  return result;
}

template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all_impl(lxb_dom_node_t* node) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr) {
    return result;
  }

  auto context = create_query_context(compiled_selector_v<Selector>);
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current, context)) {
      result.push_back(current);
    }
  }
  return result;
}
}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_MATCH_HPP_
