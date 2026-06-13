#ifndef LEXBORPP_CORE_HPP_
#define LEXBORPP_CORE_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lexbor/html/parser.h"
#include "lexbor/html/serialize.h"
#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"
#include "lexbor/selectors/selectors.h"

namespace lexborpp {
// --- Forward Declarations & Helpers ---

/**
 * @brief テキストノードの文字列ビューを取得します。
 *
 * @param node 対象ノードです。
 * @return std::string_view テキストノードであればその内容、そうでなければ空文字列ビューを返します。
 */
auto inline to_string(lxb_dom_node_t const* node) noexcept {
  if (node == nullptr || node->type != LXB_DOM_NODE_TYPE_TEXT) {
    return std::string_view{};
  }
  auto const data = lxb_dom_interface_character_data(const_cast<lxb_dom_node_t*>(node));
  return std::string_view{reinterpret_cast<const char*>(data->data.data), data->data.length};
}

/**
 * @brief 属性名を文字列ビューとして取得します。
 *
 * @param attr 対象属性です。
 * @return std::string_view 属性名を返します。取得できない場合は空文字列ビューを返します。
 */
auto inline to_name_string(lxb_dom_attr_t const* attr) noexcept {
  if (attr == nullptr) {
    return std::string_view{};
  }
  auto attr_val_len  = size_t{};
  auto attr_val_data = lxb_dom_attr_qualified_name(const_cast<lxb_dom_attr_t*>(attr), &attr_val_len);
  if (attr_val_data == nullptr) {
    return std::string_view{};
  }
  return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
}

/**
 * @brief 属性値を文字列ビューとして取得します。
 *
 * @param attr 対象属性です。
 * @return std::string_view 属性値を返します。取得できない場合は空文字列ビューを返します。
 */
auto inline to_string(lxb_dom_attr_t const* attr) noexcept {
  if (attr == nullptr) {
    return std::string_view{};
  }
  auto attr_val_len  = size_t{};
  auto attr_val_data = lxb_dom_attr_value(const_cast<lxb_dom_attr_t*>(attr), &attr_val_len);
  if (attr_val_data == nullptr) {
    return std::string_view{};
  }
  return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
}

/**
 * @brief ノードが要素ノード以外かを判定します。
 *
 * @param node 判定対象ノードです。
 * @return bool nullptr、テキスト、document、comment、doctype のいずれかであれば true を返します。
 */
auto constexpr inline is_non_element_node(lxb_dom_node_t const* node) noexcept -> bool {
  if (node == nullptr) {
    return true;
  }
  auto const tag_id = lxb_dom_node_tag_id(const_cast<lxb_dom_node_t*>(node));
  return tag_id == LXB_TAG__UNDEF || tag_id == LXB_TAG__TEXT || tag_id == LXB_TAG__DOCUMENT ||
         tag_id == LXB_TAG__EM_COMMENT || tag_id == LXB_TAG__EM_DOCTYPE;
}

/**
 * @brief ノードを要素型に変換します（キャスト）。
 */
[[nodiscard]] auto constexpr inline as_element(lxb_dom_node_t const* node) noexcept -> lxb_dom_element_t* {
  if (node == nullptr) {
    return nullptr;
  }
  return lxb_dom_interface_element(const_cast<lxb_dom_node_t*>(node));
}

// --- Walkers ---

/**
 * @brief 指定ノード以下を深さ優先で巡回する Range View です。
 */
class node_walker : public std::ranges::view_interface<node_walker> {
public:
  using value_type = lxb_dom_node_t*;

  /**
   * @brief 巡回開始ノードを指定して初期化します。
   *
  * @param node 巡回開始ノードです。
   */
  explicit node_walker(lxb_dom_node_t* node = nullptr) : start(node) { }

  /**
   * @brief 深さ優先巡回用の forward iterator です。
   */
  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_node_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_node_t**;
    using reference = lxb_dom_node_t*&;

    /**
     * @brief イテレータを初期化します。
     *
    * @param node 巡回の起点ノードです。
    * @param end 現在位置です。
     */
    iterator(lxb_dom_node_t* node = nullptr, lxb_dom_node_t* end = nullptr) : start(node), current(end) { }

