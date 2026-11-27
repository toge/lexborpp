# PR #6 追加レビュー指摘 1–5 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:test-driven-development for C++ and CMake regressions, then superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** PR #6 の追加レビュー指摘 1–5 を、既存の first-wins ID API を保ったまま修正し、重複 ID の検索同値性と CMake consumer portability を回帰テストで固定する。

**Architecture:** `document_id_index` に重複フラグと構築 root を追加し、runtime / NTTP indexed query は安全な場合だけ fast path を使う。CMake では package-config 時に解決される `lexborpp::lexbor` wrapper と、build tree / installed package の双方から再利用する resolver module を導入する。installed consumer、raw linkage、`build.sh` の各経路を実際に configure / compile / link / 実行する。

**Tech Stack:** C++23/26、CMake 3.25+、vcpkg manifest mode、Lexbor、Catch2 v3、POSIX `sh`、GCC / Clang / MSVC / WASI / Emscripten。

**Spec:** `docs/superpowers/specs/2026-11-27-pr6-review-fixes-design.md`

---

## File Structure

- `include/lexborpp/document_id_index.hpp`
  - `find()` / `contains()` の既存契約を維持しつつ、`is_ambiguous(id)`、`covers_scope(scope)`、snapshot root を追加する。
- `include/lexborpp/runtime_css_match.hpp`
  - runtime indexed query の fast path を unique ID かつ index が検索 scope を包含する場合だけ使用する。
- `include/lexborpp/nttp_match.hpp`
  - NTTP indexed query に同じ guard を適用する。
- `test/test_lexborpp.cpp`
  - 重複 ID、scope 外先頭候補、scope より狭い index、first/all parity を固定する。
- `cmake/lexborppResolveLexbor.cmake`
  - config target / raw target の解決、raw linkage tri-state、`LEXBOR_STATIC` 判定を一か所に持つ。
- `CMakeLists.txt`
  - build-tree の安定した `lexborpp::lexbor` wrapper を作り、install interface は外部 Lexbor 名 / path を含めない。
- `cmake/lexborppConfig.cmake.in`
  - consumer 側で resolver を実行し、安定した imported wrapper を構成する。
- `test/package_consumer/CMakeLists.txt`
  - 最小の installed-package consumer。外部リンクと `LEXBOR_STATIC` の期待値をコンパイル時 assertion で検証する。
- `test/package_consumer/main.cpp`
  - Lexbor API と fake symbol を条件付きで呼び出し、実際の link / run を検証する。
- `test/cmake/package_consumer_test.cmake`
  - install、target layout 変更、raw static/shared、dependency missing、export leak を実際に configure / build / run する CTest driver。
- `test/cmake/fake_lexbor_config/lexborConfig.cmake`
  - `lexbor::lexbor` だけを公開する consumer-side package config。
- `test/cmake/fake_lexbor_raw/CMakeLists.txt`, `fake.c`, `fake.h`
  - raw linkage テスト用の generic/shared library を platform-native に生成する補助 project。
- `test/CMakeLists.txt`
  - package consumer driver と add_subdirectory consumer を CTest に登録する。
- `build.sh`
  - caller-owned `VCPKG_ROOT` と明示的な `VCPKG_TARGET_TRIPLET` cache variable を使う。
- `README.md`, `AGENTS.md`
  - 重複 ID fallback、wrapper、raw linkage 設定、build script の契約を同期する。

---

### Task 1: 重複 ID と index scope の回帰テストを赤にする

**Files:**
- Modify: `test/test_lexborpp.cpp` near the existing `document_id_index registers only the first duplicate id` test

- [ ] **Step 1: 重複 ID の first/all parity test を追加する**

```cpp
TEST_CASE("indexed queries fall back for ambiguous ids") {
  auto fixture = html_document_fixture{
    R"(<div><i id="dup"></i><i id="dup" class="wanted"></i></div>)"};
  auto* root = fixture.document_node();
  auto index = lexborpp::document_id_index{root};

  REQUIRE(index.find("dup") != nullptr);
  REQUIRE(index.is_ambiguous("dup"));

  auto plain_first = lexborpp::query_selector(root, "#dup.wanted");
  auto indexed_first = lexborpp::query_selector(root, "#dup.wanted", index);
  REQUIRE(plain_first.has_value());
  REQUIRE(indexed_first.has_value());
  REQUIRE(indexed_first.value() == plain_first.value());
  REQUIRE(lexborpp::has_class(indexed_first.value(), "wanted"));

  auto plain_all = lexborpp::query_selector_all(root, "#dup");
  auto indexed_all = lexborpp::query_selector_all(root, "#dup", index);
  REQUIRE(plain_all.has_value());
  REQUIRE(indexed_all.has_value());
  REQUIRE(indexed_all.value() == plain_all.value());
  REQUIRE(indexed_all.value().size() == 2);

  REQUIRE(lexborpp::query_selector<"#dup.wanted">(root, index)
    == lexborpp::query_selector<"#dup.wanted">(root));
  REQUIRE(lexborpp::query_selector_all<"#dup">(root, index)
    == lexborpp::query_selector_all<"#dup">(root));
}
```

