# NTTP CSS Selector Template JIT Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the AST-driven NTTP CSS selector evaluation with recursive template instantiation that generates specialized matching code per selector at compile time.

**Architecture:** The current design parses CSS selectors at compile time into an AST (`selector_spec`) and then interprets that AST at runtime with loops and function pointers. The new design keeps the compile-time parser but replaces the runtime interpreter with recursive `constexpr` template functions. Each `query_selector<"div#content > p">(node)` call instantiates a unique matching function where the selector chain, combinators, and simple-match checks are all compile-time constants, enabling full inlining and constant propagation by the compiler.

**Tech Stack:** C++23/26, Lexbor C API, template metaprogramming (`constexpr`, `if constexpr`, recursive template instantiation)

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `include/lexborpp/nttp_parser.hpp` | Modify | Remove `prefilter`/`shape` classification; keep parser + simple/compound structs |
| `include/lexborpp/nttp_match.hpp` | Rewrite | Replace AST interpreter with recursive template matching functions |
| `include/lexborpp/nttp_query.hpp` | Minor update | Update to use new matching API |
| `test/test_lexborpp.cpp` | Update | Remove static assertions for removed internal structures |

---

## Task 1: Simplify the parser (remove prefilter/shape classification)

**Files:**
- Modify: `include/lexborpp/nttp_parser.hpp`

Remove `selector_prefilter_kind`, `selector_prefilter_spec`, `selector_group_shape`, `classify_group_shape()`, `build_prefilter()`, `simple_selector_priority()`. Remove `prefilter` and `shape` fields from `selector_group_spec`. Remove `group.prefilter = ...` and `group.shape = ...` lines from `parse_selector_spec`.

- [ ] **Step 1: Remove prefilter-related types and functions**

Remove from `nttp_parser.hpp`:
- `selector_prefilter_kind` enum (lines 177-180)
- `selector_prefilter_spec` struct (lines 185-188)
- `selector_group_shape` enum (lines 193-200)
- `simple_selector_priority()` function (lines 238-253)
- `build_prefilter()` function (lines 262-282)
- `classify_group_shape()` function (lines 291-316)

Remove from `selector_group_spec`:
- `selector_prefilter_spec prefilter{};` field
- `selector_group_shape shape{selector_group_shape::simple};` field

Remove from `parse_selector_spec()`:
- `group.prefilter = build_prefilter(group.compounds[group.compound_count - 1]);`
- `group.shape = classify_group_shape(group);`

- [ ] **Step 2: Build and verify parser simplification compiles**

```bash
./build.sh && ./test.sh
```

Expected: compilation succeeds (static assertions in tests that reference removed fields will fail — fix in Task 4).

- [ ] **Step 3: Commit**

```bash
git add include/lexborpp/nttp_parser.hpp
git commit -m "refactor: remove prefilter/shape classification from NTTP parser"
```

---

## Task 2: Rewrite match.hpp with recursive template matching

**Files:**
- Rewrite: `include/lexborpp/nttp_match.hpp`

Replace all runtime AST-walking match functions with recursive template functions. The new architecture:

```
match_simple<Selector, I, SI>(node)  -- checks 1 simple selector
match_compound<Selector, I>(node)    -- ANDs all simples in compound I
match_compound_chain<Selector, I>(node, current) -- recursive right-to-left chain walker
match_group<Selector, GI>(node)      -- checks group GI (iterates compounds)
match_selector_compiled<Selector>(node) -- checks all groups
```

- [ ] **Step 1: Remove old match infrastructure**

Remove from `nttp_match.hpp`:
- `selector_group_cache_key` struct
- `selector_group_cache_key_hash` struct
- `selector_group_cache` typedef
- `selector_query_context` struct
- `create_query_context()` function
- `match_simple_selector()` function
- `match_compound_selector()` function
- `match_prefilter()` function
- `match_selector_group()` function
- `match_selector_group_by_shape()` function
- `match_selector()` function
- `match_selector_group_compiled()` function
- `match_selector_compiled_impl()` function

- [ ] **Step 2: Implement new recursive template matching**

Add the following functions to `nttp_match.hpp`:

