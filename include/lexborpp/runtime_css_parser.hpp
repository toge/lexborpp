#ifndef LEXBORPP_RUNTIME_CSS_PARSER_HPP_
#define LEXBORPP_RUNTIME_CSS_PARSER_HPP_

#include <array>
#include <cstddef>
#include <expected>
#include <string_view>
#include <system_error>

#include "lexbor/dom/dom.h"
#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {
namespace detail {

// --- Flat spec reuse (same as NTTP) ---
// Runtime now reuses the flat selector_spec<Max> from nttp_parser.hpp.
// Old triple-nested array<array<array<...>>> (137KB) is gone -> ~3KB for Max=32.
template <std::size_t Max>
using runtime_selector_spec = selector_spec<Max>;

// Keep legacy aliases for external code that may have used detail::runtime_simple_spec
using runtime_simple_spec = selector_simple_spec;
template <std::size_t Max>
using runtime_compound_spec = selector_compound_info;
template <std::size_t Max>
using runtime_group_spec = selector_group_info;

// --- Runtime parsing (bool-based, no throw) ---

template <std::size_t Max>
constexpr auto parse_runtime_simple_selector(
  std::string_view input,
  std::size_t& pos,
  selector_spec<Max>& result) noexcept -> bool {
  auto append = [&](auto&& simple) -> bool {
    if (result.simple_count >= result.simples.size()) return false;
    result.simples[result.simple_count++] = std::forward<decltype(simple)>(simple);
    return true;
  };
  if (pos >= input.size()) return false;
  if (input[pos] == '*') {
    ++pos;
    return append(selector_simple_spec{.kind = selector_simple_kind::universal});
  }
  if (input[pos] == '#') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) return false;
    return append(selector_simple_spec{.kind = selector_simple_kind::id, .value = value});
  }
  if (input[pos] == '.') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) return false;
    return append(selector_simple_spec{.kind = selector_simple_kind::class_name, .value = value});
  }
  if (input[pos] == '[') {
    ++pos;
    skip_spaces(input, pos);
    auto const name = parse_name(input, pos);
    if (name.empty()) return false;
    skip_spaces(input, pos);
    auto match = selector_attribute_match::exists;
    auto value = std::string_view{};
    if (pos < input.size() && input[pos] != ']') {
      if (input[pos] == '=') { match = selector_attribute_match::equals; ++pos; }
      else if (pos + 1 < input.size() && input[pos+1] == '=' && input[pos] == '~') { match = selector_attribute_match::includes; pos+=2; }
      else if (pos + 1 < input.size() && input[pos+1] == '=' && input[pos] == '|') { match = selector_attribute_match::dash; pos+=2; }
      else if (pos + 1 < input.size() && input[pos+1] == '=' && input[pos] == '^') { match = selector_attribute_match::prefix; pos+=2; }
      else if (pos + 1 < input.size() && input[pos+1] == '=' && input[pos] == '$') { match = selector_attribute_match::suffix; pos+=2; }
      else if (pos + 1 < input.size() && input[pos+1] == '=' && input[pos] == '*') { match = selector_attribute_match::substring; pos+=2; }
      else return false;
      skip_spaces(input, pos);
      if (pos >= input.size()) return false;
      auto was_quoted = false;
      if (input[pos] == '"' || input[pos] == '\'') {
        was_quoted = true;
        auto const quoted = parse_quoted_value(input, pos);
        if (!quoted.has_value()) return false;
        value = *quoted;
      } else {
        value = parse_name(input, pos);
      }
      if (value.empty() && !was_quoted) return false;
      skip_spaces(input, pos);
    }
    if (pos >= input.size() || input[pos] != ']') return false;
    ++pos;
    return append(selector_simple_spec{.kind = selector_simple_kind::attribute, .name = name, .value = value, .attribute_match = match});
  }
  if (input[pos] == ':') {
    ++pos;
    parse_name(input, pos);
    if (pos < input.size() && input[pos] == '(') {
      ++pos;
      for (auto depth = 1; pos < input.size() && depth > 0; ++pos) {
        if (input[pos] == '(') ++depth;
        else if (input[pos] == ')') --depth;
      }
    }
    return false;
  }
  auto const value = parse_name(input, pos);
  if (value.empty()) return false;
  return append(selector_simple_spec{.kind = selector_simple_kind::type, .value = value, .tag_id = lookup_tag_id(value)});
}

