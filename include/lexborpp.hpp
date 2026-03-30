#ifndef __LEXBORPP_HPP__
#define __LEXBORPP_HPP__

#include <initializer_list>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>

#include "lexbor/html/parser.h"
#include "lexbor/dom/dom.h"

namespace lexborpp {

auto inline is_non_element_node(lxb_dom_node_t* node) noexcept -> bool {
  if (node == nullptr) {
    return true;
  }
  auto const tag_id = lxb_dom_node_tag_id(node);
  return tag_id == LXB_TAG__UNDEF || tag_id == LXB_TAG__TEXT || tag_id == LXB_TAG__DOCUMENT ||
         tag_id == LXB_TAG__EM_COMMENT || tag_id == LXB_TAG__EM_DOCTYPE;
}

/**
 * @brief 指定したIDのelementを取得する。
 *
 * @param node これ以下のノードを検索対象とする
 * @param id_name 検索対象のid名
 * @return auto 複数IDがある場合は最初のものを、ない場合はnullptrを返す
 */
auto inline get_element_by_id(lxb_dom_node_t* node, std::string_view id_name) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  auto const text_walker = +[](lxb_dom_node_t* node, void* ctx) noexcept {
    if (is_non_element_node(node)) {
      return LEXBOR_ACTION_OK;
    }

    auto& [target_id, result] = *reinterpret_cast<std::pair<std::string_view, lxb_dom_node*>*>(ctx);
    auto const element        = lxb_dom_interface_element(node);
    auto       attr_len       = size_t{};
    auto const attr           = lxb_dom_element_get_attribute(element, (const lxb_char_t*)"id", 2, &attr_len);
    if (attr != nullptr and target_id == std::string_view{reinterpret_cast<const char*>(attr), attr_len}) {
      result = node;
      return LEXBOR_ACTION_STOP;
    }
    return LEXBOR_ACTION_OK;
  };

  auto target = std::pair<std::string_view const, lxb_dom_node*>{id_name, nullptr};
  lxb_dom_node_simple_walk(node, text_walker, &target);
  return target.second;
}

/**
 * @brief class名が一致する最初のelementを取得する
 */
auto inline get_first_element_by_class(lxb_dom_node_t* node, std::string_view class_name) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }

  auto const text_walker = +[](lxb_dom_node_t* node, void* ctx) noexcept {
    auto& [target, result] = *reinterpret_cast<std::pair<std::string_view, lxb_dom_node*>*>(ctx);
    if (is_non_element_node(node)) {
      return LEXBOR_ACTION_OK;
    }

    auto attr_len = size_t{};
    auto attr     = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), (const lxb_char_t*)"class", 5, &attr_len);
    if (attr != nullptr and target == std::string_view{reinterpret_cast<const char*>(attr), attr_len}) {
      result = node;
      return LEXBOR_ACTION_STOP;
    }
    return LEXBOR_ACTION_OK;
  };

  auto target = std::pair<std::string_view, lxb_dom_node*>{class_name, nullptr};
  lxb_dom_node_simple_walk(node, text_walker, &target);
  return target.second;
}

/**
 * @brief 指定nodeから指定attributeの値を取得する
 *
 * @param node 属性値を取得するノード
 * @param attr_name 取得対象のattribute名
 * @return auto 指定attributeの値、なければ空文字列
 */
auto inline get_attr_value(lxb_dom_node_t* node, std::string_view attr_name) noexcept -> std::optional<std::string_view> {
  if (node == nullptr) {
    return std::nullopt;
  }
  if (is_non_element_node(node)) {
    return std::nullopt;
  }

  for (auto p = lxb_dom_element_first_attribute(lxb_dom_interface_element(node)); p != nullptr; p = lxb_dom_element_next_attribute(p)) {
    auto       attr_name_len  = size_t{};
    auto const attr_name_data = lxb_dom_attr_qualified_name(p, &attr_name_len);
    if (attr_name_data == nullptr or std::string_view{reinterpret_cast<const char*>(attr_name_data), attr_name_len} != attr_name) {
      continue;
    }

    auto attr_val_len  = size_t{};
    auto attr_val_data = lxb_dom_attr_value(p, &attr_val_len);
    if (attr_val_data != nullptr) {
      return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
    }
  }
  return std::nullopt;
}

