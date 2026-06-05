#include <chrono>
#include <cstdio>
#include <string_view>
#include <vector>

#include <lexborpp.hpp>

namespace {

constexpr auto kIterations = 10000;

constexpr auto kHtml = R"HTML(<!doctype html>
<html>
<body id="body">
  <div id="container" class="alpha beta" data-role="main">
    <section id="tree">
      <article id="branch-a" class="card">
        <em id="leaf-a">Leaf A</em>
        <p class="intro">Paragraph A</p>
      </article>
      <article id="branch-b" class="card">
        <strong id="leaf-b">Leaf B</strong>
        <p class="content">Paragraph B</p>
      </article>
      <article id="branch-c" class="card">
        <span id="leaf-c">Leaf C</span>
        <p class="summary">Paragraph C</p>
      </article>
    </section>
    <div id="text-only">first<!--comment-->second</div>
    <div id="mixed"><span>ignore</span>left<b>inner</b>right</div>
    <div id="nested-only"><span>ignore</span></div>
    <p id="class-target" class="match target-class"></p>
    <p id="class-second" class="match"></p>
    <div id="attrs" class="alpha beta" data-role="main" title="Title"></div>
  </div>
</body>
</html>)HTML";

template <typename Func>
auto measure(Func&& fn, std::size_t iterations) -> double {
  auto const start = std::chrono::high_resolution_clock::now();
  for (auto i = std::size_t{0}; i < iterations; ++i) {
    volatile auto result = fn();
    (void)result;
  }
  auto const end = std::chrono::high_resolution_clock::now();
  auto const elapsed = std::chrono::duration<double, std::micro>(end - start).count();
  return elapsed / static_cast<double>(iterations);
}

}  // namespace

