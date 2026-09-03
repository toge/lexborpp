/**
 * @file test/smoke_wasi_minimal.cpp
 * @brief LEXBORPP_WASI_MINIMAL モードの検証。
 *
 * -fno-exceptions 付きでビルドされる。LEXBORPP_THROW を使う NTTP パーサを
 * 含む全モジュール (parse/query/DOM 編集/walker/range アダプタ) が
 * 例外なしでコンパイル・実行できることを確認する。
 */
#include <cstdio>

#include "lexborpp.hpp"

#define CHECK(cond, label)                                                     \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("NG: %s\n", label);                                          \
      return 1;                                                                \
    }                                                                          \
    std::printf("OK: %s\n", label);                                            \
  } while (0)

auto main() -> int {
  auto constexpr html =
      "<!doctype html><div id=\"root\"><ul class=\"items\">"
      "<li id=\"a\" class=\"item\">first</li>"
      "<li id=\"b\" class=\"item featured\">second</li>"
      "</ul></div>";
  auto doc = lexborpp::parse_html(html);
  CHECK(doc.has_value(), "parse_html");
  auto* root = lexborpp::get_root(doc.value());

  // runtime selector
  auto* featured = lexborpp::query_selector(root, "li.featured");
  CHECK(featured != nullptr, "runtime query_selector");
  CHECK(lexborpp::get_deep_text(featured) == "second", "get_deep_text");
  auto items = lexborpp::query_selector_all(root, "li.item");
  CHECK(items.size() == 2, "runtime query_selector_all");

  // NTTP selector (例外なしでもコンパイル・実行できること)
  auto* nfeatured = lexborpp::query_selector<"li.featured">(root);
  CHECK(nfeatured != nullptr, "NTTP query_selector");
  auto nitems = lexborpp::query_selector_all<"li.item">(root);
  CHECK(nitems.size() == 2, "NTTP query_selector_all");

  // id lookup + index
  CHECK(lexborpp::get_element_by_id(root, "a") != nullptr, "get_element_by_id");
  lexborpp::document_id_index idx(root);
  CHECK(idx.find("b") != nullptr, "document_id_index::find");

  // DOM edit + serialize
  auto* el = lexborpp::as_element(featured);
  CHECK(lexborpp::set_attr(el, "data-state", "ready"), "set_attr");
  CHECK(lexborpp::get_attr_value(featured, "data-state") == "ready", "get_attr_value");
  auto outer = lexborpp::outer_html(featured);
  CHECK(outer.find("data-state=\"ready\"") != std::string::npos, "outer_html");
  CHECK(lexborpp::remove_attr(el, "data-state"), "remove_attr");
  CHECK(!lexborpp::get_attr_value(featured, "data-state").has_value(), "attr removed");

  // walkers + range adapters
  std::size_t count = 0;
  for ([[maybe_unused]] auto* n : lexborpp::node_walker(root)) {
    ++count;
  }
  CHECK(count > 0, "node_walker");
  std::size_t li_count = 0;
  for ([[maybe_unused]] auto* n : lexborpp::node_walker(root) | lexborpp::tag<LXB_TAG_LI>) {
    ++li_count;
  }
  CHECK(li_count == 2, "tag range adapter");

  std::printf("ALL PASS\n");
  return 0;
}