/**
 * @brief ノードのclass属性に指定したクラス名が含まれているかを判定する
 *
 * @param node 判定対象のノード
 * @param class_name 判定するクラス名
 * @return bool class属性にclass_nameが含まれている場合true、それ以外false
 */
auto inline has_class(lxb_dom_node_t* node, std::string_view class_name) noexcept -> bool {
  if (class_name.empty()) {
    return false;
  }
  auto const class_attr = get_attr_value(node, "class");
  if (not class_attr.has_value()) {
    return false;
  }

  constexpr auto kWhitespace = std::string_view{" \t\n\r\f"};
  auto sv = *class_attr;
  while (not sv.empty()) {
    auto const start = sv.find_first_not_of(kWhitespace);
    if (start == std::string_view::npos) {
      break;
    }
    sv.remove_prefix(start);

    auto const end = sv.find_first_of(kWhitespace);
    if (sv.substr(0, end) == class_name) {
      return true;
    }
    if (end == std::string_view::npos) {
      break;
    }
    sv.remove_prefix(end);
  }
  return false;
}

/**
 * @brief ノードのclass属性に指定した複数のクラス名がすべて含まれているかを判定する
 *
 * @param node 判定対象のノード
 * @param class_names 判定するクラス名のリスト
 * @return bool class属性にclass_namesの全てが含まれている場合true、それ以外false
 */
auto inline has_class(lxb_dom_node_t* node, std::initializer_list<std::string_view> class_names) noexcept -> bool {
  if (class_names.size() == 0) {
    return false;
  }
  for (auto const& name : class_names) {
    if (not has_class(node, name)) {
      return false;
    }
  }
  return true;
}

/**
 * @brief 指定nodeの最初の子要素のテキストを取得する
 */
auto inline get_first_child_text(lxb_dom_node_t* node) noexcept -> std::optional<std::string_view> {
  if (node == nullptr) {
    return std::nullopt;
  }

  for (node = lxb_dom_node_first_child(node); node != nullptr; node = lxb_dom_node_next(node)) {
    if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
      auto const data = lxb_dom_interface_character_data(node);
      return std::string_view{reinterpret_cast<const char*>(data->data.data), data->data.length};
    }
  }
  return std::nullopt;
}

/**
 * @brief 指定nodeの子要素のテキストを全て取得する
 */
auto inline get_all_children_text(lxb_dom_node_t* node) noexcept -> std::optional<std::string> {
  if (node == nullptr) {
    return std::nullopt;
  }

  auto result    = std::string{};
  auto find_text = false;
  for (node = lxb_dom_node_first_child(node); node != nullptr; node = lxb_dom_node_next(node)) {
    if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
      auto const data = lxb_dom_interface_character_data(node);
      find_text = true;
      result.append(reinterpret_cast<const char*>(data->data.data), data->data.length);
    }
  }
  if (not find_text) {
    return std::nullopt;
  }
  return result;
}

/**
 * @brief 指定nodeの子要素のテキストを全て取得する
 */
auto inline get_all_children_text(lxb_dom_node_t* node, std::string_view const sep) noexcept -> std::optional<std::string> {
  if (node == nullptr) {
    return std::nullopt;
  }

  auto result    = std::string{};
  auto find_text = false;
  for (node = lxb_dom_node_first_child(node); node != nullptr; node = lxb_dom_node_next(node)) {
    if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
      auto const data = lxb_dom_interface_character_data(node);
      if (find_text) {
        result.append(sep);
      }
      find_text = true;
      result.append(reinterpret_cast<const char*>(data->data.data), data->data.length);
    }
  }
  if (not find_text) {
    return std::nullopt;
  }
  return result;
}

