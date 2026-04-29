# Range Adapters Minimal Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the existing single-header `tag` / `id` / `clazz` / `attr` API, and finish issue #4 by hardening the implementation without changing behavior.

**Architecture:** Preserve `include/lexborpp.hpp` as the only public header and limit code changes to the ranges adapter section plus its direct includes. Use TDD to add one focused regression around null-safe filtering, then implement the smallest code change that makes the regression pass while keeping the current exact-match semantics for `clazz` and `attr`.

**Tech Stack:** C++26-compatible headers, Lexbor DOM nodes, `std::ranges`, Catch2, CMake, CTest

---

## File Structure

- Modify: `include/lexborpp.hpp`
  - Add the direct standard header dependency required by `detail::fixed_string`
  - Harden the `tag` / `id` / `clazz` / `attr` predicates in the existing range adapter block
- Modify: `test/test_lexborpp.cpp`
  - Extend the existing range adapter test area with one regression that proves `tag` ignores `nullptr` inputs instead of dereferencing them
- Reuse: `build.sh`, `test.sh`
  - Keep the existing configure/build/test workflow

### Task 1: Add a focused failing regression

**Files:**
- Modify: `test/test_lexborpp.cpp:517-553`
- Test: `test/test_lexborpp.cpp`

- [ ] **Step 0: Prepare the build directory in a fresh checkout or worktree**

Run:

```bash
./build.sh
```

Expected: CMake configure and build complete successfully, producing `./build/test/all_test`

- [ ] **Step 1: Add a regression case to the existing range adapter test**

```cpp
  SECTION("tag ignores nullptr entries in arbitrary pointer ranges") {
    auto const nodes = std::array<lxb_dom_node_t*, 3>{nullptr, body, nullptr};
    auto ids = collect_ids(nodes | lexborpp::tag<LXB_TAG_BODY>);
    REQUIRE(ids == std::vector<std::string>{"body"});
  }
```

- [ ] **Step 2: Build and run the focused test to confirm the regression fails before implementation**

Run:

```bash
cmake --build build --target all_test --parallel && ./build/test/all_test "range adapters tag id clazz attr filter expected nodes"
```

Expected: FAIL due to dereferencing `nullptr` inside `lexborpp::tag`

- [ ] **Step 3: Commit the failing test checkpoint only if your workflow requires preserving red-green history**

```bash
git add test/test_lexborpp.cpp
git commit -m "test: cover null-safe tag adapter"
```

### Task 2: Harden the adapter implementation

**Files:**
- Modify: `include/lexborpp.hpp:4-13`
- Modify: `include/lexborpp.hpp:984-1057`
- Test: `test/test_lexborpp.cpp:517-553`

- [ ] **Step 1: Add the missing direct standard include**

```cpp
#include <algorithm>
```

- [ ] **Step 2: Make the adapter predicates explicitly noexcept and null-safe**

```cpp
template <lxb_tag_id_t Tag>
inline constexpr auto tag = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return node != nullptr && node->local_name == Tag;
});

template <detail::fixed_string Id>
inline constexpr auto id = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return lexborpp::get_attr_value(node, "id") == Id.view();
});

template <detail::fixed_string Class>
inline constexpr auto clazz = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return lexborpp::get_attr_value(node, "class") == Class.view();
});

template <detail::fixed_string Attr, detail::fixed_string Value>
inline constexpr auto attr = std::views::filter([](lxb_dom_node_t* node) noexcept {
  return lexborpp::get_attr_value(node, Attr.view()) == Value.view();
});
```

- [ ] **Step 3: Re-run the focused regression**

Run:

```bash
cmake --build build --target all_test --parallel && ./build/test/all_test "range adapters tag id clazz attr filter expected nodes"
```

Expected: PASS

- [ ] **Step 4: Re-run the repository build and full test suite**

Run:

```bash
./build.sh && ./test.sh
```

Expected: build succeeds and the existing test suite stays green

- [ ] **Step 5: Commit the implementation**

```bash
git add include/lexborpp.hpp test/test_lexborpp.cpp
git commit -m "fix: harden lexborpp range adapters"
```

### Task 3: Final review and handoff

**Files:**
- Review: `include/lexborpp.hpp`
- Review: `test/test_lexborpp.cpp`

- [ ] **Step 1: Confirm the API examples in the spec still match the code**

Check:

```text
lexborpp::tag<LXB_TAG_...>
lexborpp::id<"...">
lexborpp::clazz<"...">
lexborpp::attr<"...", "...">
```

Expected: no public API drift

- [ ] **Step 2: Record completion in the issue/PR summary**

Mention:

```text
- single-header API preserved
- direct include for std::copy_n added
- predicates made noexcept
- tag adapter hardened for nullptr entries
- existing tests remained green
```