    /**
     * @brief 次のノードへ進めます。
     *
    * @return iterator& 自身への参照を返します。
     */
    iterator& operator++() noexcept {
      if (current == nullptr) {
        return *this;
      }

      if (current->first_child != nullptr) {
        current = current->first_child;
        return *this;
      }

      while (current != nullptr && current != start && current->next == nullptr) {
        current = current->parent;
      }

      if (current == start || current == nullptr) {
        current = nullptr;
      } else {
        current = current->next;
      }

      return *this;
    }

    /**
     * @brief 現在位置を返してから次へ進めます。
     *
    * @return iterator 進行前のイテレータを返します。
     */
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    /**
     * @brief ほかのイテレータと等しいか比較します。
     *
    * @param rhs 比較対象です。
    * @return auto 現在位置が同じ場合に true を返します。
     */
    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }

    /**
     * @brief 現在ノードを参照します。
     *
    * @return lxb_dom_node_t* const& 現在ノードを返します。
     */
    lxb_dom_node_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_node_t* start;
    lxb_dom_node_t* current;
  };

  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() noexcept { return iterator{start, start}; }
  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() const noexcept { return iterator{start, start}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() noexcept { return iterator{start, nullptr}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() const noexcept { return iterator{start, nullptr}; }

private:
  lxb_dom_node_t* start;
};

/**
 * @brief 指定ノードから後続兄弟を順方向に巡回する Range View です。
 */
class node_sibling_walker : public std::ranges::view_interface<node_sibling_walker> {
public:
  using value_type = lxb_dom_node_t*;

  /**
   * @brief 巡回開始ノードを指定して初期化します。
   *
  * @param node 巡回開始ノードです。
   */
  explicit node_sibling_walker(lxb_dom_node_t* node = nullptr) : start(node) { }

  /**
   * @brief 兄弟ノードを順方向にたどる forward iterator です。
   */
  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_node_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_node_t**;
    using reference = lxb_dom_node_t*&;

    /**
     * @brief イテレータを初期化します。
     *
    * @param node 現在ノードです。
     */
    iterator(lxb_dom_node_t* node = nullptr) : current(node) { }

    /**
     * @brief 次の兄弟ノードへ進めます。
     *
    * @return iterator& 自身への参照を返します。
     */
    iterator& operator++() noexcept {
      if (current != nullptr) {
        current = current->next;
      }
      return *this;
    }

    /**
     * @brief 現在位置を返してから次へ進めます。
     *
    * @return iterator 進行前のイテレータを返します。
     */
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    /**
     * @brief ほかのイテレータと等しいか比較します。
     *
    * @param rhs 比較対象です。
    * @return auto 現在位置が同じ場合に true を返します。
     */
    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }

    /**
     * @brief 現在ノードを参照します。
     *
    * @return lxb_dom_node_t* const& 現在ノードを返します。
     */
    lxb_dom_node_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_node_t* current;
  };

  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() noexcept { return iterator{start}; }
  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() const noexcept { return iterator{start}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() noexcept { return iterator{nullptr}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() const noexcept { return iterator{nullptr}; }

private:
  lxb_dom_node_t* start;
};

/**
 * @brief 指定ノードの直前兄弟を逆方向に巡回する Range View です。
 */
class node_prev_sibling_walker : public std::ranges::view_interface<node_prev_sibling_walker> {
public:
  using value_type = lxb_dom_node_t*;

  /**
   * @brief 指定ノードの直前兄弟から巡回を開始します。
   *
  * @param node 基準ノードです。
   */
  explicit node_prev_sibling_walker(lxb_dom_node_t* node = nullptr)
    : start(node ? node->prev : nullptr) { }

  /**
   * @brief 兄弟ノードを逆方向にたどる forward iterator です。
   */
  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_node_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_node_t**;
    using reference = lxb_dom_node_t*&;

    /**
     * @brief イテレータを初期化します。
     *
    * @param node 現在ノードです。
     */
    iterator(lxb_dom_node_t* node = nullptr) : current(node) { }

    /**
     * @brief 前の兄弟ノードへ進めます。
     *
    * @return iterator& 自身への参照を返します。
     */
    iterator& operator++() noexcept {
      if (current != nullptr) {
        current = current->prev;
      }
      return *this;
    }

    /**
     * @brief 現在位置を返してから前へ進めます。
     *
    * @return iterator 進行前のイテレータを返します。
     */
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    /**
     * @brief ほかのイテレータと等しいか比較します。
     *
    * @param rhs 比較対象です。
    * @return auto 現在位置が同じ場合に true を返します。
     */
    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }

    /**
     * @brief 現在ノードを参照します。
     *
    * @return lxb_dom_node_t* const& 現在ノードを返します。
     */
    lxb_dom_node_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_node_t* current;
  };

  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() noexcept { return iterator{start}; }
  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() const noexcept { return iterator{start}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() noexcept { return iterator{nullptr}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() const noexcept { return iterator{nullptr}; }

private:
  lxb_dom_node_t* start;
};

/**
 * @brief 要素の属性列を巡回する Range View です。
 */
class attr_walker : public std::ranges::view_interface<attr_walker> {
public:
  using value_type = lxb_dom_attr_t*;

  /**
   * @brief ノードから属性巡回を初期化します。
   *
  * @param node 対象ノードです。
   */
  explicit attr_walker(lxb_dom_node_t* node) : start(nullptr) {
    if (node != nullptr and not is_non_element_node(node)) {
      start = lxb_dom_element_first_attribute(lxb_dom_interface_element(node));
    }
  }

  /**
   * @brief 属性ノードから属性巡回を初期化します。
   *
  * @param attr 開始属性です。
   */
  attr_walker(lxb_dom_attr_t* attr = nullptr) : start(attr) { }

  /**
   * @brief 属性列を順方向にたどる forward iterator です。
   */
  class iterator {
  public:
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::forward_iterator_tag;
    using value_type = lxb_dom_attr_t*;
    using difference_type = std::ptrdiff_t;
    using pointer = lxb_dom_attr_t**;
    using reference = lxb_dom_attr_t*&;

    /**
     * @brief イテレータを初期化します。
     *
    * @param node 現在属性です。
     */
    iterator(lxb_dom_attr_t* node = nullptr) : current(node) { }

    /**
     * @brief 次の属性へ進めます。
     *
    * @return iterator& 自身への参照を返します。
     */
    iterator& operator++() noexcept {
      if (current != nullptr) {
        current = lxb_dom_element_next_attribute(current);
      }
      return *this;
    }

    /**
     * @brief 現在位置を返してから次へ進めます。
     *
    * @return iterator 進行前のイテレータを返します。
     */
    iterator operator++(int) noexcept {
      auto temp = *this;
      ++*this;
      return temp;
    }

    /**
     * @brief ほかのイテレータと等しいか比較します。
     *
    * @param rhs 比較対象です。
    * @return auto 現在位置が同じ場合に true を返します。
     */
    auto operator==(iterator const &rhs) const noexcept { return current == rhs.current; }

    /**
     * @brief 現在属性を参照します。
     *
    * @return lxb_dom_attr_t* const& 現在属性を返します。
     */
    lxb_dom_attr_t* const& operator*() const noexcept { return current; }

  private:
    lxb_dom_attr_t* current;
  };

  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() noexcept { return iterator{start}; }
  /** @brief 先頭イテレータを返します。 */
  [[nodiscard]] iterator begin() const noexcept { return iterator{start}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() noexcept { return iterator{nullptr}; }
  /** @brief 終端イテレータを返します。 */
  [[nodiscard]] iterator end() const noexcept { return iterator{nullptr}; }

private:
  lxb_dom_attr_t* start;
};

// --- Core API ---

/**
 * @brief ノードが指定されたクラスを持っているか確認します。
 *
 * @param node 確認対象のノードです。
 * @param class_name クラス名です。
 * @return bool 指定されたクラスを持っている場合は true、そうでない場合は false を返します。
 * @note class 属性が空白区切りのリストであることに対応しています。
 */
[[nodiscard]] auto inline has_class(lxb_dom_node_t const* node, std::string_view class_name) noexcept -> bool {
  if (node == nullptr or class_name.empty() or is_non_element_node(node)) {
    return false;
  }

  auto attr_len = size_t{};
  auto const attr = lxb_dom_element_get_attribute(lxb_dom_interface_element(const_cast<lxb_dom_node_t*>(node)),
                                                  reinterpret_cast<const lxb_char_t*>("class"), 5, &attr_len);
  if (attr == nullptr) {
    return false;
  }

  auto const attr_view = std::string_view{reinterpret_cast<const char*>(attr), attr_len};
  auto constexpr delimiters = std::string_view{" \t\n\r\f"};

  for (auto start = attr_view.find_first_not_of(delimiters); start != std::string_view::npos; ) {
    auto const end = attr_view.find_first_of(delimiters, start);
    if (attr_view.substr(start, end - start) == class_name) {
      return true;
    }
    start = attr_view.find_first_not_of(delimiters, end);
  }

  return false;
}


/**
 * @brief ノードが指定されたすべてのクラスを持っているか確認します。
 *
 * @param node 確認対象のノードです。
 * @param class_names クラス名のリストです。
 * @return bool すべてのクラスを持っている場合は true、そうでない場合は false を返します。
 */
[[nodiscard]] constexpr auto inline has_class(lxb_dom_node_t const* node, std::initializer_list<std::string_view> class_names) noexcept -> bool {
  if (class_names.size() == 0 or node == nullptr) {
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
 * @brief class 属性が指定された文字列と完全一致する最初の要素を取得します。
 *
 * @param node 検索を開始するノードです。
 * @param class_name 検索するクラス属性文字列（完全一致）です。
 * @return lxb_dom_node_t* 最初に見つかった要素を返します。見つからない場合は nullptr を返します。
 */
auto inline get_first_element_by_class(lxb_dom_node_t* node, std::string_view class_name) noexcept -> lxb_dom_node_t* {
  if (node == nullptr or class_name.empty()) {
    return nullptr;
  }

  auto const text_walker = +[](lxb_dom_node_t* node, void* ctx) noexcept {
    if (is_non_element_node(node)) {
      return LEXBOR_ACTION_OK;
    }

    auto& [target, result] = *reinterpret_cast<std::pair<std::string_view, lxb_dom_node*>*>(ctx);
    auto attr_len = size_t{};
    auto const attr = lxb_dom_element_get_attribute(lxb_dom_interface_element(node), reinterpret_cast<const lxb_char_t*>("class"), 5, &attr_len);
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
 * @brief 指定されたクラスを持つすべての要素を取得します。
 *
 * @param node 検索対象のノードです。
 * @param class_name 検索するクラス名です。
 * @return std::vector<lxb_dom_node_t*> マッチした全要素のリストを返します。
 * @note has_class 関数を使用して、空白区切りのリストからマッチングを行います。
 */
[[nodiscard]] auto inline get_elements_by_class(lxb_dom_node_t* node, std::string_view class_name) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr or class_name.empty()) {
    return result;
  }

  for (auto* current : node_walker{node}) {
    if (has_class(current, class_name)) {
      result.push_back(current);
    }
  }
  return result;
}

/**
 * @brief 全子孫テキストノードを結合して取得します。
 *
 * @param node 対象のノードです。
 * @param sep 各テキストノードの間に挿入するセパレータ（デフォルトは空文字列）です。
 * @return std::string 結合されたテキスト文字列を返します。
 *
 * @note
 * get_first_child_text  → 直下の最初のテキストノードのみ
 * get_all_children_text → 直下のテキストノードのみ（孫以下除外）
 * get_deep_text         → 全子孫テキストノード（本関数）
 *
 * sep が空文字列の場合は Lexbor ネイティブ API (lxb_dom_node_text_content) を使用し、
 * それ以外の場合は node_walker を用いてテキストを結合する。
 */
[[nodiscard]] auto inline get_deep_text(lxb_dom_node_t const* node, std::string_view sep = "") -> std::string {
  if (node == nullptr) {
    return "";
  }

  if (node->type == LXB_DOM_NODE_TYPE_TEXT) {
    return std::string{to_string(const_cast<lxb_dom_node_t*>(node))};
  }

  if (sep.empty()) {
    // 案 A — Lexbor ネイティブ API を使う（推奨）
    auto len = size_t{};
    auto* const text = lxb_dom_node_text_content(const_cast<lxb_dom_node_t*>(node), &len);
    if (text == nullptr) {
      return "";
    }
    auto result = std::string{reinterpret_cast<const char*>(text), len};
    lxb_dom_document_destroy_text(node->owner_document, text);
    return result;
  }

  // 案 B — node_walker を使う
  auto result = std::string{};
  auto first = true;
  for (auto* n : node_walker{const_cast<lxb_dom_node_t*>(node)}) {
    if (n->type != LXB_DOM_NODE_TYPE_TEXT) {
      continue;
    }
    auto const data = lxb_dom_interface_character_data(n);
    auto const sv = std::string_view{reinterpret_cast<const char*>(data->data.data), data->data.length};
    if (not first) {
      result.append(sep);
    }
    result.append(sv);
    first = false;
  }
  return result;
}

/**
 * @brief 要素に属性を設定または更新します。
 *
 * @param element 対象要素です。
 * @param name 属性名です。
 * @param value 属性値です。
 * @return bool 設定に成功した場合に true を返します。
 */
[[nodiscard]] auto inline set_attr(lxb_dom_element_t* element, std::string_view name, std::string_view value) noexcept -> bool {
  if (element == nullptr) {
    return false;
  }
  auto const attr = lxb_dom_element_set_attribute(element,
    reinterpret_cast<lxb_char_t const*>(name.data()), name.size(),
    reinterpret_cast<lxb_char_t const*>(value.data()), value.size());
  return attr != nullptr;
}

/**
 * @brief 要素から指定属性を削除します。
 *
 * @param element 対象要素です。
 * @param name 削除する属性名です。
 * @return bool 削除に成功した場合に true を返します。
 */
[[nodiscard]] auto inline remove_attr(lxb_dom_element_t* element, std::string_view name) noexcept -> bool {
  if (element == nullptr) {
    return false;
  }
  auto const status = lxb_dom_element_remove_attribute(element,
    reinterpret_cast<lxb_char_t const*>(name.data()), name.size());
  return status == LXB_STATUS_OK;
}

/**
 * @brief ノードの子要素をテキストノード 1 つに置き換えます。
 *
 * @param node 対象ノードです。
 * @param text 設定するテキストです。
 * @return bool 設定に成功した場合に true を返します。
 */
[[nodiscard]] auto inline set_text_content(lxb_dom_node_t* node, std::string_view text) noexcept -> bool {
  if (node == nullptr) {
    return false;
  }

  auto* child = lxb_dom_node_first_child(node);
  while (child != nullptr) {
    auto* next = lxb_dom_node_next(child);
    lxb_dom_node_remove(child);
    lxb_dom_node_destroy(child);
    child = next;
  }

  if (text.empty()) {
    return true;
  }

  auto* const text_node = lxb_dom_document_create_text_node(node->owner_document,
    reinterpret_cast<lxb_char_t const*>(text.data()), text.size());
  if (text_node == nullptr) {
    return false;
  }

  lxb_dom_node_insert_child(node, lxb_dom_interface_node(text_node));
  return true;
}

// --- Document RAII ---

/**
 * @brief `lxb_html_document_t` を `std::unique_ptr` で破棄するための deleter です。
 */
struct document_deleter {
  /**
   * @brief HTML ドキュメントを破棄します。
   *
  * @param doc 破棄対象ドキュメントです。
   */
  auto constexpr operator()(lxb_html_document_t* doc) const noexcept -> void {
    if (doc != nullptr) {
      lxb_html_document_destroy(doc);
    }
  }
};

using document_ptr = std::unique_ptr<lxb_html_document_t, document_deleter>;

/**
 * @brief HTML 文字列をパースして RAII 管理されたドキュメントを返します。
 *
 * @param html パース対象の HTML 文字列です。
 * @return std::expected<document_ptr, lxb_status_t> 成功時はドキュメント、失敗時は Lexbor ステータスを返します。
 */
[[nodiscard]] auto inline parse_html(std::string_view const html) noexcept -> std::expected<document_ptr, lxb_status_t> {
  auto* const doc = lxb_html_document_create();
  if (doc == nullptr) {
    return std::unexpected{LXB_STATUS_ERROR_MEMORY_ALLOCATION};
  }

  auto const status = lxb_html_document_parse(doc, reinterpret_cast<lxb_char_t const*>(html.data()), html.size());
  if (status != LXB_STATUS_OK) {
    lxb_html_document_destroy(doc);
    return std::unexpected{status};
  }

  return document_ptr{doc};
}

/**
 * @brief ドキュメントのルートノードを取得します。
 *
 * @param doc ドキュメントです。
 * @return lxb_dom_node_t* ルートノードを返します。無効な場合は nullptr を返します。
 */
[[nodiscard]] auto constexpr inline get_root(document_ptr const& doc) noexcept -> lxb_dom_node_t* {
  if (not doc) {
    return nullptr;
  }
  return lxb_dom_interface_node(doc.get());
}

// --- Lookup & Attributes ---

/**
 * @brief 指定した id 属性値を持つ最初の要素を取得します。
 *
 * @param node 検索開始ノードです。
 * @param id_name 検索する id 属性値です。
 * @return lxb_dom_node_t* 最初に見つかった要素を返します。見つからない場合は nullptr を返します。
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
 * @brief 要素から指定属性の値を取得します。
 *
 * @param node 対象ノードです。
 * @param attr_name 属性名です。
 * @return std::optional<std::string_view> 属性値を返します。存在しない場合は std::nullopt を返します。
 */
auto inline get_attr_value(lxb_dom_node_t* node, std::string_view attr_name) noexcept -> std::optional<std::string_view> {
  if (node == nullptr or is_non_element_node(node)) {
    return std::nullopt;
  }

  auto* const element = lxb_dom_interface_element(node);
  auto attr_val_len = size_t{};

  if (attr_name == "id") {
    auto const attr_val_data = lxb_dom_element_id(element, &attr_val_len);
    if (attr_val_data != nullptr) {
      return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
    }
    return std::nullopt;
  }

  if (attr_name == "class") {
    auto const attr_val_data = lxb_dom_element_class(element, &attr_val_len);
    if (attr_val_data != nullptr) {
      return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
    }
    return std::nullopt;
  }

  auto* attr = lxb_dom_element_attr_by_name(element, reinterpret_cast<lxb_char_t const*>(attr_name.data()), attr_name.size());
  if (attr != nullptr) {
    auto const attr_val_data = lxb_dom_attr_value(attr, &attr_val_len);
    if (attr_val_data != nullptr) {
      return std::string_view{reinterpret_cast<const char*>(attr_val_data), attr_val_len};
    }
  }
  return std::nullopt;
}

/**
 * @brief 直下の最初のテキスト子ノードを取得します。
 *
 * @param node 対象ノードです。
 * @return std::optional<std::string_view> 最初のテキストを返します。存在しない場合は std::nullopt を返します。
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
 * @brief 直下のすべてのテキスト子ノードを連結して取得します。
 *
 * @param node 対象ノードです。
 * @return std::optional<std::string> 連結文字列を返します。テキスト子が無い場合は std::nullopt を返します。
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
 * @brief 直下のすべてのテキスト子ノードを区切り文字付きで連結して取得します。
 *
 * @param node 対象ノードです。
 * @param sep 各テキストの間に挿入する区切り文字です。
 * @return std::optional<std::string> 連結文字列を返します。テキスト子が無い場合は std::nullopt を返します。
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

/**
 * @brief 述語に一致する後続ノードを走査して最初の 1 件を返します。
 *
 * @tparam Op ノードを判定する述語型です。
 * @param node 検索開始ノードです。
 * @param op 判定関数です。
 * @return lxb_dom_node_t* 一致したノードを返します。見つからない場合は nullptr を返します。
 */
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

/**
 * @brief 兄弟列を順方向に走査し、述語に一致する最初のノードを返します。
 *
 * @tparam Op ノードを判定する述語型です。
 * @param node 検索開始ノードです。
 * @param op 判定関数です。
 * @return lxb_dom_node_t* 一致したノードを返します。見つからない場合は nullptr を返します。
 */
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
}  // namespace lexborpp

#endif  // LEXBORPP_CORE_HPP_
