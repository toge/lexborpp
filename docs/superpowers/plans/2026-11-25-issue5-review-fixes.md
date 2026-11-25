# Issue #5 レビュー指摘修正 実装プラン

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** lexborpp issue #5 のレビュー指摘 10 項目を修正し、回帰テストと README を同期する。

**Architecture:** ヘッダオンリーライブラリの個所修正が中心。DOM 安全性 (core.hpp)、インデックス検索の契約统一 (runtime_css_match.hpp / nttp_match.hpp)、ビルドシステム (CMakeLists.txt / config テンプレート)、パーサ整合 (nttp_parser.hpp / runtime_css_parser.hpp)、ドキュメント同期 (README.md)。

**Tech Stack:** C++23/26 ヘッダオンリー、Lexbor (vcpkg `lexbor::lexbor_static`)、Catch2、CMake 3.25+。

**Spec:** https://github.com/toge/lexborpp/issues/5 (レビュー指摘の本文。修正方針と完了条件を含む)

## Global Constraints

- ビルド/テストは `./build.sh` / `./test.sh` (vcpkg `~/vm/vcpkg`、triplet `x64-linux-static`)。
- 警告は `-Wall -Wextra -pedantic` (top-level)。新規警告を出さない。
- NTTP 不正入力は `LEXBORPP_CONSTEVAL_FAIL` に統一 (config.hpp の契約)。
- 公開 API の既存シグネチャは極力維持 (例外契約の変更は issue の方針内で)。
- コメントは日本語、既存スタイルに従う。

## Review Focus

- MSVC + 静的 vcpkg での `LEXBOR_STATIC` 伝搬 — 本環境ではビルド検証不可。CMake 設定で `lexbor::lexbor_static` の usage requirements を直接使うことで構造的に解消する (手動確認は cmake キャッシュで行う)。
- OOM 経路 — 実際の bad_alloc はテスト困難。noexcept 境界を撤去/try-catch で構造的に解消し、ビルドで確認。
- インストール済み consumer + `-fno-exceptions` — 手動のステージングインストール検証で確認する (恒久テストにはしない)。
- インデックス検索と通常検索の全 combinator 同値 — 回帰テストでパリティを固定する。

---

### Task 1: 非要素ノードの安全化 (issue 1)

**Files:**
- Modify: `include/lexborpp/core.hpp:93-111` (as_element), `:517-548` (set_text_content)
- Test: `test/test_lexborpp.cpp` (新 TEST_CASE 追加)

**Interfaces:**
- Produces: `as_element(node)` は非要素ノードで `nullptr` を返す。`set_text_content(node, text)` は要素ノード以外で `false` を返す。

- [ ] **Step 1: 失敗するテストを書く** — `as_element(text_node) == nullptr`、`set_attr(as_element(text_node), ...) == false`、`set_text_content(text_node, "new") == false` かつ親の outer_html 不変、`set_text_content(comment, ...) == false`。
- [ ] **Step 2: テスト失敗を確認** (`./build/test/all_test "..."`)
- [ ] **Step 3: 実装** — `as_element` は `node->type != LXB_DOM_NODE_TYPE_ELEMENT` で nullptr。`set_text_content` 冒頭で同判定し false。
- [ ] **Step 4: テスト成功確認 + 全テスト**
- [ ] **Step 5: コミット**

### Task 2: インデックス検索を README 契約に統一 (issue 2)

**Files:**
- Modify: `include/lexborpp/runtime_css_match.hpp:188-238`
- Modify: `include/lexborpp/nttp_match.hpp:221-279`
- Test: `test/test_lexborpp.cpp` (パリティ TEST_CASE 追加)

**Interfaces:**
- Produces: インデックス fast path は「id が右端 compound」のときだけ使う。それ以外 (runtime/NTTP とも) は通常走査へフォールバックし、通常検索と結果が常に一致する。