- [ ] **Step 2: 狭い query scope と狭い index root の test を追加する**

```cpp
TEST_CASE("indexed queries require the index root to cover the query scope") {
  auto fixture = html_document_fixture{
    R"(<section id="outside"><i id="dup"></i></section>
       <section id="inside"><i id="dup" class="wanted"></i></section>)"};
  auto* root = fixture.document_node();
  auto* inside = fixture.by_id("inside");
  auto* outside = fixture.by_id("outside");
  auto document_index = lexborpp::document_id_index{root};
  auto inside_index = lexborpp::document_id_index{inside};

  REQUIRE(document_index.covers_scope(inside));
  REQUIRE_FALSE(inside_index.covers_scope(root));
  REQUIRE_FALSE(inside_index.covers_scope(outside));

  for (auto const* selector : {"#dup", "#dup.wanted"}) {
    auto runtime_plain = lexborpp::query_selector(inside, selector);
    auto runtime_indexed =
      lexborpp::query_selector(inside, selector, document_index);
    REQUIRE(runtime_indexed.has_value());
    REQUIRE(runtime_indexed.value() == runtime_plain.value());

    auto runtime_all_plain =
      lexborpp::query_selector_all(inside, selector);
    auto runtime_all_indexed =
      lexborpp::query_selector_all(inside, selector, document_index);
    REQUIRE(runtime_all_indexed.value()
      == runtime_all_plain.value());

    auto reverse_plain = lexborpp::query_selector(root, selector);
    auto reverse_indexed =
      lexborpp::query_selector(root, selector, inside_index);
    REQUIRE(reverse_indexed.has_value());
    REQUIRE(reverse_indexed.value() == reverse_plain.value());

    auto reverse_all_plain = lexborpp::query_selector_all(root, selector);
    auto reverse_all_indexed =
      lexborpp::query_selector_all(root, selector, inside_index);
    REQUIRE(reverse_all_indexed.value()
      == reverse_all_plain.value());
  }

  REQUIRE(lexborpp::query_selector<"#dup">(inside, document_index)
    == lexborpp::query_selector<"#dup">(inside));
  REQUIRE(lexborpp::query_selector_all<"#dup.wanted">(
      inside, document_index)
    == lexborpp::query_selector_all<"#dup.wanted">(inside));
  REQUIRE(lexborpp::query_selector<"#dup.wanted">(root, inside_index)
    == lexborpp::query_selector<"#dup.wanted">(root));
  REQUIRE(lexborpp::query_selector_all<"#dup">(root, inside_index)
    == lexborpp::query_selector_all<"#dup">(root));
}
```

- [ ] **Step 3: テストだけビルドして失敗を確認する**

Run:

```bash
cmake --build build --target all_test --config Release --parallel
ctest --test-dir build -R "^all_test$" -V -C Release
```

Expected: compile failure because `is_ambiguous()` and `covers_scope()` do not exist, or assertion failure if the compile is first adapted.

---

### Task 2: duplicate-safe ID fast path を実装する

**Files:**
- Modify: `include/lexborpp/document_id_index.hpp`
- Modify: `include/lexborpp/runtime_css_match.hpp`
- Modify: `include/lexborpp/nttp_match.hpp`

- [ ] **Step 1: map value と snapshot root を実装する**

`document_id_index` の private state を次の形へ変更する。

```cpp
struct index_entry {
  lxb_dom_node_t* first{};
  bool ambiguous{};
};

auto build = [&](lxb_dom_node_t const* root) {
  root_ = const_cast<lxb_dom_node_t*>(root);
  if (root == nullptr) return;
  for (auto* node : node_walker{root_}) {
    if (is_non_element_node(node)) continue;
    auto const id = get_attr_value(node, "id");
    if (!id.has_value() || id->empty()) continue;
    auto [entry, inserted] =
      map_.try_emplace(std::string{*id}, index_entry{node, false});
    if (!inserted) {
      entry->second.ambiguous = true;
    }
  }
};
```

