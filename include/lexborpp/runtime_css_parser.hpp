#ifndef LEXBORPP_RUNTIME_CSS_PARSER_HPP_
#define LEXBORPP_RUNTIME_CSS_PARSER_HPP_

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "lexbor/dom/dom.h"
#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {
namespace detail {

// --- Owned spec types (std::string instead of std::string_view) ---

struct runtime_simple_spec {
  selector_simple_kind kind{selector_simple_kind::universal};
  std::string name{};
  std::string value{};
  selector_attribute_match attribute_match{selector_attribute_match::exists};
};

template <std::size_t Max>
struct runtime_compound_spec {
  std::array<runtime_simple_spec, Max> simples{};
  std::size_t simple_count{};
  selector_combinator relation{selector_combinator::descendant};
};

template <std::size_t Max>
struct runtime_group_spec {
  std::array<runtime_compound_spec<Max>, Max> compounds{};
  std::size_t compound_count{};
};

template <std::size_t Max>
struct runtime_selector_spec {
  std::array<runtime_group_spec<Max>, Max> groups{};
  std::size_t group_count{};
};

// --- Runtime parsing functions (non-template) ---

// Parse one simple selector into a compound
constexpr auto parse_runtime_simple_selector(
  std::string_view input,
  std::size_t& pos,
  auto& compound) -> void {
  auto append = [&]<typename T>(T&& simple) {
    if (compound.simple_count >= compound.simples.size()) {
      return; // too complex, stop
    }
    compound.simples[compound.simple_count++] = std::forward<T>(simple);
  };

  if (pos >= input.size()) {
    return; // ended unexpectedly
  }

  if (input[pos] == '*') {
    append(runtime_simple_spec{.kind = selector_simple_kind::universal});
    ++pos;
    return;
  }

  if (input[pos] == '#') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) {
      return; // id must not be empty
    }
    append(runtime_simple_spec{.kind = selector_simple_kind::id, .value = std::string(value)});
    return;
  }

  if (input[pos] == '.') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) {
      return; // class must not be empty
    }
    append(runtime_simple_spec{.kind = selector_simple_kind::class_name, .value = std::string(value)});
    return;
  }

  if (input[pos] == '[') {
    ++pos;
    skip_spaces(input, pos);
    auto const name = parse_name(input, pos);
    if (name.empty()) {
      return; // attribute name must not be empty
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
        return; // unsupported operator
      }

      skip_spaces(input, pos);
      if (pos >= input.size()) {
        return; // attribute value missing
      }

      if (input[pos] == '"' || input[pos] == '\'') {
        value = parse_quoted_value(input, pos);
      } else {
        value = parse_name(input, pos);
      }

      if (value.empty()) {
        return; // attribute value must not be empty
      }
      skip_spaces(input, pos);
    }

    if (pos >= input.size() || input[pos] != ']') {
      return; // attribute selector must end with ']'
    }
    ++pos;
    append(runtime_simple_spec{.kind = selector_simple_kind::attribute, .name = std::string(name), .value = std::string(value), .attribute_match = match});
    return;
  }

  if (input[pos] == ':') {
    return; // pseudo-classes not supported
  }

  auto const value = parse_name(input, pos);
  if (value.empty()) {
    return; // token invalid
  }
  append(runtime_simple_spec{.kind = selector_simple_kind::type, .value = std::string(value)});
}

// Parse one compound selector
constexpr auto parse_runtime_compound_selector(
  std::string_view input,
  std::size_t& pos,
  auto& group,
  selector_combinator relation) -> void {
  if (group.compound_count >= group.compounds.size()) {
    return; // too complex
  }

  auto& compound = group.compounds[group.compound_count++];
  compound.simple_count = 0;
  compound.relation = relation;

  if (pos >= input.size()) {
    return; // ended unexpectedly
  }

  parse_runtime_simple_selector(input, pos, compound);

  while (pos < input.size()) {
    if (is_space(input[pos]) || input[pos] == ',' || input[pos] == '>' ||
        input[pos] == '+' || input[pos] == '~') {
      break;
    }

    if (!is_simple_selector_start(input[pos])) {
      return; // token invalid
    }

    parse_runtime_simple_selector(input, pos, compound);
  }
}

// Top-level parser
template <std::size_t Max>
[[nodiscard]] constexpr auto parse_runtime_selector(
  std::string_view input) -> runtime_selector_spec<Max> {
  auto result = runtime_selector_spec<Max>{};
  auto pos = std::size_t{0};

  skip_spaces(input, pos);
  if (pos >= input.size()) {
    return result; // empty selector → empty spec
  }

  while (pos < input.size()) {
    if (result.group_count >= result.groups.size()) {
      break; // too complex, stop
    }

    auto& group = result.groups[result.group_count++];
    group.compound_count = 0;

    auto relation = selector_combinator::descendant;
    while (true) {
      parse_runtime_compound_selector(input, pos, group, relation);

      skip_spaces(input, pos);
      if (pos >= input.size()) break;

      if (input[pos] == ',') { ++pos; skip_spaces(input, pos); break; }
      if (input[pos] == '>') { relation = selector_combinator::child; ++pos; skip_spaces(input, pos); continue; }
      if (input[pos] == '+') { relation = selector_combinator::adjacent_sibling; ++pos; skip_spaces(input, pos); continue; }
      if (input[pos] == '~') { relation = selector_combinator::following_sibling; ++pos; skip_spaces(input, pos); continue; }
      if (is_simple_selector_start(input[pos])) { relation = selector_combinator::descendant; continue; }
      break; // invalid token, stop
    }

    if (group.compound_count == 0) break;
  }

  return result;
}

// Convenience: auto-determine Max from input size
[[nodiscard]] inline auto parse_runtime_selector_auto(
  std::string_view input) -> runtime_selector_spec<8> {
  return parse_runtime_selector<8>(input);
}

}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_RUNTIME_CSS_PARSER_HPP_