```cpp
namespace detail {

// --- Match a single simple selector (compile-time kind dispatch) ---
template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI, std::size_t SimpleI>
[[nodiscard]] constexpr auto match_simple(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto& spec = compiled_selector_v<Selector>.groups[GI].compounds[CompoundI].simples[SimpleI];
  using kind = selector_simple_kind;

  if constexpr (spec.kind == kind::universal) {
    return not is_non_element_node(node);
  } else {
    if (node == nullptr || is_non_element_node(node)) {
      return false;
    }

    if constexpr (spec.kind == kind::type) {
      return iequals(node_qualified_name(node), spec.value);
    } else if constexpr (spec.kind == kind::id) {
      return get_attr_value(node, "id") == spec.value;
    } else if constexpr (spec.kind == kind::class_name) {
      return has_class(node, spec.value);
    } else if constexpr (spec.kind == kind::attribute) {
      auto const value = get_attr_value(node, spec.name);
      if (not value.has_value()) {
        return false;
      }
      return match_attribute(*value, spec.value, spec.attribute_match);
    }
  }
}

// --- Match all simple selectors in compound I (AND) ---
template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI, std::size_t... SimpleIs>
[[nodiscard]] constexpr auto match_compound_impl(
  lxb_dom_node_t* node,
  std::index_sequence<SimpleIs...>) noexcept -> bool {
  return (... && match_simple<Selector, GI, CompoundI, SimpleIs>(node));
}

template <detail::fixed_string Selector, std::size_t GI, std::size_t CompoundI>
[[nodiscard]] constexpr auto match_compound(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto simple_count = compiled_selector_v<Selector>.groups[GI].compounds[CompoundI].simple_count;
  return match_compound_impl<Selector, GI, CompoundI>(
    node, std::make_index_sequence<simple_count>{});
}

// --- Recursive right-to-left chain matching ---
// Base case: I == 0, just match this compound
template <detail::fixed_string Selector, std::size_t GI, std::size_t I = 0>
[[nodiscard]] constexpr auto match_compound_chain(
  lxb_dom_node_t* node) noexcept -> bool {
  return match_compound<Selector, GI, I>(node);
}

// Recursive case: match current, then follow combinator to parent
template <detail::fixed_string Selector, std::size_t GI, std::size_t I>
  requires (I > 0)
[[nodiscard]] constexpr auto match_compound_chain(
  lxb_dom_node_t* node) noexcept -> bool {
  if (not match_compound<Selector, GI, I>(node)) {
    return false;
  }

  constexpr auto relation = compiled_selector_v<Selector>.groups[GI].compounds[I].relation;

  if constexpr (relation == selector_combinator::child) {
    return match_compound_chain<Selector, GI, I - 1>(parent_element(node));
  } else if constexpr (relation == selector_combinator::adjacent_sibling) {
    return match_compound_chain<Selector, GI, I - 1>(prev_element_sibling(node));
  } else if constexpr (relation == selector_combinator::following_sibling) {
    for (auto* prev = prev_element_sibling(node); prev != nullptr; prev = prev_element_sibling(prev)) {
      if (match_compound_chain<Selector, GI, I - 1>(prev)) {
        return true;
      }
    }
    return false;
  } else {
    // descendant: walk up ancestors
    for (auto* ancestor = parent_element(node); ancestor != nullptr; ancestor = parent_element(ancestor)) {
      if (match_compound_chain<Selector, GI, I - 1>(ancestor)) {
        return true;
      }
    }
    return false;
  }
}

// --- Match a selector group (compound_count compounds) ---
template <detail::fixed_string Selector, std::size_t GI>
[[nodiscard]] constexpr auto match_group(
  lxb_dom_node_t* node) noexcept -> bool {
  constexpr auto compound_count = compiled_selector_v<Selector>.groups[GI].compound_count;
  if constexpr (compound_count == 0) {
    return false;
  } else {
    return match_compound_chain<Selector, GI, compound_count - 1>(node);
  }
}

// --- Match all selector groups (comma-separated) ---
template <detail::fixed_string Selector, std::size_t... GIs>
[[nodiscard]] constexpr auto match_all_groups(
  lxb_dom_node_t* node,
  std::index_sequence<GIs...>) noexcept -> bool {
  return (... || match_group<Selector, GIs>(node));
}

// --- Public entry point ---
template <detail::fixed_string Selector>
[[nodiscard]] constexpr auto match_selector_compiled(
  lxb_dom_node_t* node) noexcept -> bool {
  if (node == nullptr) {
    return false;
  }
  constexpr auto group_count = compiled_selector_v<Selector>.group_count;
  return match_all_groups<Selector>(
    node, std::make_index_sequence<group_count>{});
}

// --- Query implementations ---
template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_impl(lxb_dom_node_t* node) -> lxb_dom_node_t* {
  if (node == nullptr) {
    return nullptr;
  }
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current)) {
      return current;
    }
  }
  return nullptr;
}

template <detail::fixed_string Selector>
[[nodiscard]] auto inline query_selector_all_impl(lxb_dom_node_t* node) -> std::vector<lxb_dom_node_t*> {
  auto result = std::vector<lxb_dom_node_t*>{};
  if (node == nullptr) {
    return result;
  }
  for (auto* current : node_walker{node}) {
    if (is_non_element_node(current)) {
      continue;
    }
    if (match_selector_compiled<Selector>(current)) {
      result.push_back(current);
    }
  }
  return result;
}

}  // namespace detail
```

**Important:** The `match_simple` function accesses `compiled_selector_v<Selector>.groups[0]` for all compounds because the simple specs are shared across groups in the parsed representation. Each group's compounds are independent arrays. We need to parameterize by `GI` as well. Fix: change `match_simple` and `match_compound` to take a `GI` parameter and index `compiled_selector_v<Selector>.groups[GI].compounds[CompoundI].simples[SimpleI]`.

- [ ] **Step 3: Build and run tests**

```bash
./build.sh && ./test.sh
```

Expected: all tests pass (the public API is unchanged).

- [ ] **Step 4: Commit**