template <typename Op>
auto inline get_following_element_by_op(lxb_dom_node_t* node, Op op) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }
  auto const text_walker = +[](lxb_dom_node_t* node, void* ctx) noexcept {
    auto& [op, result] = *reinterpret_cast<std::pair<Op&, lxb_dom_node*>*>(ctx);
    if (op(node)) {
      result = node;
      return LEXBOR_ACTION_STOP;
    }
    return LEXBOR_ACTION_OK;
  };

  auto target = std::pair<Op&, lxb_dom_node*>{op, nullptr};
  lxb_dom_node_simple_walk(node, text_walker, &target);
  return target.second;
}

template <typename Op>
auto inline get_sibling_element_by_op(lxb_dom_node_t* node, Op op) noexcept -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }
  for (; node != nullptr; node = lxb_dom_node_next(node)) {
    if (op(node)) {
      return node;
    }
  }
  return nullptr;
}

auto inline to_string(lxb_dom_node_t* node) noexcept {
  if (node == nullptr || node->type != LXB_DOM_NODE_TYPE_TEXT) {
    return std::string_view{};
  }
  auto const data = lxb_dom_interface_character_data(node);
  return std::string_view{reinterpret_cast<const char*>(data->data.data), data->data.length};
}

auto inline to_name_string(lxb_dom_attr_t* attr) noexcept {
  if (attr == nullptr) {
    return std::string_view{};
  }
  auto attr_val_len  = size_t{};
  auto attr_val_data = lxb_dom_attr_qualified_name(attr, &attr_val_len);
  if (attr_val_data == nullptr) {
    return std::string_view{};
  }
  return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
}

auto inline to_string(lxb_dom_attr_t* attr) noexcept {
  if (attr == nullptr) {
    return std::string_view{};
  }
  auto attr_val_len  = size_t{};
  auto attr_val_data = lxb_dom_attr_value(attr, &attr_val_len);
  if (attr_val_data == nullptr) {
    return std::string_view{};
  }
  return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
}

class node_walker : public std::ranges::view_interface<node_walker> {
public:
  using value_type = lxb_dom_node_t*;

  explicit node_walker(lxb_dom_node_t* node = nullptr) : start(node) { }

  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_node_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_node_t**;
    using reference = lxb_dom_node_t*&;

    iterator(lxb_dom_node_t* node = nullptr, lxb_dom_node_t* end = nullptr) : start(node), current(end) { }
    // iterator(const iterator&) = default;
    // iterator(iterator&&) = default;

    iterator& operator++() noexcept {
      if (current == start) {
        return *this;
      }
      if (current->first_child != nullptr) {
        current = current->first_child;
        return *this;
      }
      while(current != start && current->next == nullptr) {
        current = current->parent;
      }
      if (current != start) {
        current = current->next;
      }
      return *this;
    }

    // TODO: 最適化
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    auto operator==(iterator const &rhs) const noexcept { return start == rhs.start and current == rhs.current; }
    lxb_dom_node_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_node_t* start;
    lxb_dom_node_t* current;
  };


  [[nodiscard]] iterator begin() noexcept { return iterator{start, start ? start->first_child : nullptr}; }
  [[nodiscard]] iterator begin() const noexcept { return iterator{start, start ? start->first_child : nullptr}; }
  [[nodiscard]] iterator end() noexcept { return iterator{start, start}; }
  [[nodiscard]] iterator end() const noexcept { return iterator{start, start}; }

private:
  lxb_dom_node_t* start;
};

class node_sibling_walker : public std::ranges::view_interface<node_sibling_walker> {
public:
  using value_type = lxb_dom_node_t*;

  explicit node_sibling_walker(lxb_dom_node_t* node = nullptr) : start(node) { }

  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_node_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_node_t**;
    using reference = lxb_dom_node_t*&;

    iterator(lxb_dom_node_t* node = nullptr) : current(node) { }
    // iterator(const iterator&) = default;
    // iterator(iterator&&) = default;

    iterator& operator++() noexcept {
      if (current == nullptr) {
        return *this;
      }
      current = current->next;
      return *this;
    }