Use the same transparent hash with `std::unordered_map<std::string, index_entry, ...>`.

- [ ] **Step 2: `find`, `is_ambiguous`, `covers_scope` を実装する**

```cpp
[[nodiscard]] auto find(std::string_view id) const noexcept -> lxb_dom_node_t* {
  auto const it = map_.find(id);
  return it != map_.end() ? it->second.first : nullptr;
}

[[nodiscard]] auto is_ambiguous(std::string_view id) const noexcept -> bool {
  auto const it = map_.find(id);
  return it != map_.end() && it->second.ambiguous;
}

[[nodiscard]] auto covers_scope(lxb_dom_node_t const* scope) const noexcept -> bool {
  if (root_ == nullptr || scope == nullptr) return false;
  for (auto* node = const_cast<lxb_dom_node_t*>(scope);
       node != nullptr;
       node = node->parent) {
    if (node == root_) return true;
  }
  return false;
}
```

Reset `root_` on a null rebuild and retain first-wins `find()`.

- [ ] **Step 3: runtime fast path の guard を同値性に揃える**

For first and all:

```cpp
if (pf.found && pf.is_last &&
    index.covers_scope(node) &&
    !index.is_ambiguous(pf.value)) {
  // existing single-node fast path
}
return query_selector_spec_first/all(node, *spec);
```

All fallback work stays inside the existing `with_bad_alloc_guard`.

- [ ] **Step 4: NTTP fast path に同じ guard を適用する**

```cpp
if constexpr (info.found && info.is_last) {
  if (index.covers_scope(node) && !index.is_ambiguous(info.value)) {
    // existing single-node fast path
  }
}
return query_selector_impl<Selector>(node);
```

Apply the corresponding `query_selector_all` logic.

- [ ] **Step 5: 対象テストと既存 indexed tests を緑にする**

Run:

```bash
cmake --build build --target all_test --config Release --parallel
ctest --test-dir build -R "^all_test$" -V -C Release
```

Expected: all selected tests pass.

- [ ] **Step 6: コミット**

```bash
git add include/lexborpp/document_id_index.hpp \
        include/lexborpp/runtime_css_match.hpp \
        include/lexborpp/nttp_match.hpp \
        test/test_lexborpp.cpp
git commit -m "fix: 重複 ID と index scope で selector 走査へフォールバック"
```

---

### Task 3: CMake consumer matrix の赤テストを追加する

**Files:**
- Create: `test/package_consumer/CMakeLists.txt`
- Create: `test/package_consumer/main.cpp`
- Create: `test/cmake/package_consumer_test.cmake`
- Create: `test/cmake/fake_lexbor_config/lexborConfig.cmake`
- Create: `test/cmake/fake_lexbor_raw/CMakeLists.txt`
- Create: `test/cmake/fake_lexbor_raw/fake.c`
- Create: `test/cmake/fake_lexbor_raw/fake.h`
- Modify: `test/CMakeLists.txt`

- [ ] **Step 1: minimal installed consumer を作る**

`test/package_consumer/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(lexborpp_consumer LANGUAGES CXX)

find_package(lexborpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE lexborpp::lexborpp)

if(LEXBORPP_EXPECT_LINKAGE STREQUAL "STATIC")
  target_compile_definitions(consumer PRIVATE LEXBORPP_EXPECT_STATIC=1)
elseif(LEXBORPP_EXPECT_LINKAGE STREQUAL "SHARED")
  target_compile_definitions(consumer PRIVATE
    LEXBORPP_EXPECT_SHARED=1
    FAKE_LEXBOR_SHARED=1)
endif()

if(LEXBORPP_LINK_PROBE)
  target_compile_definitions(consumer PRIVATE LEXBORPP_LINK_PROBE=1)
  target_include_directories(consumer PRIVATE
    "${LEXBORPP_FAKE_LEXBOR_INCLUDE_DIR}")
endif()

enable_testing()
add_test(NAME consumer_runs COMMAND consumer)

if(LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR)
  set_tests_properties(consumer_runs PROPERTIES
    ENVIRONMENT_MODIFICATION
      "PATH=path_list_prepend:${LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR}")
endif()
```

`main.cpp`:

