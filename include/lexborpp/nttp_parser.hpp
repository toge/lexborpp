#ifndef LEXBORPP_NTTP_PARSER_HPP_
#define LEXBORPP_NTTP_PARSER_HPP_

#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"

// is_non_element_node 等 core.hpp の DOM ヘルパを使用するため、
// 単独 include でも完結するように取り込む。
#include "lexborpp/core.hpp"

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
// Tag id lookup for type selector optimization (case-insensitive) — ponytail: 1 integer compare beats iequals
constexpr std::array<std::pair<std::string_view, lxb_tag_id_t>, 196> kTagLookup{{
    {"undef", LXB_TAG__UNDEF},
    {"end_of_file", LXB_TAG__END_OF_FILE},
    {"text", LXB_TAG__TEXT},
    {"document", LXB_TAG__DOCUMENT},
    {"em_comment", LXB_TAG__EM_COMMENT},
    {"em_doctype", LXB_TAG__EM_DOCTYPE},
    {"a", LXB_TAG_A},
    {"abbr", LXB_TAG_ABBR},
    {"acronym", LXB_TAG_ACRONYM},
    {"address", LXB_TAG_ADDRESS},
    {"altglyph", LXB_TAG_ALTGLYPH},
    {"altglyphdef", LXB_TAG_ALTGLYPHDEF},
    {"altglyphitem", LXB_TAG_ALTGLYPHITEM},
    {"animatecolor", LXB_TAG_ANIMATECOLOR},
    {"animatemotion", LXB_TAG_ANIMATEMOTION},
    {"animatetransform", LXB_TAG_ANIMATETRANSFORM},
    {"annotation_xml", LXB_TAG_ANNOTATION_XML},
    {"applet", LXB_TAG_APPLET},
    {"area", LXB_TAG_AREA},
    {"article", LXB_TAG_ARTICLE},
    {"aside", LXB_TAG_ASIDE},
    {"audio", LXB_TAG_AUDIO},
    {"b", LXB_TAG_B},
    {"base", LXB_TAG_BASE},
    {"basefont", LXB_TAG_BASEFONT},
    {"bdi", LXB_TAG_BDI},
    {"bdo", LXB_TAG_BDO},
    {"bgsound", LXB_TAG_BGSOUND},
    {"big", LXB_TAG_BIG},
    {"blink", LXB_TAG_BLINK},
    {"blockquote", LXB_TAG_BLOCKQUOTE},
    {"body", LXB_TAG_BODY},
    {"br", LXB_TAG_BR},
    {"button", LXB_TAG_BUTTON},
    {"canvas", LXB_TAG_CANVAS},
    {"caption", LXB_TAG_CAPTION},
    {"center", LXB_TAG_CENTER},
    {"cite", LXB_TAG_CITE},
    {"clippath", LXB_TAG_CLIPPATH},
    {"code", LXB_TAG_CODE},
    {"col", LXB_TAG_COL},
    {"colgroup", LXB_TAG_COLGROUP},
    {"data", LXB_TAG_DATA},
    {"datalist", LXB_TAG_DATALIST},
    {"dd", LXB_TAG_DD},
    {"del", LXB_TAG_DEL},
    {"desc", LXB_TAG_DESC},
    {"details", LXB_TAG_DETAILS},
    {"dfn", LXB_TAG_DFN},
    {"dialog", LXB_TAG_DIALOG},
    {"dir", LXB_TAG_DIR},
    {"div", LXB_TAG_DIV},
    {"dl", LXB_TAG_DL},
    {"dt", LXB_TAG_DT},
    {"em", LXB_TAG_EM},
    {"embed", LXB_TAG_EMBED},
    {"feblend", LXB_TAG_FEBLEND},
    {"fecolormatrix", LXB_TAG_FECOLORMATRIX},
    {"fecomponenttransfer", LXB_TAG_FECOMPONENTTRANSFER},
    {"fecomposite", LXB_TAG_FECOMPOSITE},
    {"feconvolvematrix", LXB_TAG_FECONVOLVEMATRIX},
    {"fediffuselighting", LXB_TAG_FEDIFFUSELIGHTING},
    {"fedisplacementmap", LXB_TAG_FEDISPLACEMENTMAP},
    {"fedistantlight", LXB_TAG_FEDISTANTLIGHT},
    {"fedropshadow", LXB_TAG_FEDROPSHADOW},
    {"feflood", LXB_TAG_FEFLOOD},
    {"fefunca", LXB_TAG_FEFUNCA},
    {"fefuncb", LXB_TAG_FEFUNCB},
    {"fefuncg", LXB_TAG_FEFUNCG},
    {"fefuncr", LXB_TAG_FEFUNCR},
    {"fegaussianblur", LXB_TAG_FEGAUSSIANBLUR},
    {"feimage", LXB_TAG_FEIMAGE},
    {"femerge", LXB_TAG_FEMERGE},
    {"femergenode", LXB_TAG_FEMERGENODE},
    {"femorphology", LXB_TAG_FEMORPHOLOGY},
    {"feoffset", LXB_TAG_FEOFFSET},
    {"fepointlight", LXB_TAG_FEPOINTLIGHT},
    {"fespecularlighting", LXB_TAG_FESPECULARLIGHTING},
    {"fespotlight", LXB_TAG_FESPOTLIGHT},
    {"fetile", LXB_TAG_FETILE},
    {"feturbulence", LXB_TAG_FETURBULENCE},
    {"fieldset", LXB_TAG_FIELDSET},
    {"figcaption", LXB_TAG_FIGCAPTION},
    {"figure", LXB_TAG_FIGURE},
    {"font", LXB_TAG_FONT},
    {"footer", LXB_TAG_FOOTER},
    {"foreignobject", LXB_TAG_FOREIGNOBJECT},
    {"form", LXB_TAG_FORM},
    {"frame", LXB_TAG_FRAME},
    {"frameset", LXB_TAG_FRAMESET},
    {"glyphref", LXB_TAG_GLYPHREF},
    {"h1", LXB_TAG_H1},
    {"h2", LXB_TAG_H2},
    {"h3", LXB_TAG_H3},
    {"h4", LXB_TAG_H4},
    {"h5", LXB_TAG_H5},
    {"h6", LXB_TAG_H6},
    {"head", LXB_TAG_HEAD},
    {"header", LXB_TAG_HEADER},
    {"hgroup", LXB_TAG_HGROUP},
    {"hr", LXB_TAG_HR},
    {"html", LXB_TAG_HTML},
    {"i", LXB_TAG_I},
    {"iframe", LXB_TAG_IFRAME},
    {"image", LXB_TAG_IMAGE},
    {"img", LXB_TAG_IMG},
    {"input", LXB_TAG_INPUT},
    {"ins", LXB_TAG_INS},
    {"isindex", LXB_TAG_ISINDEX},
    {"kbd", LXB_TAG_KBD},
    {"keygen", LXB_TAG_KEYGEN},
    {"label", LXB_TAG_LABEL},
    {"legend", LXB_TAG_LEGEND},
    {"li", LXB_TAG_LI},
    {"lineargradient", LXB_TAG_LINEARGRADIENT},
    {"link", LXB_TAG_LINK},
    {"listing", LXB_TAG_LISTING},
    {"main", LXB_TAG_MAIN},
    {"malignmark", LXB_TAG_MALIGNMARK},
    {"map", LXB_TAG_MAP},
    {"mark", LXB_TAG_MARK},
    {"marquee", LXB_TAG_MARQUEE},
    {"math", LXB_TAG_MATH},
    {"menu", LXB_TAG_MENU},
    {"meta", LXB_TAG_META},
    {"meter", LXB_TAG_METER},
    {"mfenced", LXB_TAG_MFENCED},
    {"mglyph", LXB_TAG_MGLYPH},
    {"mi", LXB_TAG_MI},
    {"mn", LXB_TAG_MN},
    {"mo", LXB_TAG_MO},
    {"ms", LXB_TAG_MS},
    {"mtext", LXB_TAG_MTEXT},
    {"multicol", LXB_TAG_MULTICOL},
    {"nav", LXB_TAG_NAV},
    {"nextid", LXB_TAG_NEXTID},
    {"nobr", LXB_TAG_NOBR},
    {"noembed", LXB_TAG_NOEMBED},
    {"noframes", LXB_TAG_NOFRAMES},
    {"noscript", LXB_TAG_NOSCRIPT},
    {"object", LXB_TAG_OBJECT},
    {"ol", LXB_TAG_OL},
    {"optgroup", LXB_TAG_OPTGROUP},
    {"option", LXB_TAG_OPTION},
    {"output", LXB_TAG_OUTPUT},
    {"p", LXB_TAG_P},
    {"param", LXB_TAG_PARAM},
    {"path", LXB_TAG_PATH},
    {"picture", LXB_TAG_PICTURE},
    {"plaintext", LXB_TAG_PLAINTEXT},
    {"pre", LXB_TAG_PRE},
    {"progress", LXB_TAG_PROGRESS},
    {"q", LXB_TAG_Q},
    {"radialgradient", LXB_TAG_RADIALGRADIENT},
    {"rb", LXB_TAG_RB},
    {"rp", LXB_TAG_RP},
    {"rt", LXB_TAG_RT},
    {"rtc", LXB_TAG_RTC},
    {"ruby", LXB_TAG_RUBY},
    {"s", LXB_TAG_S},
    {"samp", LXB_TAG_SAMP},
    {"script", LXB_TAG_SCRIPT},
    {"section", LXB_TAG_SECTION},
    {"select", LXB_TAG_SELECT},
    {"slot", LXB_TAG_SLOT},
    {"small", LXB_TAG_SMALL},
    {"source", LXB_TAG_SOURCE},
    {"spacer", LXB_TAG_SPACER},
    {"span", LXB_TAG_SPAN},
    {"strike", LXB_TAG_STRIKE},
    {"strong", LXB_TAG_STRONG},
    {"style", LXB_TAG_STYLE},
    {"sub", LXB_TAG_SUB},
    {"summary", LXB_TAG_SUMMARY},
    {"sup", LXB_TAG_SUP},
    {"svg", LXB_TAG_SVG},
    {"table", LXB_TAG_TABLE},
    {"tbody", LXB_TAG_TBODY},
    {"td", LXB_TAG_TD},
    {"template", LXB_TAG_TEMPLATE},
    {"textarea", LXB_TAG_TEXTAREA},
    {"textpath", LXB_TAG_TEXTPATH},
    {"tfoot", LXB_TAG_TFOOT},
    {"th", LXB_TAG_TH},
    {"thead", LXB_TAG_THEAD},
    {"time", LXB_TAG_TIME},
    {"title", LXB_TAG_TITLE},
    {"tr", LXB_TAG_TR},
    {"track", LXB_TAG_TRACK},
    {"tt", LXB_TAG_TT},
    {"u", LXB_TAG_U},
    {"ul", LXB_TAG_UL},
    {"var", LXB_TAG_VAR},
    {"video", LXB_TAG_VIDEO},
    {"wbr", LXB_TAG_WBR},
    {"xmp", LXB_TAG_XMP}
}};

