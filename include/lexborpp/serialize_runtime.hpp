#ifndef LEXBORPP_SERIALIZE_RUNTIME_HPP_
#define LEXBORPP_SERIALIZE_RUNTIME_HPP_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "lexbor/html/serialize.h"
#include "lexbor/css/css.h"
#include "lexbor/selectors/selectors.h"
#include "lexbor/dom/dom.h"

namespace lexborpp {
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
}  // namespace lexborpp

#endif  // LEXBORPP_SERIALIZE_RUNTIME_HPP_