```cpp
#include <lexborpp.hpp>

#ifdef LEXBORPP_LINK_PROBE
#  include <fake.h>
#endif

#if defined(LEXBORPP_EXPECT_STATIC) && !defined(LEXBOR_STATIC)
#  error "LEXBOR_STATIC was not propagated"
#endif
#if defined(LEXBORPP_EXPECT_SHARED) && defined(LEXBOR_STATIC)
#  error "LEXBOR_STATIC leaked into shared consumer"
#endif

auto main() -> int {
#ifdef LEXBORPP_RUN_QUERY
  auto document = lexborpp::parse_html("<p id='ok'>ok</p>");
  if (!document) return 1;
  if (lexborpp::query_selector<"#ok">(
        lexborpp::get_root(*document)) == nullptr) {
    return 2;
  }
#endif
#ifdef LEXBORPP_LINK_PROBE
  if (lexborpp_test_fake_symbol() != 0x5a) {
    return 3;
  }
#endif
  return 0;
}
```

- [ ] **Step 2: fake package / raw library helper を作る**

`fake_lexbor_config/lexborConfig.cmake` exposes only `lexbor::lexbor`, using
library and include paths passed by the driver:

```cmake
add_library(lexbor::lexbor INTERFACE IMPORTED)
set_target_properties(lexbor::lexbor PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${LEXBORPP_FAKE_LEXBOR_INCLUDE_DIR}"
  INTERFACE_LINK_LIBRARIES "${LEXBORPP_FAKE_LEXBOR_LIBRARY}"
  INTERFACE_COMPILE_DEFINITIONS
    "${LEXBORPP_FAKE_LEXBOR_COMPILE_DEFINITIONS}")
set(lexbor_FOUND TRUE)
```

`fake_lexbor_raw/CMakeLists.txt` builds one platform-native fake library per
configuration so generic static and shared output names do not collide:

```cmake
cmake_minimum_required(VERSION 3.25)
project(fake_lexbor_raw LANGUAGES C)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(FAKE_LEXBOR_KIND STREQUAL "STATIC_EXPLICIT")
  add_library(lexbor_static STATIC fake.c)
  set(fake_target lexbor_static)
elseif(FAKE_LEXBOR_KIND STREQUAL "STATIC_GENERIC")
  add_library(lexbor STATIC fake.c)
  set(fake_target lexbor)
elseif(FAKE_LEXBOR_KIND STREQUAL "SHARED_GENERIC")
  add_library(lexbor SHARED fake.c)
  set(fake_target lexbor)
else()
  message(FATAL_ERROR "FAKE_LEXBOR_KIND is invalid")
endif()

target_include_directories(${fake_target} PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
if(FAKE_LEXBOR_KIND STREQUAL "SHARED_GENERIC")
  target_compile_definitions(${fake_target}
    PRIVATE FAKE_LEXBOR_BUILDING_SHARED=1)
endif()
install(TARGETS ${fake_target} DESTINATION lib)
install(FILES fake.h DESTINATION include)
```

`fake.h` exports a C symbol with the platform-correct decoration:

```cpp
#ifdef _WIN32
#  if defined(FAKE_LEXBOR_BUILDING_SHARED)
#    define FAKE_LEXBOR_API __declspec(dllexport)
#  elif defined(FAKE_LEXBOR_SHARED)
#    define FAKE_LEXBOR_API __declspec(dllimport)
#  else
#    define FAKE_LEXBOR_API
#  endif
#else
#  define FAKE_LEXBOR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif
FAKE_LEXBOR_API int lexborpp_test_fake_symbol(void);
#ifdef __cplusplus
}
#endif
```

`fake.c` returns `0x5a`. Raw static/shared consumers compile with
`LEXBORPP_LINK_PROBE=1` and call the symbol, so an implementation that compiles
but drops or mislinks the raw library fails. For Windows shared consumers the
driver additionally passes `FAKE_LEXBOR_SHARED=1` and
`LEXBORPP_FAKE_LEXBOR_RUNTIME_DIR`; the consumer test's
`ENVIRONMENT_MODIFICATION` prepends that DLL directory for CTest execution.

The driver creates independent helper build/prefix directories for all three
`FAKE_LEXBOR_KIND` values and passes the installed prefix's actual CMake target
file to each consumer lookup path.

- [ ] **Step 3: CTest driver の matrix を作る**

The script must:

1. Run `cmake --install` into a test-local prefix.
2. Read `lexborppTargets.cmake` and fail if it contains:
   - `lexbor::lexbor_static`
   - `lexbor::lexbor`
   - the source/build header or library absolute path
3. Configure/build/run the consumer with:
   - vcpkg `lexbor::lexbor_static`
   - fake Lexbor config exporting only `lexbor::lexbor`
   - raw header + `lexbor_static` with `AUTO`
   - raw generic library with `STATIC`
   - raw generic library with `SHARED`, `LEXBORPP_RUN_QUERY` disabled, and a
     real fake shared library link
   - raw generic library with `AUTO`, a fake generic shared library, and
     `LEXBORPP_EXPECT_SHARED=1`
   - raw generic library with invalid linkage value (expect configure failure)
   - `SHARED` with only a `lexbor_static` file (expect configure failure)
