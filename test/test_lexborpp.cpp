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

static_assert(ranges_compatible_walker<lexborpp::node_walker>);
static_assert(ranges_compatible_walker<lexborpp::node_sibling_walker>);
static_assert(ranges_compatible_walker<lexborpp::attr_walker>);

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
    auto const id = lexborpp::get_attr_value(node, "id");
    result.emplace_back(id ? *id : std::string_view{});
  }

  return result;
}

template <typename Range>
auto collect_attr_names(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto* attr : range) {
    result.emplace_back(lexborpp::to_name_string(attr));
  }

  return result;
}

template <typename Range>
auto collect_attr_values(Range&& range) -> std::vector<std::string> {
  auto result = std::vector<std::string>{};

  for (auto* attr : range) {
    result.emplace_back(lexborpp::to_string(attr));
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
  if (node == nullptr || lexborpp::is_non_element_node(node)) {
    return nullptr;
  }

  for (auto* attr = lxb_dom_element_first_attribute(lxb_dom_interface_element(node));
       attr != nullptr; attr = lxb_dom_element_next_attribute(attr)) {
    if (lexborpp::to_name_string(attr) == name) {
      return attr;
    }
  }

  return nullptr;
}

}  // namespace

TEST_CASE("parse_html and document_ptr handle lifecycle safely") {
  SECTION("Valid HTML parsing succeeds") {
    auto doc_expected = lexborpp::parse_html(kHtml);
    REQUIRE(doc_expected.has_value());

    auto const& doc = doc_expected.value();
    auto* const root = lexborpp::get_root(doc);
    REQUIRE(root != nullptr);

    // Verify interaction with existing API
    auto* const leaf_b = lexborpp::get_element_by_id(root, "leaf-b");
    REQUIRE(leaf_b != nullptr);
    REQUIRE(lexborpp::get_attr_value(leaf_b, "id") == "leaf-b");
  }

  SECTION("Empty HTML parsing is handled") {
    auto doc_expected = lexborpp::parse_html("");
    REQUIRE(doc_expected.has_value()); // Lexbor usually succeeds with empty input

    auto const& doc = doc_expected.value();
    auto* const root = lexborpp::get_root(doc);
    REQUIRE(root != nullptr);
  }

  SECTION("document_ptr lifecycle") {
    // This is more of a compile-time check for deleter,
    // but ensures no crash on destruction.
    {
      auto doc_expected = lexborpp::parse_html("<div></div>");
      REQUIRE(doc_expected.has_value());
    } // doc destroyed here
    SUCCEED("Destruction completed without crash");
  }
}

TEST_CASE("lexborpp CSS selector and manipulation functions") {
  auto const html = R"HTML(<div id="root"><div id="content" class="entry"><p>First</p><p class="target">Second</p><span><b>Bold</b></span></div></div>)HTML";
  auto doc_expected = lexborpp::parse_html(html);
  REQUIRE(doc_expected.has_value());
  auto* root = lexborpp::get_root(doc_expected.value());

  SECTION("CSS selectors (query_selector)") {
    auto* content = lexborpp::query_selector(root, "div#content");
    REQUIRE(content != nullptr);
    REQUIRE(lexborpp::get_attr_value(content, "id") == "content");

    auto* entry = lexborpp::query_selector(root, "div.entry");
    REQUIRE(entry == content);

    auto* bold = lexborpp::query_selector(root, "span > b");
    REQUIRE(bold != nullptr);
    REQUIRE(lexborpp::get_deep_text(bold) == "Bold");

    REQUIRE(lexborpp::query_selector(root, "non-existent") == nullptr);
    REQUIRE(lexborpp::query_selector(root, "invalid[@selector") == nullptr);
    REQUIRE(lexborpp::query_selector(nullptr, "div") == nullptr);
  }

  SECTION("CSS selectors (query_selector_all)") {
    auto ps = lexborpp::query_selector_all(root, "p");
    REQUIRE(ps.size() == 2);
    REQUIRE(lexborpp::get_deep_text(ps[0]) == "First");
    REQUIRE(lexborpp::get_deep_text(ps[1]) == "Second");

    REQUIRE(lexborpp::query_selector_all(root, "h1").empty());
  }

  SECTION("DOM manipulation (attributes)") {
    auto* content = lexborpp::query_selector(root, "#content");
    auto* element = lexborpp::as_element(content);

    REQUIRE(lexborpp::set_attr(element, "data-test", "value"));
    REQUIRE(lexborpp::get_attr_value(content, "data-test") == "value");

    REQUIRE(lexborpp::set_attr(element, "class", "new-class"));
    REQUIRE(lexborpp::has_class(content, "new-class"));

    REQUIRE(lexborpp::remove_attr(element, "data-test"));
    REQUIRE_FALSE(lexborpp::get_attr_value(content, "data-test").has_value());

    REQUIRE_FALSE(lexborpp::remove_attr(nullptr, "id"));
  }

  SECTION("DOM manipulation (text content)") {
    auto* p = lexborpp::query_selector(root, "p.target");
    REQUIRE(lexborpp::set_text_content(p, "New Text"));
    REQUIRE(lexborpp::get_deep_text(p) == "New Text");

    REQUIRE(lexborpp::set_text_content(p, ""));
    REQUIRE(lexborpp::get_deep_text(p) == "");
  }

  SECTION("Serialization") {
    auto* bold = lexborpp::query_selector(root, "b");
    REQUIRE(lexborpp::outer_html(bold) == "<b>Bold</b>");
    REQUIRE(lexborpp::inner_html(bold) == "Bold");

    auto* content = lexborpp::query_selector(root, "#content");
    lexborpp::set_attr(lexborpp::as_element(content), "title", "hello");
    auto const html_str = lexborpp::outer_html(content);
    REQUIRE(html_str.find("title=\"hello\"") != std::string::npos);
  }
}