```bash
git add include/lexborpp/nttp_match.hpp
git commit -m "feat: replace AST-driven matching with recursive template JIT for NTTP CSS selectors"
```

---

## Task 3: Update nttp_query.hpp

**Files:**
- Modify: `include/lexborpp/nttp_query.hpp`

The public API functions call `detail::query_selector_impl` and `detail::query_selector_all_impl`, which are now in `nttp_match.hpp`. No signature changes needed. Just verify the includes are correct.

- [ ] **Step 1: Verify nttp_query.hpp compiles without changes**

The file already includes `nttp_match.hpp` and calls `detail::query_selector_impl<Selector>(node)` / `detail::query_selector_all_impl<Selector>(node)`. These are now defined in the rewritten `nttp_match.hpp`. No changes needed.

- [ ] **Step 2: Build and verify**

```bash
./build.sh && ./test.sh
```

Expected: all tests pass.

- [ ] **Step 3: Commit (if any changes were needed)**

If no changes were needed, skip this commit.

---

## Task 4: Update tests (remove static assertions for removed internals)

**Files:**
- Modify: `test/test_lexborpp.cpp`

Remove static assertions that reference `prefilter`, `shape`, and `selector_group_shape` which no longer exist. The runtime tests (`query_selector`, `query_selector_all`) remain unchanged.

- [ ] **Step 1: Remove static assertions for prefilter and shape**

Remove these lines from `test_lexborpp.cpp`:

```cpp
constexpr auto kPrefilterSpec = lexborpp::detail::parse_selector_spec<"section[data-role=tree] article.card#leaf-b">();
static_assert(kPrefilterSpec.groups[0].prefilter.kind == lexborpp::detail::selector_prefilter_kind::simple);
static_assert(kPrefilterSpec.groups[0].prefilter.simple.kind == lexborpp::detail::selector_simple_kind::id);
static_assert(kPrefilterSpec.groups[0].prefilter.simple.value == "leaf-b");

constexpr auto kGroupPrefilterSpec = lexborpp::detail::parse_selector_spec<"div.alpha[data-role=main], article.card#branch-b">();
static_assert(kGroupPrefilterSpec.groups[0].prefilter.simple.kind == lexborpp::detail::selector_simple_kind::type);
static_assert(kGroupPrefilterSpec.groups[0].prefilter.simple.value == "div");
static_assert(kGroupPrefilterSpec.groups[1].prefilter.simple.kind == lexborpp::detail::selector_simple_kind::id);
static_assert(kGroupPrefilterSpec.groups[1].prefilter.simple.value == "branch-b");

constexpr auto kShapeSpec = lexborpp::detail::parse_selector_spec<
  "div#container, section article strong, section > article > strong, article + article, article ~ article, section > article strong">();
static_assert(kShapeSpec.groups[0].shape == lexborpp::detail::selector_group_shape::simple);
static_assert(kShapeSpec.groups[1].shape == lexborpp::detail::selector_group_shape::descendant_chain);
static_assert(kShapeSpec.groups[2].shape == lexborpp::detail::selector_group_shape::child_chain);
static_assert(kShapeSpec.groups[3].shape == lexborpp::detail::selector_group_shape::adjacent_sibling_chain);
static_assert(kShapeSpec.groups[4].shape == lexborpp::detail::selector_group_shape::following_sibling_chain);
static_assert(kShapeSpec.groups[5].shape == lexborpp::detail::selector_group_shape::mixed);
```

Keep the `query_selector` and `query_selector_all` runtime tests — they verify correctness.

- [ ] **Step 2: Add compile-time verification of matching behavior**

Add new static assertions that verify the template matching produces correct results at compile time:

```cpp
// Verify that the parser still produces correct AST structure
constexpr auto kSimpleSpec = lexborpp::detail::parse_selector_spec<"div#container">();
static_assert(kSimpleSpec.group_count == 1);
static_assert(kSimpleSpec.groups[0].compound_count == 1);
static_assert(kSimpleSpec.groups[0].compounds[0].simple_count == 2);

constexpr auto kCommaSpec = lexborpp::detail::parse_selector_spec<"p, span">();
static_assert(kCommaSpec.group_count == 2);
```

- [ ] **Step 3: Build and run all tests**

```bash
./build.sh && ./test.sh
```

Expected: all tests pass.

- [ ] **Step 4: Commit**

```bash
git add test/test_lexborpp.cpp
git commit -m "test: update static assertions for simplified NTTP parser internals"
```

---

## Task 5: Verify all tests pass end-to-end

**Files:** None (verification only)

- [ ] **Step 1: Full build and test**

```bash
./build.sh && ./test.sh
```

Expected: 100% pass rate.

- [ ] **Step 2: Run specific NTTP selector test cases**

```bash
./build/test/all_test "CSS selectors (compile-time query_selector)"
./build/test/all_test "CSS selectors (compile-time query_selector_all)"
./build/test/all_test "lexborpp compile-time CSS selectors handle attributes and grouping"
```

Expected: all pass.

- [ ] **Step 3: Final commit (if any fixups were needed)**