4. Configure a consumer with neither a Lexbor config package nor raw files and assert a
   clear `lexborpp` not-found message.

Use separate binary directories and remove only the exact test-local directories.
Pass libraries through `CMAKE_PREFIX_PATH` / `CMAKE_INCLUDE_PATH` /
`CMAKE_LIBRARY_PATH`; do not depend on generator-specific compile database files.
For raw and missing-dependency cases pass
`-DCMAKE_DISABLE_FIND_PACKAGE_lexbor=TRUE`, an isolated empty prefix, and:

```cmake
-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE
-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE
-DCMAKE_FIND_USE_INSTALL_PREFIX=FALSE
-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=FALSE
-DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
```

This disables both package-config discovery and implicit header/library search
outside the explicit isolated `CMAKE_INCLUDE_PATH` / `CMAKE_LIBRARY_PATH`.
Raw cases must not receive the vcpkg toolchain; otherwise
`find_package(lexbor)` / `find_path` could still see vcpkg installed trees.
The only config-target case receives the normal parent vcpkg prefix. Use helper
functions `lexborpp_run_configure`, `lexborpp_run_build`, and
`lexborpp_run_ctest` so expected-failure configure commands can be asserted
without aborting the entire script:

```cmake
execute_process(COMMAND ... RESULT_VARIABLE result
                OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0) message(FATAL_ERROR "expected configure failure") endif()
```

Pass the parent's generator, generator instance/platform/toolset, C/C++
compilers, architecture, MSVC runtime library, OSX architectures, and (for the
config-target case only) toolchain and vcpkg target triplet/installed dir to each
child configure. Each build/install command gets
`--config "${LEXBORPP_TEST_CONFIG}"`; child CTest gets
`-C "${LEXBORPP_TEST_CONFIG}"`. Never invoke the generated executable directly.

- [ ] **Step 4: `add_subdirectory` consumer を CTest matrix に追加する**

Configure a tiny project twice, pointing `add_subdirectory("${LEXBORPP_SOURCE_DIR}"
third_party/lexborpp)` at the current checkout, with Catch2/tests/bench disabled.
The first supplies the fake `lexbor::lexbor` target; the second uses raw static
lookup with isolated package discovery. Both link `lexborpp::lexborpp` and run
the same `consumer_runs` test. This catches wrapper properties that only work in
the installed package path.

- [ ] **Step 5: CTest registration**

In `test/CMakeLists.txt`, register the driver only when not cross-compiling:

```cmake
add_test(
  NAME package_consumer_matrix
  COMMAND "${CMAKE_COMMAND}"
    "-DLEXBORPP_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DLEXBORPP_BINARY_DIR=${PROJECT_BINARY_DIR}"
    "-DLEXBORPP_TEST_CONFIG=$<CONFIG>"
    "-DLEXBORPP_REAL_LEXBOR_LIBRARY=${LEXBORPP_RESOLVED_CONCRETE_LIBRARY}"
    "-DLEXBORPP_LEXBOR_INCLUDE_DIR=${LEXBOR_INCLUDE_DIR}"
    "-DLEXBORPP_GENERATOR=${CMAKE_GENERATOR}"
    "-DLEXBORPP_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
    "-DLEXBORPP_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DLEXBORPP_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DLEXBORPP_C_COMPILER=${CMAKE_C_COMPILER}"
    "-DLEXBORPP_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DLEXBORPP_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DLEXBORPP_VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
    "-DLEXBORPP_VCPKG_INSTALLED_DIR=${VCPKG_INSTALLED_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_consumer_test.cmake"
)
```

`lexborpp::lexbor` is an INTERFACE target, so `$<TARGET_FILE:...>` is invalid.
The resolver explicitly exposes `LEXBORPP_RESOLVED_CONCRETE_LIBRARY` for the
test driver with `PARENT_SCOPE`. It is a concrete raw file when raw lookup was
used, or `$<TARGET_FILE:actual-lexbor-target>` for an imported config target.
The driver uses it only for the fake config case; real vcpkg config consumption
uses the isolated copy of the parent vcpkg prefix.

- [ ] **Step 6: 現状で test driver を赤にする**

Run:

```bash
cmake --build build --target all_test --config Release --parallel
ctest --test-dir build -R package_consumer_matrix -V -C Release
```