TEST_CASE("node_prev_sibling_walker traverses backwards") {
  auto const html = "<ul><li id='a'>A</li><li id='b'>B</li><li id='c'>C</li></ul>";
  auto doc_expected = lexborpp::parse_html(html);
  auto* root = lexborpp::get_root(doc_expected.value());

  auto* c = lexborpp::query_selector(root, "#c");
  REQUIRE(c != nullptr);

  auto walker = lexborpp::node_prev_sibling_walker{c};
  auto ids = std::vector<std::string>{};
  for (auto* node : walker) {
    if (auto id = lexborpp::get_attr_value(node, "id")) {
      ids.push_back(std::string{*id});
    }
  }

  REQUIRE(ids == std::vector<std::string>{"b", "a"});
  REQUIRE(std::ranges::distance(walker) == 2);

  auto* a = lexborpp::query_selector(root, "#a");
  REQUIRE(std::ranges::distance(lexborpp::node_prev_sibling_walker{a}) == 0);

  // Filter test
  auto filtered = lexborpp::node_prev_sibling_walker{c}
    | std::views::filter([](lxb_dom_node_t* n) {
        return lexborpp::get_attr_value(n, "id") == "a";
      });
  REQUIRE(std::ranges::distance(filtered) == 1);
}

TEST_CASE("lexborpp lookup helpers find expected nodes") {
  auto fixture = html_document_fixture{kHtml};

  auto* document = fixture.document_node();
  auto* container = require_node(fixture, "container");
  auto* text_only = require_node(fixture, "text-only");
  auto* class_target = require_node(fixture, "class-target");
  auto* class_second = require_node(fixture, "class-second");
  auto* leaf_b = require_node(fixture, "leaf-b");

  REQUIRE(lexborpp::is_non_element_node(nullptr));
  REQUIRE(lexborpp::is_non_element_node(document));
  REQUIRE_FALSE(lexborpp::is_non_element_node(container));

  auto* text_node = lxb_dom_node_first_child(text_only);
  REQUIRE(text_node != nullptr);
  REQUIRE(lexborpp::is_non_element_node(text_node));

  auto* comment_node = lxb_dom_node_next(text_node);
  REQUIRE(comment_node != nullptr);
  REQUIRE(lexborpp::is_non_element_node(comment_node));

  REQUIRE(lexborpp::get_element_by_id(document, "leaf-b") == leaf_b);
  REQUIRE(lexborpp::get_element_by_id(document, "missing") == nullptr);
  REQUIRE(lexborpp::get_element_by_id(nullptr, "leaf-b") == nullptr);

  // has_class tests
  REQUIRE(lexborpp::has_class(class_target, "match"));
  REQUIRE(lexborpp::has_class(class_target, "target-class"));
  REQUIRE(lexborpp::has_class(class_target, {"match", "target-class"}));
  REQUIRE_FALSE(lexborpp::has_class(class_target, {"match", "missing"}));
  REQUIRE_FALSE(lexborpp::has_class(class_target, "mat"));         // partial match
  REQUIRE_FALSE(lexborpp::has_class(class_target, "target-clas")); // partial match
  REQUIRE_FALSE(lexborpp::has_class(class_target, ""));            // empty class
  REQUIRE_FALSE(lexborpp::has_class(container, "match"));
  REQUIRE_FALSE(lexborpp::has_class(nullptr, "match"));

  // Multi-class attribute with different whitespaces
  auto* attrs_node = require_node(fixture, "attrs");
  REQUIRE(lexborpp::has_class(attrs_node, "alpha"));
  REQUIRE(lexborpp::has_class(attrs_node, "beta"));

  // get_first_element_by_class tests (EXACT MATCH)
  REQUIRE(lexborpp::get_first_element_by_class(document, "match") == class_second);
  REQUIRE(lexborpp::get_first_element_by_class(document, "target-class") == nullptr);
  REQUIRE(lexborpp::get_first_element_by_class(document, "match target-class") == class_target);
  REQUIRE(lexborpp::get_first_element_by_class(nullptr, "match") == nullptr);

  // get_elements_by_class tests (TOKEN MATCH)
  auto const matches = lexborpp::get_elements_by_class(document, "match");
  REQUIRE(matches.size() == 2);
  REQUIRE(matches[0] == class_target);
  REQUIRE(matches[1] == class_second);

  auto const alpha_matches = lexborpp::get_elements_by_class(document, "alpha");
  REQUIRE(alpha_matches.size() == 2);
  REQUIRE(alpha_matches[0] == container);
  REQUIRE(alpha_matches[1] == attrs_node);

  auto const missing_matches = lexborpp::get_elements_by_class(document, "non-existent");
  REQUIRE(missing_matches.empty());

  auto const null_matches = lexborpp::get_elements_by_class(nullptr, "match");
  REQUIRE(null_matches.empty());
}