[[nodiscard]] constexpr auto lookup_tag_id(std::string_view name) noexcept -> lxb_tag_id_t {
  for (auto const& p : kTagLookup) {
    if (iequals(p.first, name)) return p.second;
  }
  return LXB_TAG__UNDEF;
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
 *
 * 空文字列（`[attr=""]`）も有効な値として許容します。
 * 未終端の引用符は不正な selector とみなします。
 */
constexpr auto parse_quoted_value(std::string_view input, std::size_t& pos) -> std::optional<std::string_view> {
  auto const quote = input[pos];
  ++pos;
  auto const begin = pos;
  while (pos < input.size() && input[pos] != quote) {
    ++pos;
  }
  if (pos >= input.size()) {
    return std::nullopt;  // unterminated quote
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
  lxb_tag_id_t tag_id{LXB_TAG__UNDEF}; // for type kind: resolved tag id, else UNDEF
};

/**
 * @brief フラット配列中の compound selector の範囲を保持します。
 */
struct selector_compound_info {
  std::size_t simple_start{};
  std::size_t simple_count{};
  selector_combinator relation{selector_combinator::descendant};
};

/**
 * @brief フラット配列中の selector group の範囲を保持します。
 */
struct selector_group_info {
  std::size_t compound_start{};
  std::size_t compound_count{};
};

/**
 * @brief selector 全体の解析結果をフラット配列で保持します。
 *
 * groups → compounds → simples の三段階をフラットにし、
 * 各 group/compound が自身の子要素の範囲を offset + count で参照します。
 * これにより O(N³) の compile-time メモリ消費を O(N) に抑えます。
 */
template <std::size_t Max>
struct selector_spec {
  std::array<selector_simple_spec, Max> simples{};
  std::size_t simple_count{};
  std::array<selector_compound_info, Max> compounds{};
  std::size_t compound_count{};
  std::array<selector_group_info, Max> groups{};
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
 * @brief `candidate` が `scope` 自身または `scope` の子孫かを判定します。
 */
[[nodiscard]] constexpr auto is_descendant_of(lxb_dom_node_t* candidate, lxb_dom_node_t* scope) noexcept -> bool {
  for (auto* n = candidate; n != nullptr; n = n->parent) {
    if (n == scope) {
      return true;
    }
  }
  return false;
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
    throw std::runtime_error{"NTTP CSS selector must not be empty"};
  }

  while (pos < input.size()) {
    if (result.group_count >= max) {
      throw std::runtime_error{"NTTP CSS selector is too complex"};
    }

    auto& group = result.groups[result.group_count];
    group.compound_start = result.compound_count;
    group.compound_count = 0;

    auto relation = selector_combinator::descendant;
    while (true) {
      if (result.compound_count >= max) {
        throw std::runtime_error{"NTTP CSS selector is too complex"};
      }

      auto& compound = result.compounds[result.compound_count];
      auto const simple_start = result.simple_count;
      compound.simple_start = simple_start;
      compound.relation = relation;

      parse_compound_elements<max>(input, pos, result);

      compound.simple_count = result.simple_count - simple_start;
      result.compound_count++;
      group.compound_count++;

      skip_spaces(input, pos);
      if (pos >= input.size()) {
        break;
      }

      if (input[pos] == ',') {
        ++pos;
        skip_spaces(input, pos);
        if (pos >= input.size()) {
          throw std::runtime_error{"NTTP CSS selector must not end with a comma"};
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

      throw std::runtime_error{"NTTP CSS selector token is invalid"};
    }

    if (group.compound_count == 0) {
      throw std::runtime_error{"NTTP CSS selector group must not be empty"};
    }

    result.group_count++;
  }

  return result;
}

/**
 * @brief compound selector 1 個分を解析して埋めます。
 */
template <std::size_t Max>
constexpr auto parse_simple_selector(
  std::string_view input,
  std::size_t& pos,
  selector_spec<Max>& result) -> void {
  auto append = [&](auto&& simple) {
    if (result.simple_count >= result.simples.size()) {
      throw std::runtime_error{"NTTP CSS selector is too complex"};
    }
    result.simples[result.simple_count++] = std::forward<decltype(simple)>(simple);
  };

  if (pos >= input.size()) {
    throw std::runtime_error{"NTTP CSS selector ended unexpectedly"};
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
      throw std::runtime_error{"NTTP CSS selector id must not be empty"};
    }
    append(selector_simple_spec{.kind = selector_simple_kind::id, .value = value});
    return;
  }

  if (input[pos] == '.') {
    ++pos;
    auto const value = parse_name(input, pos);
    if (value.empty()) {
      throw std::runtime_error{"NTTP CSS selector class must not be empty"};
    }
    append(selector_simple_spec{.kind = selector_simple_kind::class_name, .value = value});
    return;
  }

  if (input[pos] == '[') {
    ++pos;
    skip_spaces(input, pos);
    auto const name = parse_name(input, pos);
    if (name.empty()) {
      throw std::runtime_error{"NTTP CSS selector attribute name must not be empty"};
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
        throw std::runtime_error{"NTTP CSS selector attribute operator is unsupported"};
      }

      skip_spaces(input, pos);
      if (pos >= input.size()) {
        throw std::runtime_error{"NTTP CSS selector attribute value is missing"};
      }

      auto was_quoted = false;
      if (input[pos] == '"' || input[pos] == '\'') {
        was_quoted = true;
        auto const quoted = parse_quoted_value(input, pos);
        if (not quoted.has_value()) {
          throw std::runtime_error{"NTTP CSS selector attribute value is missing a closing quote"};
        }
        value = *quoted;
      } else {
        value = parse_name(input, pos);
      }

      // 空値は引用符付きの場合のみ有効 (`[attr=""]`)。`[attr=]` は不正。
      if (value.empty() && not was_quoted) {
        throw std::runtime_error{"NTTP CSS selector attribute value must not be empty"};
      }
      skip_spaces(input, pos);
    }

    if (pos >= input.size() || input[pos] != ']') {
      throw std::runtime_error{"NTTP CSS selector attribute selector must end with ']'"};
    }
    ++pos;
    append(selector_simple_spec{.kind = selector_simple_kind::attribute, .name = name, .value = value, .attribute_match = match});
    return;
  }

  if (input[pos] == ':') {
    throw std::runtime_error{"NTTP CSS selector pseudo-classes are not supported yet"};
  }

  auto const value = parse_name(input, pos);
  if (value.empty()) {
    throw std::runtime_error{"NTTP CSS selector token is invalid"};
  }
  append(selector_simple_spec{.kind = selector_simple_kind::type, .value = value, .tag_id = lookup_tag_id(value)});
}

/**
 * @brief compound selector 内の simple selector 列を解析します。
 *
 * 最初の !is_name_terminator または combinator/comma までを
 * 連続する simple selector として解析し、フラットな simples 配列に追加します。
 */
template <std::size_t Max>
constexpr auto parse_compound_elements(
  std::string_view input,
  std::size_t& pos,
  selector_spec<Max>& result) -> void {
  if (pos >= input.size()) {
    throw std::runtime_error{"NTTP CSS selector ended unexpectedly"};
  }

  parse_simple_selector<Max>(input, pos, result);

  while (pos < input.size()) {
    if (is_space(input[pos]) || input[pos] == ',' || input[pos] == '>' ||
        input[pos] == '+' || input[pos] == '~') {
      break;
    }

    if (not is_simple_selector_start(input[pos])) {
      throw std::runtime_error{"NTTP CSS selector token is invalid"};
    }

    parse_simple_selector<Max>(input, pos, result);
  }
}
}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_PARSER_HPP_