Expected: failure on installed package export containing
`lexbor::lexbor_static`, and/or generation failure with the fake
`lexbor::lexbor`-only config. No source implementation may be changed yet.

---

### Task 4: Lexbor resolver と安定 wrapper を実装する

**Files:**
- Create: `cmake/lexborppResolveLexbor.cmake`
- Modify: `CMakeLists.txt`
- Modify: `cmake/lexborppConfig.cmake.in`

- [ ] **Step 1: resolver module を実装する**

Define a function with an explicit result prefix:

```cmake
function(lexborpp_resolve_lexbor_dependency out_prefix)
  find_package(lexbor CONFIG QUIET)
  set(found FALSE)
  set(link_item "")
  set(include_dirs "")
  set(compile_definitions "")
  set(concrete_library "")
  set(not_found_message "")

  if(TARGET lexbor::lexbor_static)
    set(found TRUE)
    set(link_item lexbor::lexbor_static)
    set(compile_definitions LEXBOR_STATIC)
    set(concrete_library "$<TARGET_FILE:lexbor::lexbor_static>")
  elseif(TARGET lexbor::lexbor)
    set(found TRUE)
    set(link_item lexbor::lexbor)
    get_target_property(lexbor_type lexbor::lexbor TYPE)
    if(NOT lexbor_type STREQUAL "INTERFACE_LIBRARY")
      set(concrete_library "$<TARGET_FILE:lexbor::lexbor>")
    endif()
  else()
    # Validate linkage, then perform the raw fallback described below.
  endif()

  set(${out_prefix}_FOUND "${found}" PARENT_SCOPE)
  set(${out_prefix}_LINK_ITEM "${link_item}" PARENT_SCOPE)
  set(${out_prefix}_INCLUDE_DIRS "${include_dirs}" PARENT_SCOPE)
  set(${out_prefix}_COMPILE_DEFINITIONS
      "${compile_definitions}" PARENT_SCOPE)
  set(${out_prefix}_CONCRETE_LIBRARY
      "${concrete_library}" PARENT_SCOPE)
  set(${out_prefix}_NOT_FOUND_MESSAGE
      "${not_found_message}" PARENT_SCOPE)
endfunction()
```

Assign every output with `PARENT_SCOPE` on every return path. For invalid raw
linkage, missing header, or missing library, set `not_found_message` to a useful
reason and return after publishing all outputs. A config target owns its usage
requirements; additionally, `lexbor::lexbor_static` must result in
`LEXBOR_STATIC` on the wrapper so that the guarantee does not depend on the
upstream export implementation.

Set raw linkage default:

```cmake
set(LEXBORPP_RAW_LEXBOR_LINKAGE "AUTO" CACHE STRING
    "Raw Lexbor linkage: AUTO, STATIC, or SHARED")
set_property(CACHE LEXBORPP_RAW_LEXBOR_LINKAGE PROPERTY STRINGS
             AUTO STATIC SHARED)
```

Validate the exact allowed set and fail on lowercase values.

Use:

| mode | `find_library` names |
| --- | --- |
| `AUTO` | `lexbor_static`, then `lexbor`; mark static only for first |
| `STATIC` | `lexbor_static`, then `lexbor`; mark static for both |
| `SHARED` | `lexbor` only; mark static false |

Return:

```cmake
${out_prefix}_FOUND
${out_prefix}_LINK_ITEM
${out_prefix}_INCLUDE_DIRS
${out_prefix}_COMPILE_DEFINITIONS
${out_prefix}_CONCRETE_LIBRARY
${out_prefix}_NOT_FOUND_MESSAGE
```

Set `LEXBOR_STATIC` for `lexbor::lexbor_static` config targets and for raw paths
identified as static by the resolved linkage mode. Do not set it for a generic
raw library in `AUTO` or for `SHARED`. In raw mode assign
`concrete_library` to the exact `find_library()` result. This variable is for
test infrastructure only; it must never enter an install interface property.

- [ ] **Step 2: build-tree stable wrapper を作る**

In top-level `CMakeLists.txt`:

```cmake
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/lexborppResolveLexbor.cmake")
lexborpp_resolve_lexbor_dependency(LEXBORPP_RESOLVED)
if(NOT LEXBORPP_RESOLVED_FOUND)
  message(FATAL_ERROR "${LEXBORPP_RESOLVED_NOT_FOUND_MESSAGE}")
endif()

add_library(lexborpp_lexbor INTERFACE)
add_library(lexborpp::lexbor ALIAS lexborpp_lexbor)
set_target_properties(lexborpp_lexbor PROPERTIES EXPORT_NAME lexbor)
target_link_libraries(lexborpp_lexbor INTERFACE
  "$<BUILD_INTERFACE:${LEXBORPP_RESOLVED_LINK_ITEM}>")
target_include_directories(lexborpp_lexbor INTERFACE
  "$<BUILD_INTERFACE:${LEXBORPP_RESOLVED_INCLUDE_DIRS}>")
target_compile_definitions(lexborpp_lexbor INTERFACE
  "$<BUILD_INTERFACE:${LEXBORPP_RESOLVED_COMPILE_DEFINITIONS}>")

target_link_libraries(lexborpp INTERFACE
  lexborpp_lexbor)
```