TEST_CASE("lexborpp attribute and text helpers return direct content") {
  auto fixture = html_document_fixture{kHtml};

  auto* attrs = require_node(fixture, "attrs");
  auto* mixed = require_node(fixture, "mixed");
  auto* text_only = require_node(fixture, "text-only");
  auto* nested_only = require_node(fixture, "nested-only");

  auto const role = lexborpp::get_attr_value(attrs, "data-role");
  REQUIRE(role.has_value());
  REQUIRE(*role == "main");
  REQUIRE_FALSE(lexborpp::get_attr_value(attrs, "missing").has_value());

  auto* text_node = lxb_dom_node_first_child(text_only);
  REQUIRE(text_node != nullptr);
  REQUIRE_FALSE(lexborpp::get_attr_value(text_node, "id").has_value());
  REQUIRE_FALSE(lexborpp::get_attr_value(nullptr, "id").has_value());

  auto const first_text = lexborpp::get_first_child_text(mixed);
  REQUIRE(first_text.has_value());
  REQUIRE(*first_text == "left");
  REQUIRE_FALSE(lexborpp::get_first_child_text(nested_only).has_value());

  auto const text_only_all = lexborpp::get_all_children_text(text_only);
  REQUIRE(text_only_all.has_value());
  REQUIRE(*text_only_all == "firstsecond");

  auto const mixed_joined = lexborpp::get_all_children_text(mixed, "|");
  REQUIRE(mixed_joined.has_value());
  REQUIRE(*mixed_joined == "left|right");

  REQUIRE_FALSE(lexborpp::get_all_children_text(nested_only).has_value());
  REQUIRE_FALSE(lexborpp::get_all_children_text(nullptr).has_value());
}

