/**
 * @file test/smoke_nttp.cpp
 * @brief NTTP 版セレクタの hosted 向けスモーク (例外あり)。
 */
#include <cstdio>

#include "lexborpp.hpp"

auto main() -> int {
  auto constexpr html =
      "<!doctype html><div id=\"root\">"
      "<li id=\"a\" class=\"item\">first</li>"
      "<li id=\"b\" class=\"item featured\">second</li></div>";
  auto doc = lexborpp::parse_html(html);
  if (!doc.has_value()) {
    std::printf("parse failed\n");
    return 1;
  }
  auto* root = lexborpp::get_root(doc.value());
  auto* nf = lexborpp::query_selector<"li.featured">(root);
  if (nf == nullptr) {
    std::printf("NTTP not found\n");
    return 1;
  }
  std::printf("NTTP found: %s\n", lexborpp::get_deep_text(nf).c_str());
  auto nitems = lexborpp::query_selector_all<"li.item">(root);
  std::printf("NTTP items: %zu\n", nitems.size());
  return nitems.size() == 2 ? 0 : 1;
}