template <std::size_t Max>
constexpr auto parse_runtime_compound_elements(
  std::string_view input,
  std::size_t& pos,
  selector_spec<Max>& result) noexcept -> bool {
  if (pos >= input.size()) return false;
  if (!parse_runtime_simple_selector<Max>(input, pos, result)) return false;
  while (pos < input.size()) {
    if (is_space(input[pos]) || input[pos] == ',' || input[pos] == '>' ||
        input[pos] == '+' || input[pos] == '~') break;
    if (!is_simple_selector_start(input[pos])) return false;
    if (!parse_runtime_simple_selector<Max>(input, pos, result)) return false;
  }
  return true;
}

template <std::size_t Max>
[[nodiscard]] constexpr auto parse_runtime_selector(
  std::string_view input) noexcept -> std::expected<selector_spec<Max>, std::errc> {
  auto result = selector_spec<Max>{};
  auto pos = std::size_t{0};
  skip_spaces(input, pos);
  if (pos >= input.size()) return std::unexpected(std::errc::invalid_argument);
  while (pos < input.size()) {
    if (result.group_count >= result.groups.size()) {
      return std::unexpected(std::errc::invalid_argument);
    }
    auto& group = result.groups[result.group_count];
    group.compound_start = result.compound_count;
    group.compound_count = 0;
    auto relation = selector_combinator::descendant;
    while (true) {
      if (result.compound_count >= result.compounds.size()) {
        return std::unexpected(std::errc::invalid_argument);
      }
      auto& compound = result.compounds[result.compound_count];
      auto const simple_start = result.simple_count;
      compound.simple_start = simple_start;
      compound.relation = relation;
      if (!parse_runtime_compound_elements<Max>(input, pos, result)) {
        return std::unexpected(std::errc::invalid_argument);
      }
      compound.simple_count = result.simple_count - simple_start;
      ++result.compound_count;
      ++group.compound_count;
      skip_spaces(input, pos);
      if (pos >= input.size()) break;
      if (input[pos] == ',') { ++pos; skip_spaces(input, pos); break; }
      if (input[pos] == '>') { relation = selector_combinator::child; ++pos; skip_spaces(input, pos); continue; }
      if (input[pos] == '+') { relation = selector_combinator::adjacent_sibling; ++pos; skip_spaces(input, pos); continue; }
      if (input[pos] == '~') { relation = selector_combinator::following_sibling; ++pos; skip_spaces(input, pos); continue; }
      if (is_simple_selector_start(input[pos])) { relation = selector_combinator::descendant; continue; }
      break;
    }
    if (group.compound_count == 0) break;
    ++result.group_count;
  }
  return result;
}

// Convenience; Max=32 gives ~3KB stack, Max=12 was 137KB via triple nesting.
// セレクタ文字列の長さは最大 256 文字です。超過した場合はエラーを返します。
[[nodiscard]] inline auto parse_runtime_selector_auto(
  std::string_view input) noexcept -> std::expected<selector_spec<32>, std::errc> {
  if (input.size() > 256) return std::unexpected(std::errc::invalid_argument);
  return parse_runtime_selector<32>(input);
}

// Legacy alias for code that used runtime_selector_spec<12>
template <std::size_t Max>
constexpr auto parse_runtime_selector_legacy(std::string_view input) noexcept -> runtime_selector_spec<Max> {
  auto result = parse_runtime_selector<Max>(input);
  if (!result) return {};
  return *result;
}

}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_RUNTIME_CSS_PARSER_HPP_