TEST_CASE("lexborpp predicate helpers traverse in expected order") {
  auto fixture = html_document_fixture{kHtml};

  auto matcher = [](lxb_dom_node_t* node, std::string_view expected_id) noexcept {
    auto const id = lexborpp::get_attr_value(node, "id");
    return id.has_value() && *id == expected_id;
  };

  auto* leaf_b = require_node(fixture, "leaf-b");
  REQUIRE(lexborpp::get_following_element_by_op(fixture.body_node(), [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "leaf-b");
          }) == leaf_b);

  auto* branch_a = require_node(fixture, "branch-a");
  auto* branch_b = require_node(fixture, "branch-b");
  REQUIRE(lexborpp::get_sibling_element_by_op(branch_a, [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "branch-a");
          }) == branch_a);
  REQUIRE(lexborpp::get_sibling_element_by_op(branch_a, [&](lxb_dom_node_t* node) noexcept {
            return matcher(node, "branch-b");
          }) == branch_b);
  REQUIRE(lexborpp::get_sibling_element_by_op(nullptr, [&](lxb_dom_node_t*) noexcept {
            return true;
          }) == nullptr);
}

TEST_CASE("lexborpp string helpers expose node and attribute text") {
  auto fixture = html_document_fixture{kHtml};

  auto* text_node = lxb_dom_node_first_child(require_node(fixture, "text-only"));
  REQUIRE(text_node != nullptr);
  REQUIRE(lexborpp::to_string(text_node) == "first");
  REQUIRE(lexborpp::to_string(require_node(fixture, "attrs")).empty());

  auto* attr = find_attr(require_node(fixture, "attrs"), "data-role");
  REQUIRE(attr != nullptr);
  REQUIRE(lexborpp::to_name_string(attr) == "data-role");
  REQUIRE(lexborpp::to_string(attr) == "main");
  REQUIRE(lexborpp::to_name_string(nullptr).empty());
  REQUIRE(lexborpp::to_string(static_cast<lxb_dom_attr_t*>(nullptr)).empty());
}

TEST_CASE("get_deep_text collects all descendant text nodes") {
  SECTION("Direct text node") {
    auto const html = R"HTML(<div id="target">Text</div>)HTML";
    auto fixture = html_document_fixture{html};
    auto* node = fixture.by_id("target");
    auto* text_node = lxb_dom_node_first_child(node);
    REQUIRE(text_node != nullptr);
    REQUIRE(text_node->type == LXB_DOM_NODE_TYPE_TEXT);
    
    REQUIRE(lexborpp::get_deep_text(text_node) == "Text");
    REQUIRE(lexborpp::get_deep_text(text_node, "|") == "Text");
  }

  SECTION("Complex nested element") {
    auto const html = R"HTML(<p id="target">Hello <b>World</b></p>)HTML";
    auto fixture = html_document_fixture{html};
    auto* node = fixture.by_id("target");
    REQUIRE(node != nullptr);

    // Default: case A (native API)
    REQUIRE(lexborpp::get_deep_text(node) == "Hello World");
    // With separator: case B (node_walker)
    // In "<p>Hello <b>World</b></p>", nodes are: text("Hello "), b(text("World"))
    // get_deep_text(node, " ") -> "Hello " + " " + "World" = "Hello  World"
    REQUIRE(lexborpp::get_deep_text(node, " ") == "Hello  World");
  }

  SECTION("Separator test with simple nested") {
    auto const html = R"HTML(<p id="target">Hello<b>World</b></p>)HTML";
    auto fixture = html_document_fixture{html};
    auto* node = fixture.by_id("target");
    REQUIRE(lexborpp::get_deep_text(node) == "HelloWorld");
    REQUIRE(lexborpp::get_deep_text(node, " ") == "Hello World");
  }

  SECTION("Element with only text node") {
    auto const html = R"HTML(<div id="target">Only Text</div>)HTML";
    auto fixture = html_document_fixture{html};
    auto* node = fixture.by_id("target");
    REQUIRE(lexborpp::get_deep_text(node) == "Only Text");
    REQUIRE(lexborpp::get_deep_text(node, "-") == "Only Text");
  }

  SECTION("No text nodes") {
    auto const html = R"HTML(<div id="target"><span></span><p></p></div>)HTML";
    auto fixture = html_document_fixture{html};
    auto* node = fixture.by_id("target");
    REQUIRE(lexborpp::get_deep_text(node) == "");
    REQUIRE(lexborpp::get_deep_text(node, " ") == "");
  }

  SECTION("nullptr handling") {
    REQUIRE(lexborpp::get_deep_text(nullptr) == "");
    REQUIRE(lexborpp::get_deep_text(nullptr, " ") == "");
  }

  SECTION("Deeply nested nodes") {
    auto const deep_html = R"HTML(<div id="deep"><span>Hello</span> <span>World</span><p>!</p></div>)HTML";
    auto fixture = html_document_fixture{deep_html};
    auto* node = fixture.by_id("deep");
    REQUIRE(lexborpp::get_deep_text(node) == "Hello World!");
    REQUIRE(lexborpp::get_deep_text(node, "|") == "Hello| |World|!");
  }
}

