#ifndef LEXBORPP_NTTP_PARSER_HPP_
#define LEXBORPP_NTTP_PARSER_HPP_

#include <array>
#include <cstddef>
#include <ranges>
#include <string_view>

#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"

namespace lexborpp {

namespace detail {

/**
 * @brief 文字列を非型テンプレートパラメータとして渡すためのヘルパーです。
 *
 * C++20 以降、構造体は NTTP として使用できます。
 */
template <std::size_t N>
struct fixed_string {
  char data[N]{};

  /**
   * @brief 文字列リテラルから fixed_string を構築します。
   *
  * @param str 元となる文字列リテラルです。
   */
  constexpr fixed_string(char const (&str)[N]) noexcept {
    std::copy_n(str, N, data);
  }

  /**
   * @brief 末尾のヌル文字を除いた文字列ビューを返します。
   *
  * @return std::string_view 保持中の文字列ビューを返します。
   */
  constexpr auto view() const noexcept -> std::string_view { return {data, N - 1}; }

  /**
   * @brief ほかの fixed_string と内容が同じか比較します。
   *
   * @param rhs 比較対象です。
   * @return bool 内容が一致する場合に true を返します。
   */
  constexpr auto operator==(fixed_string const& rhs) const -> bool = default;
};

/**
 * @brief CSS セレクタ間の結合方法を表します。
 */
enum class selector_combinator {
  descendant,
  child,
  adjacent_sibling,
  following_sibling,
};

/**
 * @brief 単一 selector の種類を表します。
 */
enum class selector_simple_kind {
  universal,
  type,
  id,
  class_name,
  attribute,
};

/**
 * @brief 属性 selector の比較方法を表します。
 */
enum class selector_attribute_match {
  exists,
  equals,
  includes,
  dash,
  prefix,
  suffix,
  substring,
};

/**
 * @brief CSS 解析で使用する空白判定を行います。
 */
constexpr auto is_space(char const c) noexcept -> bool {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/**
 * @brief selector 名称の終端文字を判定します。
 */
constexpr auto is_name_terminator(char const c) noexcept -> bool {
  return c == '\0' || is_space(c) || c == ',' || c == '>' || c == '+' || c == '~' ||
         c == '[' || c == ']' || c == '#' || c == '.' || c == ':' || c == '(' || c == ')' ||
         c == '"' || c == '\'' || c == '=' || c == '|' || c == '^' || c == '$' || c == '*';
}

/**
 * @brief ASCII 文字を小文字化します。
 */
constexpr auto ascii_lower(char const c) noexcept -> char {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

/**
 * @brief ASCII 大文字小文字を無視して文字列を比較します。
 */
constexpr auto iequals(std::string_view lhs, std::string_view rhs) noexcept -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (auto const index : std::views::iota(std::size_t{0}, lhs.size())) {
    if (ascii_lower(lhs[index]) != ascii_lower(rhs[index])) {
      return false;
    }
  }
  return true;
}

/**
 * @brief 入力文字列の空白を読み飛ばします。
 */
constexpr auto skip_spaces(std::string_view input, std::size_t& pos) noexcept -> void {
  while (pos < input.size() && is_space(input[pos])) {
    ++pos;
  }
}

/**
 * @brief selector 名称として読める範囲を切り出します。
 */
constexpr auto parse_name(std::string_view input, std::size_t& pos) noexcept -> std::string_view {
  auto const begin = pos;
  while (pos < input.size() && !is_name_terminator(input[pos])) {
    ++pos;
  }
  return input.substr(begin, pos - begin);
}

/**
 * @brief 属性 selector の引用付き値を切り出します。
 */
constexpr auto parse_quoted_value(std::string_view input, std::size_t& pos) -> std::string_view {
  auto const quote = input[pos];
  ++pos;
  auto const begin = pos;
  while (pos < input.size() && input[pos] != quote) {
    ++pos;
  }
  if (pos >= input.size()) {
    return {};
  }
  auto const value = input.substr(begin, pos - begin);
  ++pos;
  return value;
}

/**
 * @brief 単一 selector の解析結果を保持します。
 */
struct selector_simple_spec {
  selector_simple_kind kind{selector_simple_kind::universal};
  std::string_view name{};
  std::string_view value{};
  selector_attribute_match attribute_match{selector_attribute_match::exists};
};

/**
 * @brief 連続する単一 selector をまとめた compound selector を保持します。
 */
template <std::size_t Max>
struct selector_compound_spec {
  std::array<selector_simple_spec, Max> simples{};
  std::size_t simple_count{};
  selector_combinator relation{selector_combinator::descendant};
};

/**
 * @brief カンマ区切りの selector group を保持します。
 */
template <std::size_t Max>
struct selector_group_spec {
  std::array<selector_compound_spec<Max>, Max> compounds{};
  std::size_t compound_count{};
};

/**
 * @brief selector 全体の解析結果を保持します。
 */
template <std::size_t Max>
struct selector_spec {
  std::array<selector_group_spec<Max>, Max> groups{};
  std::size_t group_count{};
};

/**
 * @brief 要素ノードの qualified name を文字列として取得します。
 */
[[nodiscard]] constexpr auto node_qualified_name(lxb_dom_node_t* node) noexcept -> std::string_view {
  if (node == nullptr || is_non_element_node(node)) {
    return {};
  }

  auto len = size_t{};
  auto* const data = lxb_dom_element_qualified_name(lxb_dom_interface_element(node), &len);
  if (data == nullptr) {
    return {};
  }

  return {reinterpret_cast<const char*>(data), len};
}

/**
 * @brief 直前の要素兄弟を返します。
 */
[[nodiscard]] constexpr auto prev_element_sibling(lxb_dom_node_t* node) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  for (auto* prev = lxb_dom_node_prev(node); prev != nullptr; prev = lxb_dom_node_prev(prev)) {
    if (not is_non_element_node(prev)) {
      return prev;
    }
  }
  return nullptr;
}

/**
 * @brief 最も近い親要素ノードを返します。
 *
 * @param node 対象ノードです。
 * @return lxb_dom_node_t* 親要素ノードを返します。存在しない場合は nullptr を返します。
 */
[[nodiscard]] constexpr auto parent_element(lxb_dom_node_t* node) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  auto* parent = node->parent;
  while (parent != nullptr && is_non_element_node(parent)) {
    parent = parent->parent;
  }
  return parent;
}

/**
 * @brief 属性 selector の条件を DOM 属性値に対して評価します。
 */
[[nodiscard]] constexpr auto match_attribute(
  std::string_view attr_value,
  std::string_view expected,
  selector_attribute_match match) noexcept -> bool {
  switch (match) {
  case selector_attribute_match::exists:
    return true;
  case selector_attribute_match::equals:
    return attr_value == expected;
  case selector_attribute_match::includes:
    for (auto start = attr_value.find_first_not_of(" \t\n\r\f"); start != std::string_view::npos; ) {
      auto const end = attr_value.find_first_of(" \t\n\r\f", start);
      auto const length = end == std::string_view::npos ? attr_value.size() - start : end - start;
      if (attr_value.substr(start, length) == expected) {
        return true;
      }
      start = attr_value.find_first_not_of(" \t\n\r\f", end);
    }
    return false;
  case selector_attribute_match::dash:
    return attr_value == expected || (attr_value.size() > expected.size() &&
      attr_value.starts_with(expected) && attr_value[expected.size()] == '-');
  case selector_attribute_match::prefix:
    return attr_value.starts_with(expected);
  case selector_attribute_match::suffix:
    return attr_value.ends_with(expected);
  case selector_attribute_match::substring:
    return attr_value.find(expected) != std::string_view::npos;
  }

  return false;
}

/**
 * @brief 1 つの simple selector が始まる位置かを判定します。
 */
[[nodiscard]] constexpr auto is_simple_selector_start(char const c) noexcept -> bool {
  return c == '*' || c == '#' || c == '.' || c == '[' || c == ':' ||
         (!is_name_terminator(c) && c != '>');
}

template <detail::fixed_string Selector, std::size_t Max>
constexpr auto parse_simple_selector(
  std::string_view input,
  std::size_t& pos,
  selector_compound_spec<Max>& compound) -> void;

template <detail::fixed_string Selector, std::size_t Max>
constexpr auto parse_compound_selector(
  std::string_view input,
  std::size_t& pos,
  selector_group_spec<Max>& group,
  selector_combinator relation) -> void;

/**
 * @brief CSS セレクタ文字列をコンパイル時に解析します。
 *
 * @tparam Selector 解析対象の selector 文字列です。
 * @return auto 解析済み selector 情報を返します。
 *
 * @note 無効なセレクタは compile error になります。エラーメッセージは
 *       throw の内容を直接表示せず "not a constant expression" になります。
 *       各 throw 文に書かれた文字列（"NTTP CSS selector must not be empty" など）を
 *       参考にデバッグしてください。
 */
template <detail::fixed_string Selector>
constexpr auto parse_selector_spec() {
  auto constexpr max = Selector.view().empty() ? std::size_t{1} : Selector.view().size();
  auto result = selector_spec<max>{};
  auto const input = Selector.view();
  auto pos = std::size_t{0};

  skip_spaces(input, pos);
  if (pos >= input.size()) {
    throw "NTTP CSS selector must not be empty";
  }

  while (pos < input.size()) {
    if (result.group_count >= result.groups.size()) {
      throw "NTTP CSS selector is too complex";
    }

    auto& group = result.groups[result.group_count++];
    group.compound_count = 0;

    auto relation = selector_combinator::descendant;
    while (true) {
      parse_compound_selector<Selector, max>(input, pos, group, relation);

      skip_spaces(input, pos);
      if (pos >= input.size()) {
        break;
      }

      if (input[pos] == ',') {
        ++pos;
        skip_spaces(input, pos);
        if (pos >= input.size()) {
          throw "NTTP CSS selector must not end with a comma";
        }
        break;
      }

      if (input[pos] == '>') {
        relation = selector_combinator::child;
        ++pos;
        skip_spaces(input, pos);
        continue;
      }

      if (input[pos] == '+') {
        relation = selector_combinator::adjacent_sibling;
        ++pos;
        skip_spaces(input, pos);
        continue;
      }

      if (input[pos] == '~') {
        relation = selector_combinator::following_sibling;
        ++pos;
        skip_spaces(input, pos);
        continue;
      }

      if (is_simple_selector_start(input[pos])) {
        relation = selector_combinator::descendant;
        continue;
      }

      throw "NTTP CSS selector token is invalid";
    }

    if (group.compound_count == 0) {
      throw "NTTP CSS selector group must not be empty";
    }
  }

  return result;
}

/**
 * @brief compound selector 1 個分を解析して埋めます。
 */
template <detail::fixed_string Selector, std::size_t Max>
constexpr auto parse_simple_selector(
  std::string_view input,
  std::size_t& pos,
  selector_compound_spec<Max>& compound) -> void {
  auto append = [&]<typename T>(T&& simple) {
    if (compound.simple_count >= compound.simples.size()) {
      throw "NTTP CSS selector is too complex";
    }
    compound.simples[compound.simple_count++] = std::forward<T>(simple);
  };

  if (pos >= input.size()) {
    throw "NTTP CSS selector ended unexpectedly";
  }

  if (input[pos] == '*') {
    append(selector_simple_spec{.kind = selector_simple_kind::universal});
    ++pos;
    return;
  }

  if (input[pos] == '#') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) {
      throw "NTTP CSS selector id must not be empty";
    }
    append(selector_simple_spec{.kind = selector_simple_kind::id, .value = value});
    return;
  }

  if (input[pos] == '.') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) {
      throw "NTTP CSS selector class must not be empty";
    }
    append(selector_simple_spec{.kind = selector_simple_kind::class_name, .value = value});
    return;
  }

  if (input[pos] == '[') {
    ++pos;
    skip_spaces(input, pos);
    auto const name = parse_name(input, pos);
    if (name.empty()) {
      throw "NTTP CSS selector attribute name must not be empty";
    }
    skip_spaces(input, pos);

    auto match = selector_attribute_match::exists;
    auto value = std::string_view{};
    if (pos < input.size() && input[pos] != ']') {
      if (input[pos] == '=' ) {
        match = selector_attribute_match::equals;
        ++pos;
      } else if (pos + 1 < input.size() && input[pos + 1] == '=' && input[pos] == '~') {
        match = selector_attribute_match::includes;
        pos += 2;
      } else if (pos + 1 < input.size() && input[pos + 1] == '=' && input[pos] == '|') {
        match = selector_attribute_match::dash;
        pos += 2;
      } else if (pos + 1 < input.size() && input[pos + 1] == '=' && input[pos] == '^') {
        match = selector_attribute_match::prefix;
        pos += 2;
      } else if (pos + 1 < input.size() && input[pos + 1] == '=' && input[pos] == '$') {
        match = selector_attribute_match::suffix;
        pos += 2;
      } else if (pos + 1 < input.size() && input[pos + 1] == '=' && input[pos] == '*') {
        match = selector_attribute_match::substring;
        pos += 2;
      } else {
        throw "NTTP CSS selector attribute operator is unsupported";
      }

      skip_spaces(input, pos);
      if (pos >= input.size()) {
        throw "NTTP CSS selector attribute value is missing";
      }

      if (input[pos] == '"' || input[pos] == '\'') {
        value = parse_quoted_value(input, pos);
      } else {
        value = parse_name(input, pos);
      }

      if (value.empty()) {
        throw "NTTP CSS selector attribute value must not be empty";
      }
      skip_spaces(input, pos);
    }

    if (pos >= input.size() || input[pos] != ']') {
      throw "NTTP CSS selector attribute selector must end with ']'";
    }
    ++pos;
    append(selector_simple_spec{.kind = selector_simple_kind::attribute, .name = name, .value = value, .attribute_match = match});
    return;
  }

  if (input[pos] == ':') {
    throw "NTTP CSS selector pseudo-classes are not supported yet";
  }

  auto const value = parse_name(input, pos);
  if (value.empty()) {
    throw "NTTP CSS selector token is invalid";
  }
  append(selector_simple_spec{.kind = selector_simple_kind::type, .value = value});
}

/**
 * @brief compound selector の列を解析して group に追加します。
 */
template <detail::fixed_string Selector, std::size_t Max>
constexpr auto parse_compound_selector(
  std::string_view input,
  std::size_t& pos,
  selector_group_spec<Max>& group,
  selector_combinator relation) -> void {
  if (group.compound_count >= group.compounds.size()) {
    throw "NTTP CSS selector is too complex";
  }

  auto& compound = group.compounds[group.compound_count++];
  compound.simple_count = 0;
  compound.relation = relation;

  if (pos >= input.size()) {
    throw "NTTP CSS selector ended unexpectedly";
  }

  parse_simple_selector<Selector, Max>(input, pos, compound);

  while (pos < input.size()) {
    if (is_space(input[pos]) || input[pos] == ',' || input[pos] == '>' ||
        input[pos] == '+' || input[pos] == '~') {
      break;
    }

    if (not is_simple_selector_start(input[pos])) {
      throw "NTTP CSS selector token is invalid";
    }

    parse_simple_selector<Selector, Max>(input, pos, compound);
  }
}
}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_PARSER_HPP_