If a resolver field is empty, omit the corresponding `target_*` call instead of
passing an empty include path, link item, or compile definition.

Install and export `lexborpp_lexbor` together with `lexborpp`; `EXPORT_NAME lexbor`
creates the stable imported `lexborpp::lexbor`. `lexborpp::lexborpp` must link only
to this exported wrapper. The wrapper's install interface contains no external
Lexbor link item or header/library path; `$<BUILD_INTERFACE:...>` must hold all
build-time external dependency data. Replace the existing install line with:

```cmake
install(TARGETS lexborpp lexborpp_lexbor EXPORT lexborppTargets)
```

- [ ] **Step 3: package config を consumer 側で再解決する**

`lexborppConfig.cmake.in` must:

1. Resolve consumer-side Lexbor with the installed resolver module.
2. On failure set `lexborpp_FOUND FALSE` and `lexborpp_NOT_FOUND_MESSAGE`, then
   `return()`.
3. Include `lexborppTargets.cmake`; it creates imported `lexborpp::lexborpp` and
   `lexborpp::lexbor`.
4. Apply the resolved link item, include directories, and compile definitions to
   `lexborpp::lexbor`.
5. Re-read the installed target file in the CTest driver and verify that no
   `lexbor::lexbor*`, source path, or build path leaked into it.
6. Call `check_required_components(lexborpp)`.

Install `cmake/lexborppResolveLexbor.cmake` next to the package config.

- [ ] **Step 4: package matrix を緑にする**

Run:

```bash
cmake --build build --config Release --parallel
ctest --test-dir build -R package_consumer_matrix -V -C Release
```

Expected: all package target variants, raw linkage modes, missing dependency, and
export leak checks pass.

- [ ] **Step 5: raw / MSVC compile assertions を確認する**

- GNU/Clang: inspect verbose build output to confirm static consumer receives
  `-DLEXBOR_STATIC`, shared consumer does not.
- The resolver matrix includes `AUTO + lexbor_static`,
  `AUTO + generic fake shared`, `STATIC + generic fake static`,
  `SHARED + generic fake shared`, invalid value, and static-only + `SHARED`.
- MSVC CI: consumer uses preprocessor assertions and compiles real Lexbor headers.
- Visual Studio generators must not require `compile_commands.json`.

- [ ] **Step 6: コミット**

```bash
git add CMakeLists.txt cmake/lexborppConfig.cmake.in \
        cmake/lexborppResolveLexbor.cmake test/CMakeLists.txt \
        test/package_consumer test/cmake/package_consumer_test.cmake \
        test/cmake/fake_lexbor_config test/cmake/fake_lexbor_raw
git commit -m "fix: installed consumer から Lexbor dependency を再解決"
```

---

### Task 5: `build.sh` の vcpkg triplet と root を修正する

**Files:**
- Modify: `build.sh`

- [ ] **Step 1: root / triplet の優先順位を実装する**

Use shell-compatible defaults:

```sh
: "${VCPKG_ROOT:=$HOME/vm/vcpkg}"
: "${VCPKG_TARGET_TRIPLET:=x64-linux-static}"
: "${VCPKG_OVERLAY_PORTS:=$VCPKG_ROOT/ports}"

if ! VCPKG_ROOT=$(readlink -f "$VCPKG_ROOT"); then
  echo "error: cannot resolve VCPKG_ROOT: $VCPKG_ROOT" >&2
  exit 1
fi
if [ ! -d "$VCPKG_ROOT" ] ||
   [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
  echo "error: VCPKG_ROOT is not a vcpkg checkout: $VCPKG_ROOT" >&2
  exit 1
fi
export VCPKG_ROOT VCPKG_TARGET_TRIPLET VCPKG_OVERLAY_PORTS
```

Quote all paths and pass:

```sh
cmake -B "$BUILD_DIR" \
  "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  "-DVCPKG_TARGET_TRIPLET=$VCPKG_TARGET_TRIPLET" \
  -DLEXBORPP_NATIVE_ARCH=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -S .
```