TEST_CASE("node_walker traverses descendants depth first") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  REQUIRE(collect_ids(lexborpp::node_walker{tree}) ==
          std::vector<std::string>{"branch-a", "leaf-a", "branch-b", "leaf-b"});
  REQUIRE(collect_ids(lexborpp::node_walker{}) == std::vector<std::string>{});
}

TEST_CASE("sibling and attribute walkers preserve parser order") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  auto* first_child = lxb_dom_node_first_child(tree);
  REQUIRE(first_child != nullptr);
  REQUIRE(collect_ids(lexborpp::node_sibling_walker{first_child}) == std::vector<std::string>{"branch-a", "branch-b"});

  auto* attrs = require_node(fixture, "attrs");
  REQUIRE(collect_attr_names(lexborpp::attr_walker{attrs}) == std::vector<std::string>{"id", "class", "data-role", "title"});
  REQUIRE(collect_attr_values(lexborpp::attr_walker{attrs}) == std::vector<std::string>{"attrs", "alpha beta", "main", "Title"});

  auto* class_attr = find_attr(attrs, "class");
  REQUIRE(class_attr != nullptr);
  REQUIRE(collect_attr_names(lexborpp::attr_walker{class_attr}) == std::vector<std::string>{"class", "data-role", "title"});

  auto* text_node = lxb_dom_node_first_child(require_node(fixture, "text-only"));
  REQUIRE(text_node != nullptr);
  REQUIRE(collect_attr_names(lexborpp::attr_walker{text_node}) == std::vector<std::string>{});
}

TEST_CASE("tag_name returns representative tag names") {
  REQUIRE(lexborpp::tag_name(LXB_TAG_DIV) == "DIV");
  REQUIRE(lexborpp::tag_name(LXB_TAG__TEXT) == "TEXT");
  REQUIRE(lexborpp::tag_name(static_cast<lxb_tag_id_t>(999999)).empty());
}

TEST_CASE("walkers compose with std ranges adaptors") {
  auto fixture = html_document_fixture{kHtml};

  auto* tree = require_node(fixture, "tree");
  auto const descendants = lexborpp::node_walker{tree};
  auto const descendant_ids =
      collect_ids(descendants
                  | std::views::filter([](lxb_dom_node_t* node) noexcept {
                      return lxb_dom_node_tag_id(node) == LXB_TAG_ARTICLE;
                    }));
  REQUIRE(descendant_ids == std::vector<std::string>{"branch-a", "branch-b"});

  auto* first_child = lxb_dom_node_first_child(tree);
  REQUIRE(first_child != nullptr);

  auto const sibling_ids =
      collect_ids(lexborpp::node_sibling_walker{first_child}
                  | std::views::drop(1)
                  | std::views::take(1));
  REQUIRE(sibling_ids == std::vector<std::string>{"branch-b"});

  auto const attr_names = lexborpp::attr_walker{require_node(fixture, "attrs")};
  auto const attrs =
      collect_strings(attr_names
                      | std::views::drop(1)
                      | std::views::take(2)
                      | std::views::transform([](lxb_dom_attr_t* attr) noexcept {
                          return std::string_view{lexborpp::to_name_string(attr)};
                        }));
  REQUIRE(attrs == std::vector<std::string>{"class", "data-role"});

  auto const found = std::ranges::find_if(
      lexborpp::node_sibling_walker{first_child},
      [](lxb_dom_node_t* node) noexcept {
        auto const id = lexborpp::get_attr_value(node, "id");
        return id.has_value() && *id == "branch-b";
      });
  REQUIRE(found != std::ranges::end(lexborpp::node_sibling_walker{first_child}));
  REQUIRE(*found == require_node(fixture, "branch-b"));
}