auto main() -> int {
  auto doc = lexborpp::parse_html(kHtml);
  if (!doc) {
    std::fprintf(stderr, "Failed to parse HTML\n");
    return 1;
  }
  auto* root = lexborpp::get_root(doc.value());

  // =========================================================================
  // query_selector (first match) benchmark
  // =========================================================================
  std::printf("=== query_selector (first match) ===\n");
  std::printf("HTML: %zu bytes, Iterations: %zu\n\n", std::string_view{kHtml}.size(), kIterations);

  std::printf("%-42s %10s %10s %10s %8s\n",
    "Selector", "NTTP", "Runtime", "Naive", "NTTP/Naive");
  std::printf("%-42s %10s %10s %10s %8s\n",
    "--------", "----", "-------", "-----", "----------");

  // --- #leaf-b ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"section#tree article.card strong#leaf-b">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "section#tree article.card strong#leaf-b");
    }, kIterations);

    // Naive: node_walker + filter by id
    auto naive = measure([&]() {
      for (auto* n : lexborpp::node_walker{root}) {
        if (lexborpp::get_attr_value(n, "id") == "leaf-b") return n;
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "#leaf-b (via tree)", nttp, runtime, naive, naive / nttp);
  }

  // --- article.card ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"article.card">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "article.card");
    }, kIterations);

    // Naive: node_walker + tag + class filter
    auto naive = measure([&]() {
      for (auto* n : lexborpp::node_walker{root}
                      | lexborpp::tag<LXB_TAG_ARTICLE>) {
        if (lexborpp::has_class(n, "card")) return n;
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "article.card", nttp, runtime, naive, naive / nttp);
  }

  // --- section#tree > article.card ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"section#tree > article.card">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "section#tree > article.card");
    }, kIterations);

    // Naive: find #tree, then walk its direct children
    auto naive = measure([&]() {
      for (auto* n : lexborpp::node_walker{root} | lexborpp::id<"tree">) {
        for (auto* child : lexborpp::node_sibling_walker{lxb_dom_node_first_child(n)}) {
          if (lxb_dom_node_tag_id(child) == LXB_TAG_ARTICLE && lexborpp::has_class(child, "card")) {
            return child;
          }
        }
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "section#tree > article.card", nttp, runtime, naive, naive / nttp);
  }

  // --- section#tree article.card p ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"section#tree article.card p">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "section#tree article.card p");
    }, kIterations);

    // Naive: nested loops — tree → article.card descendants → p
    auto naive = measure([&]() {
      for (auto* tree : lexborpp::node_walker{root} | lexborpp::id<"tree">) {
        for (auto* article : lexborpp::node_walker{tree} | lexborpp::tag<LXB_TAG_ARTICLE>) {
          if (!lexborpp::has_class(article, "card")) continue;
          for (auto* p : lexborpp::node_walker{article} | lexborpp::tag<LXB_TAG_P>) {
            return p;
          }
        }
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "section#tree article.card p", nttp, runtime, naive, naive / nttp);
  }

  // --- #branch-a + #branch-b (adjacent sibling) ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"article#branch-a + article#branch-b">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "article#branch-a + article#branch-b");
    }, kIterations);

    // Naive: find #branch-a, get next element sibling, check id
    auto naive = measure([&]() {
      for (auto* a : lexborpp::node_walker{root} | lexborpp::id<"branch-a">) {
        auto* next = a->next;
        while (next && lexborpp::is_non_element_node(next)) next = next->next;
        if (next && lexborpp::get_attr_value(next, "id") == "branch-b") return next;
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "article#a + article#b (adj sibling)", nttp, runtime, naive, naive / nttp);
  }

  // --- #branch-a ~ article (following sibling) ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"article#branch-a ~ article">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "article#branch-a ~ article");
    }, kIterations);

    // Naive: find #branch-a, walk following siblings
    auto naive = measure([&]() {
      for (auto* a : lexborpp::node_walker{root} | lexborpp::id<"branch-a">) {
        for (auto* sib = a->next; sib; sib = sib->next) {
          if (!lexborpp::is_non_element_node(sib) && lxb_dom_node_tag_id(sib) == LXB_TAG_ARTICLE) {
            return sib;
          }
        }
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "article#a ~ article (follow sibling)", nttp, runtime, naive, naive / nttp);
  }

  // --- div[data-role=main] ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"div[data-role=main]">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "div[data-role=main]");
    }, kIterations);

    // Naive: tag + attr filter
    auto naive = measure([&]() {
      for (auto* n : lexborpp::node_walker{root}
                      | lexborpp::tag<LXB_TAG_DIV>
                      | lexborpp::attr<"data-role", "main">) {
        return n;
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "div[data-role=main]", nttp, runtime, naive, naive / nttp);
  }

  // --- p.match ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"p.match">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "p.match");
    }, kIterations);

    // Naive: tag + class filter
    auto naive = measure([&]() {
      for (auto* n : lexborpp::node_walker{root}
                      | lexborpp::tag<LXB_TAG_P>) {
        if (lexborpp::has_class(n, "match")) return n;
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "p.match", nttp, runtime, naive, naive / nttp);
  }

  // --- p, span > b (comma group) ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector<"p, span > b">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector(root, "p, span > b");
    }, kIterations);

    // Naive: find first p, OR find span > b
    auto naive = measure([&]() {
      // Try p first
      for (auto* n : lexborpp::node_walker{root} | lexborpp::tag<LXB_TAG_P>) {
        return n;
      }
      // Then try span > b
      for (auto* span : lexborpp::node_walker{root} | lexborpp::tag<LXB_TAG_SPAN>) {
        for (auto* child = lxb_dom_node_first_child(span); child; child = child->next) {
          if (!lexborpp::is_non_element_node(child) && lxb_dom_node_tag_id(child) == LXB_TAG_B) {
            return child;
          }
        }
      }
      return static_cast<lxb_dom_node_t*>(nullptr);
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "p, span > b (group)", nttp, runtime, naive, naive / nttp);
  }

  // =========================================================================
  // query_selector_all benchmark
  // =========================================================================
  std::printf("\n=== query_selector_all (all matches) ===\n\n");

  std::printf("%-42s %10s %10s %10s %8s\n",
    "Selector", "NTTP", "Runtime", "Naive", "NTTP/Naive");
  std::printf("%-42s %10s %10s %10s %8s\n",
    "--------", "----", "-------", "-----", "----------");

  // --- All article.card ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector_all<"article.card">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector_all(root, "article.card");
    }, kIterations);

    auto naive = measure([&]() {
      auto result = std::vector<lxb_dom_node_t*>{};
      for (auto* n : lexborpp::node_walker{root}
                      | lexborpp::tag<LXB_TAG_ARTICLE>) {
        if (lexborpp::has_class(n, "card")) result.push_back(n);
      }
      return result;
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "article.card", nttp, runtime, naive, naive / nttp);
  }

  // --- All section#tree article.card p ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector_all<"section#tree article.card p">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector_all(root, "section#tree article.card p");
    }, kIterations);

    auto naive = measure([&]() {
      auto result = std::vector<lxb_dom_node_t*>{};
      for (auto* tree : lexborpp::node_walker{root} | lexborpp::id<"tree">) {
        for (auto* article : lexborpp::node_walker{tree} | lexborpp::tag<LXB_TAG_ARTICLE>) {
          if (!lexborpp::has_class(article, "card")) continue;
          for (auto* p : lexborpp::node_walker{article} | lexborpp::tag<LXB_TAG_P>) {
            result.push_back(p);
          }
        }
      }
      return result;
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "section#tree article.card p", nttp, runtime, naive, naive / nttp);
  }

  // --- All p, span > b (comma group) ---
  {
    auto nttp = measure([&]() {
      return lexborpp::query_selector_all<"p, span > b">(root);
    }, kIterations);

    auto runtime = measure([&]() {
      return lexborpp::query_selector_all(root, "p, span > b");
    }, kIterations);

    auto naive = measure([&]() {
      auto result = std::vector<lxb_dom_node_t*>{};
      for (auto* n : lexborpp::node_walker{root} | lexborpp::tag<LXB_TAG_P>) {
        result.push_back(n);
      }
      for (auto* span : lexborpp::node_walker{root} | lexborpp::tag<LXB_TAG_SPAN>) {
        for (auto* child = lxb_dom_node_first_child(span); child; child = child->next) {
          if (!lexborpp::is_non_element_node(child) && lxb_dom_node_tag_id(child) == LXB_TAG_B) {
            result.push_back(child);
          }
        }
      }
      return result;
    }, kIterations);

    std::printf("%-42s %9.3f %9.3f %9.3f %7.1fx\n",
      "p, span > b (group)", nttp, runtime, naive, naive / nttp);
  }

  std::printf("\nDone.\n");
  return 0;
}
