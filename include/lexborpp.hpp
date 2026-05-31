#ifndef LEXBORPP_HPP_
#define LEXBORPP_HPP_

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
  auto const walker = node_walker{const_cast<lxb_dom_node_t*>(node)};
  auto text_nodes = walker
    | std::views::filter([](lxb_dom_node_t* n) noexcept { return n->type == LXB_DOM_NODE_TYPE_TEXT; })
    | std::views::transform([](lxb_dom_node_t* n) noexcept {
        auto const data = lxb_dom_interface_character_data(n);
        return std::string_view{reinterpret_cast<const char*>(data->data.data), data->data.length};
      });

  auto first = true;
  return std::ranges::fold_left(text_nodes, std::string{}, [sep, &first](std::string acc, std::string_view sv) mutable {
    if (not first) {
      acc.append(sep);
    }
    acc.append(sv);
    first = false;
    return acc;
  });
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

  for (auto* p = lxb_dom_element_first_attribute(element); p != nullptr; p = lxb_dom_element_next_attribute(p)) {
    auto attr_name_len = size_t{};
    auto const attr_name_data = lxb_dom_attr_qualified_name(p, &attr_name_len);
    if (attr_name_data == nullptr or std::string_view{reinterpret_cast<const char*>(attr_name_data), attr_name_len} != attr_name) {
      continue;
    }

    auto const attr_val_data = lxb_dom_attr_value(p, &attr_val_len);
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

/**
 * @brief Lexbor のタグ ID を可読なタグ名へ変換します。
 *
 * @param tag_id 変換対象のタグ ID です。
 * @return auto 対応するタグ名を返します。未知の場合は空文字列ビューを返します。
 */
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

namespace detail {

/**
 * @brief CSS パーサを破棄する deleter です。
 */
struct css_parser_deleter {
  /**
   * @brief CSS パーサを破棄します。
   *
  * @param p 破棄対象パーサです。
   */
  void operator()(lxb_css_parser_t* p) const noexcept { lxb_css_parser_destroy(p, true); }
};

/**
 * @brief CSS selector リストを破棄する deleter です。
 */
struct css_selectors_deleter {
  /**
   * @brief CSS selector リストを破棄します。
   *
  * @param l 破棄対象リストです。
   */
  void operator()(lxb_css_selector_list_t* l) const noexcept { lxb_css_selector_list_destroy_memory(l); }
};

/**
 * @brief Selector エンジンを破棄する deleter です。
 */
struct selectors_deleter {
  /**
   * @brief Selector エンジンを破棄します。
   *
  * @param s 破棄対象エンジンです。
   */
  void operator()(lxb_selectors_t* s) const noexcept { lxb_selectors_destroy(s, true); }
};

/**
 * @brief HTML シリアライズ結果を文字列へ追記するコールバックです。
 *
 * @param data 追記する文字列データです。
 * @param len データ長です。
 * @param ctx `std::string*` を指すコンテキストです。
 * @return lxb_status_t 常に `LXB_STATUS_OK` を返します。
 */
inline auto serialize_callback(const lxb_char_t* data, size_t len, void* ctx) noexcept -> lxb_status_t {
  auto* const str = static_cast<std::string*>(ctx);
  str->append(reinterpret_cast<const char*>(data), len);
  return LXB_STATUS_OK;
}

} // namespace detail

/**
 * @brief ノードの外部 HTML（自身を含む）を取得します。
 */
[[nodiscard]] auto inline outer_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(128);
  lxb_html_serialize_tree_cb(const_cast<lxb_dom_node_t*>(node), detail::serialize_callback, &result);
  return result;
}

/**
 * @brief ノードの内部 HTML（子ノード群）を取得します。
 */
[[nodiscard]] auto inline inner_html(lxb_dom_node_t const* node) -> std::string {
  if (node == nullptr) return "";
  auto result = std::string{};
  result.reserve(128);
  for (auto* child = lxb_dom_node_first_child(const_cast<lxb_dom_node_t*>(node)); child != nullptr; child = lxb_dom_node_next(child)) {
    lxb_html_serialize_tree_cb(child, detail::serialize_callback, &result);
  }
  return result;
}

/**
 * @brief CSS セレクタにマッチする最初の要素を返します。
 */