- [ ] **Step 1: 失敗するテスト** — `<i id="a"></i><span></span>` + index で `#a + span` / `#a ~ span` の runtime・NTTP、first/all が通常検索と同じ結果になること。加えて `div#x p`(id 右端) のパリティ。
- [ ] **Step 2: 失敗確認**
- [ ] **Step 3: 実装** — runtime: `if (pf.found && pf.is_last)` に変更し、非右端は `query_selector_spec_first/all(node, *spec)` へ。重複ブランチを統合。NTTP: `if constexpr (info.found && info.is_last)` に変更し、subtree-walker ブランチを削除。
- [ ] **Step 4: 全テスト** (既存 "indexed queries respect the search scope" も通ること)
- [ ] **Step 5: コミット**

### Task 3: OOM が terminate しないようにする (issue 5)

**Files:**
- Modify: `include/lexborpp/runtime_css_match.hpp:127-158` (query_selector_spec_all の noexcept 削除、公開 all 関数に try/catch)
- Modify: `include/lexborpp/serialize_runtime.hpp:16-20` (serialize_callback に try/catch)

**Interfaces:**
- Produces: `query_selector_all` (runtime) は OOM 時 `std::unexpected(std::errc::not_enough_memory)`。`outer_html`/`inner_html` は OOM で terminate しない (部分的な文字列を返し得る)。

- [ ] **Step 1: 実装** — `query_selector_spec_all` から `noexcept` を削除。公開 `query_selector_all_runtime` 2 関数は呼び出しを `try { ... } catch (std::bad_alloc const&) { return std::unexpected(std::errc::not_enough_memory); }` で包む。`serialize_callback` は append を try/catch し `LXB_STATUS_ERROR_MEMORY_ALLOCATION` を返す。`#include <new>` 追加。
- [ ] **Step 2: ビルド + 全テスト** (OOM は直接テストしない。`shortcut: OOM 経路はテスト不能、契約変更のみ`)
- [ ] **Step 3: コミット (Task 1, 2 とまとめてもよい)**

### Task 4: SVG `<text>` type selector (issue 6)

**Files:**
- Modify: `include/lexborpp/nttp_parser.hpp:134-331` (kTagLookup から内部 synthetic tag を削除)
- Test: `test/test_lexborpp.cpp`

**Interfaces:**
- Produces: `lookup_tag_id` は要素名に存在しない内部タグ (`undef` / `end_of_file` / `text` / `document` / `em_comment` / `em_doctype`) を返さない。`text` 型 selector は qualified name フォールバックで SVG `<text>` に一致する。

- [ ] **Step 1: 失敗するテスト** — `<svg><text id="svg-text">` に対し runtime/NTTP の `text` と `svg text` が一致する。
- [ ] **Step 2: 失敗確認**
- [ ] **Step 3: 実装** — kTagLookup の 6 件を削除 (196 → 190)。
- [ ] **Step 4: 全テスト**
- [ ] **Step 5: コミット (Task 7, 8 とまとめてもよい)**

### Task 5: 空文字列属性 selector を CSS 仕様に合わせる (issue 7)

**Files:**
- Modify: `include/lexborpp/nttp_parser.hpp:493-524` (match_attribute)
- Test: `test/test_lexborpp.cpp`

- [ ] **Step 1: 失敗するテスト** — `[data-x^=""]` / `[data-x$=""]` / `[data-x*=""]` は `data-x=""` の要素に一致しない。`[data-x=""]` は一致する。runtime + NTTP。
- [ ] **Step 2: 失敗確認**
- [ ] **Step 3: 実装** — prefix/suffix/substring に `!expected.empty()` を追加。
- [ ] **Step 4: 全テスト**
- [ ] **Step 5: コミット**

### Task 6: runtime parser の末尾カンマ拒否 (issue 8)

**Files:**
- Modify: `include/lexborpp/runtime_css_parser.hpp:157` 付近
- Test: `test/test_lexborpp.cpp`

- [ ] **Step 1: 失敗するテスト** — `query_selector(root, "div,")` / `"div,   "` が unexpected。`"div, span"` は有効。
- [ ] **Step 2: 失敗確認**
- [ ] **Step 3: 実装** — カンマ消費後 `if (pos >= input.size()) return std::unexpected(std::errc::invalid_argument);` を追加 (NTTP と同じ契約)。
- [ ] **Step 4: 全テスト**
- [ ] **Step 5: コミット**