    // TODO: 最適化
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }
    lxb_dom_node_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_node_t* current;
  };


  [[nodiscard]] iterator begin() noexcept { return iterator{start}; }
  [[nodiscard]] iterator begin() const noexcept { return iterator{start}; }
  [[nodiscard]] iterator end() noexcept { return iterator{nullptr}; }
  [[nodiscard]] iterator end() const noexcept { return iterator{nullptr}; }

private:
  lxb_dom_node_t* start;
};

class attr_walker : public std::ranges::view_interface<attr_walker> {
public:
  using value_type = lxb_dom_attr_t*;

  explicit attr_walker(lxb_dom_node_t* node) : start(nullptr) {
    if (not is_non_element_node(node)) {
      start = lxb_dom_element_first_attribute(lxb_dom_interface_element(node));
    }
  }
  attr_walker(lxb_dom_attr_t* attr = nullptr) : start(attr) {  }

  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_attr_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_attr_t**;
    using reference = lxb_dom_attr_t*&;

    iterator(lxb_dom_attr_t* node = nullptr) : current(node) { }
    // iterator(const iterator&) = default;
    // iterator(iterator&&) = default;

    iterator& operator++() noexcept {
      if (current == nullptr) {
        return *this;
      }
      current = lxb_dom_element_next_attribute(current);
      return *this;
    }

    // TODO: 最適化
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }
    lxb_dom_attr_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_attr_t* current;
  };


  [[nodiscard]] iterator begin() noexcept { return iterator{start}; }
  [[nodiscard]] iterator begin() const noexcept { return iterator{start}; }
  [[nodiscard]] iterator end() noexcept { return iterator{nullptr}; }
  [[nodiscard]] iterator end() const noexcept { return iterator{nullptr}; }

private:
  lxb_dom_attr_t* start;
};

