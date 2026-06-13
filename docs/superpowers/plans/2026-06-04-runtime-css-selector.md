# Runtime CSS Selector Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Lexbor C API-based runtime CSS selector engine with a custom parser + AST interpreter, matching the NTTP version's feature set.

**Architecture:** Create a runtime CSS parser that produces an owned `selector_spec` (using `std::string` instead of `std::string_view`), and a runtime matching engine that interprets the AST with loops instead of recursive templates. The existing NTTP compile-time path remains unchanged.

**Tech Stack:** C++23/26, Lexbor DOM API (for node traversal only), custom CSS parser

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `include/lexborpp/runtime_css_parser.hpp` | Create | Owned spec types + runtime CSS parser |
| `include/lexborpp/runtime_css_match.hpp` | Create | Runtime AST interpreter (matching engine) |
| `include/lexborpp/serialize_runtime.hpp` | Modify | Replace Lexbor CSS API with custom engine |
| `include/lexborpp.hpp` | Modify | Add new headers to umbrella |
| `test/test_lexborpp.cpp` | Modify | Add tests for runtime CSS selector |

---

## Task 1: Create runtime CSS parser (owned AST)

**Files:**
- Create: `include/lexborpp/runtime_css_parser.hpp`

- [ ] **Step 1: Create the runtime parser file**

Create `include/lexborpp/runtime_css_parser.hpp` with:

1. Owned spec types (same structure as NTTP but with `std::string` instead of `std::string_view`):
   - `runtime_simple_spec` — kind, name (string), value (string), attribute_match
   - `runtime_compound_spec` — simples array, simple_count, relation
   - `runtime_group_spec` — compounds array, compound_count
   - `runtime_selector_spec` — groups array, group_count

2. Copied pure helper functions from `nttp_parser.hpp` (constexpr, no template dependency):
   - `is_space`, `is_name_terminator`, `ascii_lower`, `iequals`
   - `skip_spaces`, `parse_name`, `parse_quoted_value`
   - `is_simple_selector_start`

3. Runtime parsing functions (non-template, take `std::string_view` input):
   - `parse_runtime_simple_selector(input, pos, compound)` — parse one simple selector
   - `parse_runtime_compound_selector(input, pos, group, relation)` — parse one compound
   - `parse_runtime_selector(input)` → `runtime_selector_spec` — top-level parser

4. The `enum class` types are reused from `nttp_parser.hpp` (`selector_combinator`, `selector_simple_kind`, `selector_attribute_match`).

- [ ] **Step 2: Commit**

```bash
git add include/lexborpp/runtime_css_parser.hpp
git commit -m "feat: add runtime CSS parser with owned AST"
```

---

## Task 2: Create runtime matching engine

**Files:**
- Create: `include/lexborpp/runtime_css_match.hpp`

- [ ] **Step 1: Create the runtime matcher file**

Create `include/lexborpp/runtime_css_match.hpp` with:

1. `match_runtime_simple(node, simple)` — interpret one `runtime_simple_spec` against a node (switch on kind)
2. `match_runtime_compound(node, compound)` — AND all simples in a `runtime_compound_spec`
3. `match_runtime_compound_chain(node, compounds, index)` — recursive right-to-left walk (loop-based, not template-based)
4. `match_runtime_group(node, group)` — match one group
5. `match_runtime_selector(node, selector)` — match all groups (OR)
6. `query_selector_runtime(node, selector_string)` → `lxb_dom_node_t*` — parse + match + walk DOM
7. `query_selector_all_runtime(node, selector_string)` → `std::vector<lxb_dom_node_t*>` — same, all results

- [ ] **Step 2: Commit**

```bash
git add include/lexborpp/runtime_css_match.hpp
git commit -m "feat: add runtime CSS matching engine with AST interpreter"
```

---

## Task 3: Replace Lexbor CSS API in serialize_runtime.hpp

**Files:**
- Modify: `include/lexborpp/serialize_runtime.hpp`

- [ ] **Step 1: Replace implementation**

In `serialize_runtime.hpp`:
1. Remove `#include "lexbor/css/css.h"` and `#include "lexbor/selectors/selectors.h"`
2. Remove `css_parser_deleter`, `css_selectors_deleter`, `selectors_deleter` structs
3. Add `#include "lexborpp/runtime_css_match.hpp"`
4. Replace `query_selector` body with: `return detail::query_selector_runtime(node, selector);`
5. Replace `query_selector_all` body with: `return detail::query_selector_all_runtime(node, selector);`

- [ ] **Step 2: Update umbrella header**

Add `#include "lexborpp/runtime_css_parser.hpp"` and `#include "lexborpp/runtime_css_match.hpp"` to `include/lexborpp.hpp`.

- [ ] **Step 3: Build and run tests**

```bash
./build.sh && ./test.sh
```

Expected: all existing tests pass (runtime `query_selector` / `query_selector_all` are used in test_lexborpp.cpp).

- [ ] **Step 4: Commit**

```bash
git add include/lexborpp/serialize_runtime.hpp include/lexborpp.hpp
git commit -m "feat: replace Lexbor CSS API with custom runtime engine"
```

---

## Task 4: Add runtime CSS selector tests

**Files:**
- Modify: `test/test_lexborpp.cpp`

- [ ] **Step 1: Add test cases**

Add a new `TEST_CASE` for runtime CSS selectors covering:
- Type selector: `query_selector(root, "div")`
- ID selector: `query_selector(root, "#content")`
- Class selector: `query_selector(root, ".entry")`
- Attribute selector: `query_selector(root, "[data-role=main]")`
- Compound selector: `query_selector(root, "div#content.entry")`
- Child combinator: `query_selector(root, "span > b")`
- Descendant combinator: `query_selector(root, "div b")`
- Adjacent sibling: `query_selector(root, "p + p.target")`
- Following sibling: `query_selector(root, "article ~ article")`
- Comma group: `query_selector_all(root, "p, span > b")`
- Empty/invalid: `query_selector(root, "")` → nullptr
- nullptr: `query_selector(nullptr, "div")` → nullptr

- [ ] **Step 2: Build and run tests**

```bash
./build.sh && ./test.sh
```

Expected: all tests pass including new ones.

- [ ] **Step 3: Commit**

```bash
git add test/test_lexborpp.cpp
git commit -m "test: add runtime CSS selector tests"
```

---

## Task 5: Full verification

- [ ] **Step 1: Final build and test**

```bash
./build.sh && ./test.sh
```

Expected: 100% pass rate.