[[nodiscard]] auto inline query_selector(lxb_dom_node_t* node, std::string_view selector) -> lxb_dom_node_t* {
  if (node == nullptr or selector.empty()) {
    return nullptr;
  }

  auto parser = lxb_css_parser_create();
  if (parser == nullptr) {
    return nullptr;
  }
  [[maybe_unused]] auto parser_guard = std::unique_ptr<lxb_css_parser_t, detail::css_parser_deleter>{parser};
  if (lxb_css_parser_init(parser, nullptr) != LXB_STATUS_OK) {
    return nullptr;
  }
  std::ignore = lxb_css_parser_selectors_init(parser);

  auto list = lxb_css_selectors_parse(parser, reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  if (list == nullptr) {
    return nullptr;
  }
  [[maybe_unused]] auto list_guard = std::unique_ptr<lxb_css_selector_list_t, detail::css_selectors_deleter>{list};

  auto selectors = lxb_selectors_create();
  if (selectors == nullptr) {
    return nullptr;
  }
  [[maybe_unused]] auto selectors_guard = std::unique_ptr<lxb_selectors_t, detail::selectors_deleter>{selectors};
  if (lxb_selectors_init(selectors) != LXB_STATUS_OK) {
    return nullptr;
  }
  lxb_selectors_opt_set(selectors, LXB_SELECTORS_OPT_MATCH_ROOT);

  lxb_dom_node_t* result = nullptr;
  auto const cb = [](lxb_dom_node_t* n, lxb_css_selector_specificity_t, void* ctx) -> lxb_status_t {
    *static_cast<lxb_dom_node_t**>(ctx) = n;
    return LXB_STATUS_STOP;
  };

  auto const status = lxb_selectors_find(selectors, node, list, cb, &result);
  if (status != LXB_STATUS_OK and status != LXB_STATUS_STOP) {
    return nullptr;
  }
  return result;
}

/**
 * @brief CSS セレクタにマッチするすべての要素を返します。
 */
[[nodiscard]] auto inline query_selector_all(lxb_dom_node_t* node, std::string_view selector) -> std::vector<lxb_dom_node_t*> {
  if (node == nullptr or selector.empty()) {
    return {};
  }

  auto parser = lxb_css_parser_create();
  if (parser == nullptr) {
    return {};
  }
  [[maybe_unused]] auto parser_guard = std::unique_ptr<lxb_css_parser_t, detail::css_parser_deleter>{parser};
  if (lxb_css_parser_init(parser, nullptr) != LXB_STATUS_OK) {
    return {};
  }
  std::ignore = lxb_css_parser_selectors_init(parser);

  auto list = lxb_css_selectors_parse(parser, reinterpret_cast<const lxb_char_t*>(selector.data()), selector.size());
  if (list == nullptr) {
    return {};
  }
  [[maybe_unused]] auto list_guard = std::unique_ptr<lxb_css_selector_list_t, detail::css_selectors_deleter>{list};

  auto selectors = lxb_selectors_create();
  if (selectors == nullptr) {
    return {};
  }
  [[maybe_unused]] auto selectors_guard = std::unique_ptr<lxb_selectors_t, detail::selectors_deleter>{selectors};
  if (lxb_selectors_init(selectors) != LXB_STATUS_OK) {
    return {};
  }
  lxb_selectors_opt_set(selectors, LXB_SELECTORS_OPT_MATCH_ROOT);

  std::vector<lxb_dom_node_t*> result;
  auto const cb = [](lxb_dom_node_t* n, [[maybe_unused]] lxb_css_selector_specificity_t spec, void* ctx) -> lxb_status_t {
    static_cast<std::vector<lxb_dom_node_t*>*>(ctx)->push_back(n);
    return LXB_STATUS_OK;
  };

  auto const status = lxb_selectors_find(selectors, node, list, cb, &result);
  if (status != LXB_STATUS_OK and status != LXB_STATUS_STOP) {
    return {};
  }
  return result;
}

/**
 * @brief 指定したタグ名のいずれかに一致するノードのみを通過させる Range アダプタです。
 *
 * @tparam Tags 対象タグです (例: LXB_TAG_TABLE, LXB_TAG_TR)。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::tag<LXB_TAG_TABLE>
 * lexborpp::node_walker(root) | lexborpp::tag<LXB_TAG_TABLE, LXB_TAG_TBODY>
 * @endcode
 */
template <lxb_tag_id_t... Tags>
inline constexpr auto tag = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return node != nullptr && ((node->local_name == Tags) || ...);
});

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
 * @brief selector 評価前の候補絞り込み方法を表します。
 */
enum class selector_prefilter_kind {
  none,
  simple,
};

/**
 * @brief selector 評価前に適用する絞り込み条件を保持します。
 */
struct selector_prefilter_spec {
  selector_prefilter_kind kind{selector_prefilter_kind::none};
  selector_simple_spec simple{};
};

/**
 * @brief selector group の結合パターンを分類します。
 */