### Task 7: CMake — Lexbor usage requirements 保持 (issue 3)

**Files:**
- Modify: `CMakeLists.txt:56-91`
- Modify: `cmake/lexborppConfig.cmake.in`

**Interfaces:**
- Produces: `LEXBORPP_LEXBOR_TARGET` (`lexbor::lexbor_static` → `lexbor::lexbor` → フォールバック生成の順で解決) を `target_link_libraries(lexborpp INTERFACE ...)` に使う。config テンプレートも同一ロジック。

- [ ] **Step 1: 実装**
- [ ] **Step 2: クリーン reconfigure + ビルド + 全テスト**。CMake キャッシュ / link.txt で `lexbor::lexbor_static` 参照と `LEXBOR_STATIC` 定義 (`-D LEXBOR_STATIC` が compile command に出ないことを INTERFACE_COMPILE_DEFINITIONS 経由の伝搬で確認 — vcpkg static triplet は既に static ライブラリなのでビルド成功とターゲット参照で判定) を確認。
- [ ] **Step 3: コミット (Task 8 とまとめてもよい)**

### Task 8: WASI minimal 定義の INTERFACE 伝搬 + エラー経路統一 (issue 4)

**Files:**
- Modify: `CMakeLists.txt:13-15` (add_compile_definitions → target_compile_definitions INTERFACE)
- Modify: `include/lexborpp/config.hpp` (LEGACY LEXBORPP_THROW ブロック削除)
- Modify: `include/lexborpp/nttp_parser.hpp` (`LEXBORPP_THROW` → `LEXBORPP_CONSTEVAL_FAIL`、replaceAll)
- Modify: `test/test_support.hpp` (plain throw へ)
- Modify: `test/smoke_wasi_minimal.cpp` (コメント更新)

- [ ] **Step 1: 実装**
- [ ] **Step 2: ビルド + 全テスト** (`smoke_wasi_minimal` が -fno-exceptions で通ること)
- [ ] **Step 3: 手動検証** — `cmake --install` → ステージング先で `find_package(lexborpp)` + `-fno-exceptions` + `LEXBORPP_WASI_MINIMAL=1` の consumer をビルドして成功すること。
- [ ] **Step 4: コミット**

### Task 9: README 同期 + スクリプト追跡 (issue 9, 10)

**Files:**
- Modify: `README.md` (55: wasmedge→wasmtime、219-228 の API 表、230 付近、303-304 の注意点、WASI セクション、as_element/set_text_content/属性 selector/上限の記述)
- Track: `build.sh` / `test.sh` (git add -f — ユーザーのグローバル gitignore 対策)
- Test: 256 バイト上限のテスト (runtime)

- [ ] **Step 1: 上限テスト追加** (`257 バイト selector → unexpected`)
- [ ] **Step 2: README 更新** (実 API・tracked files・CI と同期)
- [ ] **Step 3: 全テスト + git add -f build.sh test.sh + コミット**

### Task 10: 最終検証

- [ ] `./build.sh && ./test.sh` 全緑
- [ ] `git log` / `git status` 確認、Issue #5 の完了条件を一つずつ照合

## 完了条件との対応

| Issue の完了条件 | Task |
| --- | --- |
| 再現ケースを Catch2 回帰テストに追加 | Task 1,2,4,5,6,9 |
| 通常検索とインデックス検索が全 combinator で同じ結果 | Task 2 |
| 非要素ノードへの変換・DOM 変更が安全に失敗 | Task 1 |
| vcpkg static / MSVC でビルド・リンクできる | Task 7 (MSVC は構造的解消のみ) |
| インストール済み consumer を -fno-exceptions + WASI minimal でビルド | Task 8 |
| SVG type selector、空属性値、末尾カンマが仕様どおり | Task 4,5,6 |
| README を実 API・tracked files・CI に同期 | Task 9 |