auto inline tag_name(lxb_tag_id_t const tag_id) {
  using namespace std::string_view_literals;
  switch(tag_id) {
  case LXB_TAG__UNDEF: return "UNDEF"sv;
  case LXB_TAG__END_OF_FILE: return "END_OF_FILE"sv;
  case LXB_TAG__TEXT: return "TEXT"sv;
  case LXB_TAG__DOCUMENT: return "DOCUMENT"sv;
  case LXB_TAG__EM_COMMENT: return "EM_COMMENT"sv;
  case LXB_TAG__EM_DOCTYPE: return "EM_DOCTYPE"sv;
  case LXB_TAG_A: return "A"sv;
  case LXB_TAG_ABBR: return "ABBR"sv;
  case LXB_TAG_ACRONYM: return "ACRONYM"sv;
  case LXB_TAG_ADDRESS: return "ADDRESS"sv;
  case LXB_TAG_ALTGLYPH: return "ALTGLYPH"sv;
  case LXB_TAG_ALTGLYPHDEF: return "ALTGLYPHDEF"sv;
  case LXB_TAG_ALTGLYPHITEM: return "ALTGLYPHITEM"sv;
  case LXB_TAG_ANIMATECOLOR: return "ANIMATECOLOR"sv;
  case LXB_TAG_ANIMATEMOTION: return "ANIMATEMOTION"sv;
  case LXB_TAG_ANIMATETRANSFORM: return "ANIMATETRANSFORM"sv;
  case LXB_TAG_ANNOTATION_XML: return "ANNOTATION_XML"sv;
  case LXB_TAG_APPLET: return "APPLET"sv;
  case LXB_TAG_AREA: return "AREA"sv;
  case LXB_TAG_ARTICLE: return "ARTICLE"sv;
  case LXB_TAG_ASIDE: return "ASIDE"sv;
  case LXB_TAG_AUDIO: return "AUDIO"sv;
  case LXB_TAG_B: return "B"sv;
  case LXB_TAG_BASE: return "BASE"sv;
  case LXB_TAG_BASEFONT: return "BASEFONT"sv;
  case LXB_TAG_BDI: return "BDI"sv;
  case LXB_TAG_BDO: return "BDO"sv;
  case LXB_TAG_BGSOUND: return "BGSOUND"sv;
  case LXB_TAG_BIG: return "BIG"sv;
  case LXB_TAG_BLINK: return "BLINK"sv;
  case LXB_TAG_BLOCKQUOTE: return "BLOCKQUOTE"sv;
  case LXB_TAG_BODY: return "BODY"sv;
  case LXB_TAG_BR: return "BR"sv;
  case LXB_TAG_BUTTON: return "BUTTON"sv;
  case LXB_TAG_CANVAS: return "CANVAS"sv;
  case LXB_TAG_CAPTION: return "CAPTION"sv;
  case LXB_TAG_CENTER: return "CENTER"sv;
  case LXB_TAG_CITE: return "CITE"sv;
  case LXB_TAG_CLIPPATH: return "CLIPPATH"sv;
  case LXB_TAG_CODE: return "CODE"sv;
  case LXB_TAG_COL: return "COL"sv;
  case LXB_TAG_COLGROUP: return "COLGROUP"sv;
  case LXB_TAG_DATA: return "DATA"sv;
  case LXB_TAG_DATALIST: return "DATALIST"sv;
  case LXB_TAG_DD: return "DD"sv;
  case LXB_TAG_DEL: return "DEL"sv;
  case LXB_TAG_DESC: return "DESC"sv;
  case LXB_TAG_DETAILS: return "DETAILS"sv;
  case LXB_TAG_DFN: return "DFN"sv;
  case LXB_TAG_DIALOG: return "DIALOG"sv;
  case LXB_TAG_DIR: return "DIR"sv;
  case LXB_TAG_DIV: return "DIV"sv;
  case LXB_TAG_DL: return "DL"sv;
  case LXB_TAG_DT: return "DT"sv;
  case LXB_TAG_EM: return "EM"sv;
  case LXB_TAG_EMBED: return "EMBED"sv;
  case LXB_TAG_FEBLEND: return "FEBLEND"sv;
  case LXB_TAG_FECOLORMATRIX: return "FECOLORMATRIX"sv;
  case LXB_TAG_FECOMPONENTTRANSFER: return "FECOMPONENTTRANSFER"sv;
  case LXB_TAG_FECOMPOSITE: return "FECOMPOSITE"sv;
  case LXB_TAG_FECONVOLVEMATRIX: return "FECONVOLVEMATRIX"sv;
  case LXB_TAG_FEDIFFUSELIGHTING: return "FEDIFFUSELIGHTING"sv;
  case LXB_TAG_FEDISPLACEMENTMAP: return "FEDISPLACEMENTMAP"sv;
  case LXB_TAG_FEDISTANTLIGHT: return "FEDISTANTLIGHT"sv;
  case LXB_TAG_FEDROPSHADOW: return "FEDROPSHADOW"sv;
  case LXB_TAG_FEFLOOD: return "FEFLOOD"sv;
  case LXB_TAG_FEFUNCA: return "FEFUNCA"sv;
  case LXB_TAG_FEFUNCB: return "FEFUNCB"sv;
  case LXB_TAG_FEFUNCG: return "FEFUNCG"sv;
  case LXB_TAG_FEFUNCR: return "FEFUNCR"sv;
  case LXB_TAG_FEGAUSSIANBLUR: return "FEGAUSSIANBLUR"sv;
  case LXB_TAG_FEIMAGE: return "FEIMAGE"sv;
  case LXB_TAG_FEMERGE: return "FEMERGE"sv;
  case LXB_TAG_FEMERGENODE: return "FEMERGENODE"sv;
  case LXB_TAG_FEMORPHOLOGY: return "FEMORPHOLOGY"sv;
  case LXB_TAG_FEOFFSET: return "FEOFFSET"sv;
  case LXB_TAG_FEPOINTLIGHT: return "FEPOINTLIGHT"sv;
  case LXB_TAG_FESPECULARLIGHTING: return "FESPECULARLIGHTING"sv;
  case LXB_TAG_FESPOTLIGHT: return "FESPOTLIGHT"sv;
  case LXB_TAG_FETILE: return "FETILE"sv;
  case LXB_TAG_FETURBULENCE: return "FETURBULENCE"sv;
  case LXB_TAG_FIELDSET: return "FIELDSET"sv;
  case LXB_TAG_FIGCAPTION: return "FIGCAPTION"sv;
  case LXB_TAG_FIGURE: return "FIGURE"sv;
  case LXB_TAG_FONT: return "FONT"sv;
  case LXB_TAG_FOOTER: return "FOOTER"sv;
  case LXB_TAG_FOREIGNOBJECT: return "FOREIGNOBJECT"sv;
  case LXB_TAG_FORM: return "FORM"sv;
  case LXB_TAG_FRAME: return "FRAME"sv;
  case LXB_TAG_FRAMESET: return "FRAMESET"sv;
  case LXB_TAG_GLYPHREF: return "GLYPHREF"sv;
  case LXB_TAG_H1: return "H1"sv;
  case LXB_TAG_H2: return "H2"sv;
  case LXB_TAG_H3: return "H3"sv;
  case LXB_TAG_H4: return "H4"sv;
  case LXB_TAG_H5: return "H5"sv;
  case LXB_TAG_H6: return "H6"sv;
  case LXB_TAG_HEAD: return "HEAD"sv;
  case LXB_TAG_HEADER: return "HEADER"sv;
  case LXB_TAG_HGROUP: return "HGROUP"sv;
  case LXB_TAG_HR: return "HR"sv;
  case LXB_TAG_HTML: return "HTML"sv;
  case LXB_TAG_I: return "I"sv;
  case LXB_TAG_IFRAME: return "IFRAME"sv;
  case LXB_TAG_IMAGE: return "IMAGE"sv;
  case LXB_TAG_IMG: return "IMG"sv;
  case LXB_TAG_INPUT: return "INPUT"sv;
  case LXB_TAG_INS: return "INS"sv;
  case LXB_TAG_ISINDEX: return "ISINDEX"sv;
  case LXB_TAG_KBD: return "KBD"sv;
  case LXB_TAG_KEYGEN: return "KEYGEN"sv;
  case LXB_TAG_LABEL: return "LABEL"sv;
  case LXB_TAG_LEGEND: return "LEGEND"sv;
  case LXB_TAG_LI: return "LI"sv;
  case LXB_TAG_LINEARGRADIENT: return "LINEARGRADIENT"sv;
  case LXB_TAG_LINK: return "LINK"sv;
  case LXB_TAG_LISTING: return "LISTING"sv;
  case LXB_TAG_MAIN: return "MAIN"sv;
  case LXB_TAG_MALIGNMARK: return "MALIGNMARK"sv;
  case LXB_TAG_MAP: return "MAP"sv;
  case LXB_TAG_MARK: return "MARK"sv;
  case LXB_TAG_MARQUEE: return "MARQUEE"sv;
  case LXB_TAG_MATH: return "MATH"sv;
  case LXB_TAG_MENU: return "MENU"sv;
  case LXB_TAG_META: return "META"sv;
  case LXB_TAG_METER: return "METER"sv;
  case LXB_TAG_MFENCED: return "MFENCED"sv;
  case LXB_TAG_MGLYPH: return "MGLYPH"sv;
  case LXB_TAG_MI: return "MI"sv;
  case LXB_TAG_MN: return "MN"sv;
  case LXB_TAG_MO: return "MO"sv;
  case LXB_TAG_MS: return "MS"sv;
  case LXB_TAG_MTEXT: return "MTEXT"sv;
  case LXB_TAG_MULTICOL: return "MULTICOL"sv;
  case LXB_TAG_NAV: return "NAV"sv;
  case LXB_TAG_NEXTID: return "NEXTID"sv;
  case LXB_TAG_NOBR: return "NOBR"sv;
  case LXB_TAG_NOEMBED: return "NOEMBED"sv;
  case LXB_TAG_NOFRAMES: return "NOFRAMES"sv;
  case LXB_TAG_NOSCRIPT: return "NOSCRIPT"sv;
  case LXB_TAG_OBJECT: return "OBJECT"sv;
  case LXB_TAG_OL: return "OL"sv;
  case LXB_TAG_OPTGROUP: return "OPTGROUP"sv;
  case LXB_TAG_OPTION: return "OPTION"sv;
  case LXB_TAG_OUTPUT: return "OUTPUT"sv;
  case LXB_TAG_P: return "P"sv;
  case LXB_TAG_PARAM: return "PARAM"sv;
  case LXB_TAG_PATH: return "PATH"sv;
  case LXB_TAG_PICTURE: return "PICTURE"sv;
  case LXB_TAG_PLAINTEXT: return "PLAINTEXT"sv;
  case LXB_TAG_PRE: return "PRE"sv;
  case LXB_TAG_PROGRESS: return "PROGRESS"sv;
  case LXB_TAG_Q: return "Q"sv;
  case LXB_TAG_RADIALGRADIENT: return "RADIALGRADIENT"sv;
  case LXB_TAG_RB: return "RB"sv;
  case LXB_TAG_RP: return "RP"sv;
  case LXB_TAG_RT: return "RT"sv;
  case LXB_TAG_RTC: return "RTC"sv;
  case LXB_TAG_RUBY: return "RUBY"sv;
  case LXB_TAG_S: return "S"sv;
  case LXB_TAG_SAMP: return "SAMP"sv;
  case LXB_TAG_SCRIPT: return "SCRIPT"sv;
  case LXB_TAG_SECTION: return "SECTION"sv;
  case LXB_TAG_SELECT: return "SELECT"sv;
  case LXB_TAG_SLOT: return "SLOT"sv;
  case LXB_TAG_SMALL: return "SMALL"sv;
  case LXB_TAG_SOURCE: return "SOURCE"sv;
  case LXB_TAG_SPACER: return "SPACER"sv;
  case LXB_TAG_SPAN: return "SPAN"sv;
  case LXB_TAG_STRIKE: return "STRIKE"sv;
  case LXB_TAG_STRONG: return "STRONG"sv;
  case LXB_TAG_STYLE: return "STYLE"sv;
  case LXB_TAG_SUB: return "SUB"sv;
  case LXB_TAG_SUMMARY: return "SUMMARY"sv;
  case LXB_TAG_SUP: return "SUP"sv;
  case LXB_TAG_SVG: return "SVG"sv;
  case LXB_TAG_TABLE: return "TABLE"sv;
  case LXB_TAG_TBODY: return "TBODY"sv;
  case LXB_TAG_TD: return "TD"sv;
  case LXB_TAG_TEMPLATE: return "TEMPLATE"sv;
  case LXB_TAG_TEXTAREA: return "TEXTAREA"sv;
  case LXB_TAG_TEXTPATH: return "TEXTPATH"sv;
  case LXB_TAG_TFOOT: return "TFOOT"sv;
  case LXB_TAG_TH: return "TH"sv;
  case LXB_TAG_THEAD: return "THEAD"sv;
  case LXB_TAG_TIME: return "TIME"sv;
  case LXB_TAG_TITLE: return "TITLE"sv;
  case LXB_TAG_TR: return "TR"sv;
  case LXB_TAG_TRACK: return "TRACK"sv;
  case LXB_TAG_TT: return "TT"sv;
  case LXB_TAG_U: return "U"sv;
  case LXB_TAG_UL: return "UL"sv;
  case LXB_TAG_VAR: return "VAR"sv;
  case LXB_TAG_VIDEO: return "VIDEO"sv;
  case LXB_TAG_WBR: return "WBR"sv;
  case LXB_TAG_XMP: return "XMP"sv;
  default: return ""sv;
  }
}

} // namespace lexborpp

namespace std::ranges {

template <>
inline constexpr bool enable_view<lexborpp::node_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_walker> = true;

template <>
inline constexpr bool enable_view<lexborpp::node_sibling_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_sibling_walker> = true;

template <>
inline constexpr bool enable_view<lexborpp::attr_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::attr_walker> = true;

}  // namespace std::ranges



#endif /* __LEXBORPP_HPP__ */