- [ ] **Step 2: caller override と clean static build を検証する**

Start with an explicitly clean output. Inspect and remove only the exact
`build_static` directory if it belongs to this test; otherwise stop rather than
reusing a cache whose triplet differs.

Run values that differ from the old hard-coded behavior:

```bash
VCPKG_ROOT="$HOME/vm/vcpkg" VCPKG_TARGET_TRIPLET=x64-linux ./build.sh static
grep '^VCPKG_TARGET_TRIPLET:' build_static/CMakeCache.txt
```

Expected: `VCPKG_TARGET_TRIPLET:STRING=x64-linux`.

Run again using a valid alternate root containing `scripts/buildsystems/vcpkg.cmake`
as a wrapper that includes the real vcpkg toolchain. Create
`build/vcpkg-root-wrapper/scripts/buildsystems/vcpkg.cmake` with:

```cmake
include("$ENV{REAL_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
```

Export `REAL_VCPKG_ROOT="$HOME/vm/vcpkg"`, run with that wrapper as
`VCPKG_ROOT`, and assert:

```bash
grep '^CMAKE_TOOLCHAIN_FILE:' build_static/CMakeCache.txt
```

contains the wrapper path. This test fails with the old hard-coded root whereas a
simple symlink would canonicalize back to the old path. Run an existing non-vcpkg
directory and a nonexistent path; both must fail the `-d` / toolchain-file checks
with a clear error. Finally unset the triplet on a clean output and assert the
default is `x64-linux-static`.

- [ ] **Step 3: コミット**

```bash
git add build.sh
git commit -m "fix: build.sh の vcpkg root と triplet を明示"
```

---

### Task 6: API / CMake 契約を文書化し最終検証する

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: README を同期する**

Update the package-install section to state:

- `lexbor::lexbor_static` / `lexbor::lexbor` config targets and raw
  header/library fallback are supported.
- `lexborpp::lexborpp` does not expose a build-time Lexbor target name.
- raw linkage defaults to `AUTO`; `LEXBORPP_RAW_LEXBOR_LINKAGE=STATIC|SHARED`
  resolves ambiguous generic library names.

Update the index section:

- `find(id)` remains first-wins.
- `is_ambiguous(id)` identifies duplicate snapshots.
- `covers_scope(scope)` controls safe fast paths.
- indexed query falls back to ordinary scanning for ambiguous IDs or an index that
  does not cover the whole query scope.

- [ ] **Step 2: AGENTS.md を同期する**

Replace stale statements that runtime failures return only null/empty and that
duplicate IDs necessarily differ from non-indexed searches. Add the raw linkage
variable, wrapper target, and package-consumer test commands.

- [ ] **Step 3: 完全検証**

Run in order:

```bash
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure -C Release
cmake --install build --prefix "$PWD/build/final-install" --config Release
ctest --test-dir build -R package_consumer_matrix -V -C Release
git diff --check
git status --short
```

Expected:

- 3 existing CTest targets plus package consumer matrix pass.
- no whitespace errors.
- no unintended tracked modifications.
- installed package export contains no external Lexbor target / raw absolute path.

- [ ] **Step 4: CI matrix を確認する**

Confirm GitHub Actions is green for:

- GCC 14–16
- Clang 19–22
- MSVC
- macOS
- Emscripten
- WASI p1 / p2

If a matrix job exposes a generator-specific issue, fix it in this task rather than
weakening the consumer assertions.

- [ ] **Step 5: ドキュメントコミット**

```bash
git add README.md AGENTS.md
git commit -m "docs: 重複 ID と Lexbor consumer 解決契約を更新"
```

---

## Completion Checklist

- [ ] Duplicate-ID first/all results match runtime and NTTP normal scans.
- [ ] Fast path is disabled for ambiguous IDs and uncovered query roots.
- [ ] `document_id_index::find()` still returns the first duplicate.
- [ ] Installed export contains only stable `lexborpp::lexbor`, not `lexbor::lexbor*` or build paths.
- [ ] `lexbor::lexbor_static`, `lexbor::lexbor`, raw static, raw generic STATIC/SHARED, missing dependency, and add_subdirectory consumers are tested.
- [ ] `LEXBOR_STATIC` propagates for static raw linkage and config-provided
      `lexbor::lexbor_static`, but never for shared/generic AUTO linkage.
- [ ] `./build.sh static` records the requested vcpkg triplet and preserves caller `VCPKG_ROOT`.
- [ ] README and AGENTS.md describe the actual contracts.
- [ ] PR #6 checks and local full verification pass.