enum class selector_group_shape {
  simple,
  descendant_chain,
  child_chain,
  adjacent_sibling_chain,
  following_sibling_chain,
  mixed,
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
  selector_prefilter_spec prefilter{};
  selector_group_shape shape{selector_group_shape::simple};
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
 * @brief simple selector の評価優先度を返します。
 *
 * @param simple 対象 simple selector です。
 * @return std::size_t 優先度を返します。値が小さいほど優先です。
 */
[[nodiscard]] constexpr auto simple_selector_priority(selector_simple_spec const& simple) noexcept -> std::size_t {
  switch (simple.kind) {
  case selector_simple_kind::id:
    return 0;
  case selector_simple_kind::type:
    return 1;
  case selector_simple_kind::class_name:
    return 2;
  case selector_simple_kind::attribute:
    return 3;
  case selector_simple_kind::universal:
    return 4;
  }

  return 4;
}

/**
 * @brief compound selector から前処理に適した simple selector を選びます。
 *
 * @tparam Max 配列容量です。
 * @param compound 対象 compound selector です。
 * @return selector_prefilter_spec 最適化用の prefilter 条件を返します。
 */
template <std::size_t Max>
[[nodiscard]] constexpr auto build_prefilter(selector_compound_spec<Max> const& compound) noexcept -> selector_prefilter_spec {
  auto best = selector_prefilter_spec{};
  auto best_priority = std::size_t{5};

  for (auto const index : std::views::iota(std::size_t{0}, compound.simple_count)) {
    auto const& simple = compound.simples[index];
    auto const priority = simple_selector_priority(simple);
    if (priority >= best_priority || simple.kind == selector_simple_kind::universal) {
      continue;
    }

    best = selector_prefilter_spec{
      .kind = selector_prefilter_kind::simple,
      .simple = simple,
    };
    best_priority = priority;
  }

  return best;
}

/**
 * @brief selector group の結合パターンを分類します。
 *
 * @tparam Max 配列容量です。
 * @param group 対象 selector group です。
 * @return selector_group_shape 分類結果を返します。
 */
template <std::size_t Max>
[[nodiscard]] constexpr auto classify_group_shape(selector_group_spec<Max> const& group) noexcept -> selector_group_shape {
  if (group.compound_count <= 1) {
    return selector_group_shape::simple;
  }

  auto const first_relation = group.compounds[1].relation;
  for (auto const index : std::views::iota(std::size_t{2}, group.compound_count)) {
    if (group.compounds[index].relation != first_relation) {
      return selector_group_shape::mixed;
    }
  }

  switch (first_relation) {
  case selector_combinator::descendant:
    return selector_group_shape::descendant_chain;
  case selector_combinator::child:
    return selector_group_shape::child_chain;
  case selector_combinator::adjacent_sibling:
    return selector_group_shape::adjacent_sibling_chain;
  case selector_combinator::following_sibling:
    return selector_group_shape::following_sibling_chain;
  }

  return selector_group_shape::mixed;
}

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
    group.prefilter = build_prefilter(group.compounds[group.compound_count - 1]);
    group.shape = classify_group_shape(group);
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

} // namespace detail

/**
 * @brief 指定した id 属性値を持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Id 対象の id 属性値です (例: "forecast-point-3h-today")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::id<"forecast-point-3h-today">
 * @endcode
 */
template <detail::fixed_string Id>
inline constexpr auto id = std::views::filter(
    [](lxb_dom_node_t* node) noexcept { return lexborpp::get_attr_value(node, "id") == Id.view(); });

/**
 * @brief 指定した class 属性値のいずれかを持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Classes 対象の class 属性値です (例: "section-wrap")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::clazz<"section-wrap">
 * lexborpp::node_walker(root) | lexborpp::clazz<"section-wrap", "section-wrap active">
 * @endcode
 */
template <detail::fixed_string... Classes>
inline constexpr auto clazz = std::views::filter([](lxb_dom_node_t* node) noexcept {
  auto const class_value = lexborpp::get_attr_value(node, "class");
  return class_value.has_value() && ((*class_value == Classes.view()) || ...);
});

/**
 * @brief 指定した属性名・属性値を持つノードのみを通過させる Range アダプタです。
 *
 * @tparam Attr 属性名です (例: "class")。
 * @tparam Value 属性値です (例: "section-wrap")。
 *
 * 使用例:
 * @code
 * lexborpp::node_walker(root) | lexborpp::attr<"class", "section-wrap">
 * @endcode
 */
template <detail::fixed_string Attr, detail::fixed_string Value>
inline constexpr auto attr = std::views::filter(
    [](lxb_dom_node_t* node) noexcept { return lexborpp::get_attr_value(node, Attr.view()) == Value.view(); });

/**
 * @brief NTTP で渡された CSS セレクタにマッチする最初の要素を返します。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector(lxb_dom_node_t* node) -> lxb_dom_node_t* {
  if constexpr (Selector.view().empty()) {
    return nullptr;
  } else {
    return detail::query_selector_impl<Selector>(node);
  }
}

/**
 * @brief NTTP で渡された CSS セレクタにマッチするすべての要素を返します。
 */
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all(lxb_dom_node_t* node) -> std::vector<lxb_dom_node_t*> {
  if constexpr (Selector.view().empty()) {
    return {};
  } else {
    return detail::query_selector_all_impl<Selector>(node);
  }
}

} // namespace lexborpp

namespace std::ranges {

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_sibling_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::node_prev_sibling_walker> = true;

template <>
inline constexpr bool enable_borrowed_range<lexborpp::attr_walker> = true;

}  // namespace std::ranges

#endif  // LEXBORPP_HPP_
