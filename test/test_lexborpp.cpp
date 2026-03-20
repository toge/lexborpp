#include <concepts>
#include <ranges>
#include "catch2/catch_all.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "test_support.hpp"

namespace {

template <typename Walker>
concept ranges_compatible_walker =
    std::ranges::forward_range<Walker> &&
    std::ranges::view<Walker> &&
    std::ranges::borrowed_range<Walker> &&
    std::ranges::forward_range<Walker const> &&
    std::ranges::borrowed_range<Walker const> &&
    std::ranges::viewable_range<Walker const>;

static_assert(ranges_compatible_walker<node_walker>);
static_assert(ranges_compatible_walker<node_sibling_walker>);
static_assert(ranges_compatible_walker<attr_walker>);

constexpr auto kHtml = R"HTML(<!doctype html><html><body id="body"><div id="container" class="alpha beta" data-role="main"><section id="tree"><article id="branch-a"><em id="leaf-a"></em></article><article id="branch-b"><strong id="leaf-b"></strong></article></section><div id="text-only">first<!--comment-->second</div><div id="mixed"><span>ignore</span>left<b>inner</b>right</div><div id="nested-only"><span>ignore</span></div><p id="class-target" class="match target-class"></p><p id="class-second" class="match"></p><div id="attrs" class="alpha beta" data-role="main" title="Title"></div></div></body></html>)HTML";

auto require_node(html_document_fixture const& fixture, std::string_view id) -> lxb_dom_node_t* {
  auto* node = fixture.by_id(id);
  REQUIRE(node != nullptr);
  return node;
}

template <typename Range>
auto collect_ids(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto* node : range) {
    auto const id = get_attr_value(node, "id");
    result.emplace_back(id ? *id : std::string_view{});
  }

  return result;
}

template <typename Range>
auto collect_attr_names(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto* attr : range) {
    result.emplace_back(to_name_string(attr));
  }

  return result;
}

template <typename Range>
auto collect_attr_values(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto* attr : range) {
    result.emplace_back(to_string(attr));
  }

  return result;
}

template <typename Range>
auto collect_strings(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto value : range) {
    result.emplace_back(value);
  }

  return result;
}

auto find_attr(lxb_dom_node_t* node, std::string_view name) noexcept -> lxb_dom_attr_t* {
  if (node == nullptr || is_non_element_node(node)) {
    return nullptr;
  }

  for (auto* attr = lxb_dom_element_first_attribute(lxb_dom_interface_element(node));
       attr != nullptr; attr = lxb_dom_element_next_attribute(attr)) {
    if (to_name_string(attr) == name) {
      return attr;
    }
  }

  return nullptr;
}

}  // namespace

TEST_CASE("lexborpp lookup helpers find expected nodes") {
  auto fixture = html_document_fixture{kHtml};

  auto* document = fixture.document_node();
  auto* container = require_node(fixture, "container");
  auto* text_only = require_node(fixture, "text-only");
  auto* class_target = require_node(fixture, "class-target");
  auto* class_second = require_node(fixture, "class-second");
  auto* leaf_b = require_node(fixture, "leaf-b");

  REQUIRE(is_non_element_node(nullptr));
  REQUIRE(is_non_element_node(document));
  REQUIRE_FALSE(is_non_element_node(container));

  auto* text_node = lxb_dom_node_first_child(text_only);
  REQUIRE(text_node != nullptr);
  REQUIRE(is_non_element_node(text_node));

  auto* comment_node = lxb_dom_node_next(text_node);
  REQUIRE(comment_node != nullptr);
  REQUIRE(is_non_element_node(comment_node));

  REQUIRE(get_element_by_id(document, "leaf-b") == leaf_b);
  REQUIRE(get_element_by_id(document, "missing") == nullptr);
  REQUIRE(get_element_by_id(nullptr, "leaf-b") == nullptr);

  REQUIRE(get_first_element_by_class(document, "match target-class") == class_target);
  REQUIRE(get_first_element_by_class(document, "match") == class_second);
  REQUIRE(get_first_element_by_class(document, "target-class") == nullptr);
  REQUIRE(get_first_element_by_class(nullptr, "match") == nullptr);
}

TEST_CASE("lexborpp attribute and text helpers return direct content") {
  auto fixture = html_document_fixture{kHtml};

  auto* attrs = require_node(fixture, "attrs");
  auto* mixed = require_node(fixture, "mixed");
  auto* text_only = require_node(fixture, "text-only");
  auto* nested_only = require_node(fixture, "nested-only");

  auto const role = get_attr_value(attrs, "data-role");
  REQUIRE(role.has_value());
  REQUIRE(*role == "main");
  REQUIRE_FALSE(get_attr_value(attrs, "missing").has_value());

  auto* text_node = lxb_dom_node_first_child(text_only);
  REQUIRE(text_node != nullptr);
  REQUIRE_FALSE(get_attr_value(text_node, "id").has_value());
  REQUIRE_FALSE(get_attr_value(nullptr, "id").has_value());

  auto const first_text = get_first_child_text(mixed);
  REQUIRE(first_text.has_value());
  REQUIRE(*first_text == "left");
  REQUIRE_FALSE(get_first_child_text(nested_only).has_value());

  auto const text_only_all = get_all_children_text(text_only);
  REQUIRE(text_only_all.has_value());
  REQUIRE(*text_only_all == "firstsecond");

  auto const mixed_joined = get_all_children_text(mixed, "|");
  REQUIRE(mixed_joined.has_value());
  REQUIRE(*mixed_joined == "left|right");

  REQUIRE_FALSE(get_all_children_text(nested_only).has_value());
  REQUIRE_FALSE(get_all_children_text(nullptr).has_value());
}

TEST_CASE("lexborpp predicate helpers traverse in expected order") {
  auto fixture = html_document_fixture{kHtml};

  auto matcher = [](lxb_dom_node_t* node, std::string_view expected_id) noexcept {
    auto const id = get_attr_value(node, "id");
    return id.has_value() && *id == expected_id;
  };

  auto* leaf_b = require_node(fixture, "leaf-b");
  REQUIRE(get_following_element_by_op(fixture.body_node(), [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "leaf-b");
          }) == leaf_b);

  auto* branch_a = require_node(fixture, "branch-a");
  auto* branch_b = require_node(fixture, "branch-b");
  REQUIRE(get_sibling_element_by_op(branch_a, [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "branch-a");
          }) == branch_a);
  REQUIRE(get_sibling_element_by_op(branch_a, [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "branch-b");
          }) == branch_b);
  REQUIRE(get_sibling_element_by_op(nullptr, [&](lxb_dom_node_t*) noexcept {
            return true;
          }) == nullptr);
}

TEST_CASE("lexborpp string helpers expose node and attribute text") {
  auto fixture = html_document_fixture{kHtml};

  auto* text_node = lxb_dom_node_first_child(require_node(fixture, "text-only"));
  REQUIRE(text_node != nullptr);
  REQUIRE(to_string(text_node) == "first");
  REQUIRE(to_string(require_node(fixture, "attrs")).empty());

  auto* attr = find_attr(require_node(fixture, "attrs"), "data-role");
  REQUIRE(attr != nullptr);
  REQUIRE(to_name_string(attr) == "data-role");
  REQUIRE(to_string(attr) == "main");
  REQUIRE(to_name_string(nullptr).empty());
  REQUIRE(to_string(static_cast<lxb_dom_attr_t*>(nullptr)).empty());
}

TEST_CASE("node_walker traverses descendants depth first") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  REQUIRE(collect_ids(node_walker{tree}) ==
          std::vector<std::string>{"branch-a", "leaf-a", "branch-b", "leaf-b"});
  REQUIRE(collect_ids(node_walker{}) == std::vector<std::string>{});
}

TEST_CASE("sibling and attribute walkers preserve parser order") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  auto* first_child = lxb_dom_node_first_child(tree);
  REQUIRE(first_child != nullptr);
  REQUIRE(collect_ids(node_sibling_walker{first_child}) == std::vector<std::string>{"branch-a", "branch-b"});

  auto* attrs = require_node(fixture, "attrs");
  REQUIRE(collect_attr_names(attr_walker{attrs}) == std::vector<std::string>{"id", "class", "data-role", "title"});
  REQUIRE(collect_attr_values(attr_walker{attrs}) == std::vector<std::string>{"attrs", "alpha beta", "main", "Title"});

  auto* class_attr = find_attr(attrs, "class");
  REQUIRE(class_attr != nullptr);
  REQUIRE(collect_attr_names(attr_walker{class_attr}) == std::vector<std::string>{"class", "data-role", "title"});

  auto* text_node = lxb_dom_node_first_child(require_node(fixture, "text-only"));
  REQUIRE(text_node != nullptr);
  REQUIRE(collect_attr_names(attr_walker{text_node}) == std::vector<std::string>{});
}

TEST_CASE("tag_name returns representative tag names") {
  REQUIRE(tag_name(LXB_TAG_DIV) == "DIV");
  REQUIRE(tag_name(LXB_TAG__TEXT) == "TEXT");
  REQUIRE(tag_name(static_cast<lxb_tag_id_t>(999999)).empty());
}

TEST_CASE("walkers compose with std ranges adaptors") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  auto const descendants = node_walker{tree};
  auto const descendant_ids =
      collect_ids(descendants
                  | std::views::filter([](lxb_dom_node_t* node) noexcept {
                      return lxb_dom_node_tag_id(node) == LXB_TAG_ARTICLE;
                    }));
  REQUIRE(descendant_ids == std::vector<std::string>{"branch-a", "branch-b"});

  auto* first_child = lxb_dom_node_first_child(tree);
  REQUIRE(first_child != nullptr);

  auto const sibling_ids =
      collect_ids(node_sibling_walker{first_child}
                  | std::views::drop(1)
                  | std::views::take(1));
  REQUIRE(sibling_ids == std::vector<std::string>{"branch-b"});

  auto const attr_names = attr_walker{require_node(fixture, "attrs")};
  auto const attrs =
      collect_strings(attr_names
                      | std::views::drop(1)
                      | std::views::take(2)
                      | std::views::transform([](lxb_dom_attr_t* attr) noexcept {
                          return std::string_view{to_name_string(attr)};
                        }));
  REQUIRE(attrs == std::vector<std::string>{"class", "data-role"});

  auto const found = std::ranges::find_if(
      node_sibling_walker{first_child},
      [](lxb_dom_node_t* node) noexcept {
        auto const id = get_attr_value(node, "id");
        return id.has_value() && *id == "branch-b";
      });
  REQUIRE(found != std::ranges::end(node_sibling_walker{first_child}));
  REQUIRE(*found == require_node(fixture, "branch-b"));
}
