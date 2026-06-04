# lexborpp ヘッダ分割 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline) or superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `include/lexborpp.hpp` (2,558 行) を `include/lexborpp/` 配下の 8 ファイルに物理分割し、公開 include パス `<lexborpp.hpp>` は維持する。挙動の一切の変更なし。

**Architecture:** 公開ヘッダ `include/lexborpp.hpp` を 8 行の umbrella 化し、関心事ごとの 8 内部ヘッダ (`include/lexborpp/*.hpp`) を `#include` する。各内部ヘッダは現状の `lexborpp.hpp` から該当行範囲を切り出す。依存方向は片方向 (core を最下流)。

**Tech Stack:** C++23/26, CMake 3.25+, Catch2, vcpkg. 既存ツールチェーンをそのまま使う。

---

## ファイル構造

### 新規

| パス | 役割 |
| --- | --- |
| `include/lexborpp/core.hpp` | ヘルパー + 4 walker + DOM 操作 API |
| `include/lexborpp/tag_name.hpp` | `LXB_TAG_*` switch |
| `include/lexborpp/serialize_runtime.hpp` | detail deleter + 文字列化 + 実行時 CSS query |
| `include/lexborpp/nttp_parser.hpp` | NTTP 用 enum/struct/コンパイル時 parser |
| `include/lexborpp/nttp_match.hpp` | キャッシュ + match + query impl |
| `include/lexborpp/nttp_range_adapters.hpp` | `tag` / `id` / `clazz` / `attr` |
| `include/lexborpp/nttp_query.hpp` | `query_selector<"...">` / `query_selector_all<"...">` |
| `include/lexborpp/ranges_specializations.hpp` | `std::ranges::enable_borrowed_range` |

### 変更

| パス | 変更内容 |
| --- | --- |
| `include/lexborpp.hpp` | 8 行の umbrella に置換 |
| `AGENTS.md` | 32 行目 (構成表) とスタイル節を新構成に追従 |
| `README.md` | 「収録ファイル」節に umbrella である旨を追記 |

### 無変更

`CMakeLists.txt`, `vcpkg.json`, `build.sh`, `test.sh`, `test/*`, `cmake/*`

---

## タスク一覧

すべてのタスクを **単一の最終コミット** (`refactor: split lexborpp.hpp into umbrella + 8 internal headers`) にまとめて反映する。中間ビルド検証はステップに含めるが、コミットは最後。

---

### Task 1: ベースライン確立

**Files:** (なし — 確認のみ)

- [ ] **Step 1.1: クリーンビルド + 全テスト pass を確認**

実行:
```bash
cd /home/toge/src/lexborpp
rm -rf build
./build.sh
./test.sh
```

期待出力: `100% tests passed, 0 tests failed`。失敗したら本タスクを始める前に修正。

- [ ] **Step 1.2: ベースラインログを保存 (任意)**

実行:
```bash
ctest --test-dir build -V 2>&1 | tail -40
```

期待出力: 全 Catch2 ケースが pass のログ。これをリファクタ後の比較基準とする。

---

### Task 2: `include/lexborpp/core.hpp` 作成

**Files:**
- Create: `include/lexborpp/core.hpp`

- [ ] **Step 2.1: ディレクトリ作成 + 雛形ファイル作成**

実行:
```bash
mkdir -p /home/toge/src/lexborpp/include/lexborpp
touch /home/toge/src/lexborpp/include/lexborpp/core.hpp
```

- [ ] **Step 2.2: ヘッダガードと C++ 標準ヘッダ群を書く**

`include/lexborpp/core.hpp` の冒頭に以下を貼り付ける:

```cpp
#ifndef LEXBORPP_CORE_HPP_
#define LEXBORPP_CORE_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "lexbor/html/parser.h"
#include "lexbor/html/serialize.h"
#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"
#include "lexbor/selectors/selectors.h"

namespace lexborpp {
```

- [ ] **Step 2.3: ヘルパー関数 (1–103) を移植**

`include/lexborpp.hpp` の 1–103 行目 (`auto inline to_string(lxb_dom_node_t const* node) ...` から `auto constexpr inline as_element(lxb_dom_node_t const* node) noexcept -> lxb_dom_element_t*` 定義末まで) を **Doxygen コメントごと** コピーして `core.hpp` に貼り付ける。

注意: 先頭が `namespace lexborpp {` なので、貼り付け時に **その行をスキップ** する (Step 2.2 で既に書いている)。

- [ ] **Step 2.4: Walker 4 種 (104–486) を移植**

`include/lexborpp.hpp` の 104–486 行目をすべてコピーして `core.hpp` に貼り付ける。先頭の `// --- Walkers ---` コメントもそのまま。

- [ ] **Step 2.5: Core API (488–988) を移植**

`include/lexborpp.hpp` の 488–988 行目をすべてコピーして `core.hpp` に貼り付ける。`// --- Core API ---`, `// --- Document RAII ---`, `// --- Lookup & Attributes ---` の見出しもそのまま。

- [ ] **Step 2.6: ファイル末尾を閉じる**

`core.hpp` の末尾に以下を追加:

```cpp
}  // namespace lexborpp

#endif  // LEXBORPP_CORE_HPP_
```

- [ ] **Step 2.7: 確認**

`core.hpp` の `namespace lexborpp {` ブロックが 1 つだけであることを確認 (重複していないこと):

```bash
grep -c "^namespace lexborpp" /home/toge/src/lexborpp/include/lexborpp/core.hpp
```

期待出力: `1`

---

### Task 3: `include/lexborpp/tag_name.hpp` 作成

**Files:**
- Create: `include/lexborpp/tag_name.hpp`

- [ ] **Step 3.1: 雛形作成**

`include/lexborpp/tag_name.hpp` に以下を書く:

```cpp
#ifndef LEXBORPP_TAG_NAME_HPP_
#define LEXBORPP_TAG_NAME_HPP_

#include <string_view>

#include "lexbor/dom/dom.h"

namespace lexborpp {
```

- [ ] **Step 3.2: `tag_name` 本体 (990–1197) を移植**

`include/lexborpp.hpp` の 990–1197 行目 (`auto inline tag_name(lxb_tag_id_t const tag_id) {` から switch の `}` と `}` まで) をコピーして貼り付ける。

- [ ] **Step 3.3: ファイル末尾を閉じる**

`tag_name.hpp` の末尾に:

```cpp
}  // namespace lexborpp

#endif  // LEXBORPP_TAG_NAME_HPP_
```

---

### Task 4: `include/lexborpp/serialize_runtime.hpp` 作成

**Files:**
- Create: `include/lexborpp/serialize_runtime.hpp`

- [ ] **Step 4.1: 雛形作成**

`include/lexborpp/serialize_runtime.hpp` に:

```cpp
#ifndef LEXBORPP_SERIALIZE_RUNTIME_HPP_
#define LEXBORPP_SERIALIZE_RUNTIME_HPP_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "lexbor/html/serialize.h"
#include "lexbor/css/css.h"
#include "lexbor/selectors/selectors.h"
#include "lexbor/dom/dom.h"

namespace lexborpp {
```

- [ ] **Step 4.2: detail 名前空間の deleter / callback (1199–1251) を移植**

`include/lexborpp.hpp` の 1199 行目 (`namespace detail {`) から 1251 行目 (`} // namespace detail`) までをコピー。`detail::css_parser_deleter`, `detail::css_selectors_deleter`, `detail::selectors_deleter`, `detail::serialize_callback` を含む。

- [ ] **Step 4.3: 公開 serialize + 実行時 query (1253–1369) を移植**

`include/lexborpp.hpp` の 1253–1369 行目 (`/** @brief ノードの外部 HTML ...` から `query_selector_all` 関数の `}` まで) をコピー。`outer_html`, `inner_html`, 実行時版 `query_selector`, 実行時版 `query_selector_all` を含む。

- [ ] **Step 4.4: ファイル末尾を閉じる**

`serialize_runtime.hpp` の末尾に:

```cpp
}  // namespace lexborpp

#endif  // LEXBORPP_SERIALIZE_RUNTIME_HPP_
```

---

### Task 5: `include/lexborpp/nttp_parser.hpp` 作成

**Files:**
- Create: `include/lexborpp/nttp_parser.hpp`

- [ ] **Step 5.1: 雛形作成**

`include/lexborpp/nttp_parser.hpp` に:

```cpp
#ifndef LEXBORPP_NTTP_PARSER_HPP_
#define LEXBORPP_NTTP_PARSER_HPP_

#include <array>
#include <cstddef>
#include <ranges>
#include <string_view>

#include "lexbor/dom/dom.h"
#include "lexbor/css/css.h"

namespace lexborpp {

namespace detail {
```

- [ ] **Step 5.2: enum / struct / パーサ (1387–2032) を移植**

`include/lexborpp.hpp` の 1387 行目 (`namespace detail {`) から 2032 行目 (`parse_compound_selector` 定義の `}` と `}` まで) までをコピー。`fixed_string`, 各 enum, 各 spec struct, `simple_selector_priority`, `build_prefilter`, `classify_group_shape`, `node_qualified_name`, `prev_element_sibling`, `parent_element`, `match_attribute`, `is_simple_selector_start`, `parse_selector_spec`, `parse_simple_selector`, `parse_compound_selector` の **forward declaration と定義の両方** を含む。

- [ ] **Step 5.3: ファイル末尾を閉じる**

`nttp_parser.hpp` の末尾に:

```cpp
}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_PARSER_HPP_
```

---

### Task 6: `include/lexborpp/nttp_match.hpp` 作成

**Files:**
- Create: `include/lexborpp/nttp_match.hpp`

- [ ] **Step 6.1: 雛形作成**

`include/lexborpp/nttp_match.hpp` に:

```cpp
#ifndef LEXBORPP_NTTP_MATCH_HPP_
#define LEXBORPP_NTTP_MATCH_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {

namespace detail {
```

- [ ] **Step 6.2: キャッシュ・match 関数 (2034–2468) を移植**

`include/lexborpp.hpp` の 2034 行目 (`struct selector_group_cache_key {`) から 2468 行目 (`} // namespace detail`) までをコピー。`selector_group_cache_key`, `selector_group_cache_key_hash`, `selector_group_cache`, `selector_query_context`, `selector_storage_capacity_v`, `compiled_selector_v`, `create_query_context`, `match_simple_selector`, `match_compound_selector`, `match_prefilter`, `match_selector_group`, `match_selector_group_by_shape`, `match_selector`, `match_selector_group_compiled`, `match_selector_compiled_impl`, `match_selector_compiled`, `query_selector_impl` (Max), `query_selector_impl` (NTTP), `query_selector_all_impl` (Max), `query_selector_all_impl` (NTTP) を含む。

- [ ] **Step 6.3: ファイル末尾を閉じる**

`nttp_match.hpp` の末尾に:

```cpp
}  // namespace detail
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_MATCH_HPP_
```

---

### Task 7: `include/lexborpp/nttp_range_adapters.hpp` 作成

**Files:**
- Create: `include/lexborpp/nttp_range_adapters.hpp`

- [ ] **Step 7.1: 雛形作成**

`include/lexborpp/nttp_range_adapters.hpp` に:

```cpp
#ifndef LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_
#define LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_

#include <ranges>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {
```

- [ ] **Step 7.2: `tag<Tags...>` (1371–1385) を移植**

`include/lexborpp.hpp` の 1371–1385 行目 (`/** @brief 指定したタグ名 ...` から `tag<Tags...>` 定義の `};` まで) をコピー。

- [ ] **Step 7.3: `id` / `clazz` / `attr` (2470–2514) を移植**

`include/lexborpp.hpp` の 2470 行目 (`/** @brief 指定した id 属性値 ...` から 2514 行目 (`attr<Attr, Value>` 定義の `};` まで) をコピー。

- [ ] **Step 7.4: ファイル末尾を閉じる**

`nttp_range_adapters.hpp` の末尾に:

```cpp
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_RANGE_ADAPTERS_HPP_
```

---

### Task 8: `include/lexborpp/nttp_query.hpp` 作成

**Files:**
- Create: `include/lexborpp/nttp_query.hpp`

- [ ] **Step 8.1: 雛形作成**

`include/lexborpp/nttp_query.hpp` に:

```cpp
#ifndef LEXBORPP_NTTP_QUERY_HPP_
#define LEXBORPP_NTTP_QUERY_HPP_

#include <string_view>
#include <vector>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
#include "lexborpp/nttp_match.hpp"
#include "lexborpp/nttp_parser.hpp"

namespace lexborpp {
```

- [ ] **Step 8.2: `query_selector<"...">` / `query_selector_all<"...">` (2516–2538) を移植**

`include/lexborpp.hpp` の 2516 行目 (`/** @brief NTTP で渡された CSS セレクタ ...`) から 2538 行目 (`query_selector_all<Selector>` 定義の `}` まで) をコピー。

- [ ] **Step 8.3: ファイル末尾を閉じる**

`nttp_query.hpp` の末尾に:

```cpp
}  // namespace lexborpp

#endif  // LEXBORPP_NTTP_QUERY_HPP_
```

---

### Task 9: `include/lexborpp/ranges_specializations.hpp` 作成

**Files:**
- Create: `include/lexborpp/ranges_specializations.hpp`

- [ ] **Step 9.1: 雛形作成**

`include/lexborpp/ranges_specializations.hpp` に:

```cpp
#ifndef LEXBORPP_RANGES_SPECIALIZATIONS_HPP_
#define LEXBORPP_RANGES_SPECIALIZATIONS_HPP_

#include <ranges>

#include "lexbor/dom/dom.h"

#include "lexborpp/core.hpp"
```

- [ ] **Step 9.2: `std::ranges` 特殊化 (2540–2556) を移植**

`include/lexborpp.hpp` の 2540 行目 (`} // namespace lexborpp` の **直後** にある `namespace std::ranges {`) から 2556 行目 (`}  // namespace std::ranges`) までをコピー。

注意: 元ファイルでは `namespace lexborpp` の閉じ括弧が先にあって `namespace std::ranges` が後に続く。本ファイルでは `namespace std::ranges` のみを貼り付け、 `lexborpp` の閉じ括弧は **書かない**。

- [ ] **Step 9.3: ファイル末尾を閉じる**

`ranges_specializations.hpp` の末尾に:

```cpp
#endif  // LEXBORPP_RANGES_SPECIALIZATIONS_HPP_
```

---

### Task 10: umbrella `include/lexborpp.hpp` 置き換え

**Files:**
- Modify: `include/lexborpp.hpp` (全置換)

- [ ] **Step 10.1: 既存の中身をバックアップ (任意)**

```bash
cp /home/toge/src/lexborpp/include/lexborpp.hpp /tmp/opencode/lexborpp.hpp.bak
```

- [ ] **Step 10.2: 既存ファイルを全置換**

`include/lexborpp.hpp` を次の内容で **完全に上書き**:

```cpp
#ifndef LEXBORPP_HPP_
#define LEXBORPP_HPP_

#include "lexborpp/core.hpp"
#include "lexborpp/tag_name.hpp"
#include "lexborpp/serialize_runtime.hpp"
#include "lexborpp/nttp_parser.hpp"
#include "lexborpp/nttp_match.hpp"
#include "lexborpp/nttp_range_adapters.hpp"
#include "lexborpp/nttp_query.hpp"
#include "lexborpp/ranges_specializations.hpp"

#endif  // LEXBORPP_HPP_
```

- [ ] **Step 10.3: ファイル長を確認**

```bash
wc -l /home/toge/src/lexborpp/include/lexborpp.hpp
```

期待出力: `12` (8 include + 4 ガード行 + 空行)

---

### Task 11: ビルド + 全テストで等価性検証

**Files:** (なし — 検証のみ)

- [ ] **Step 11.1: フルクリーンビルド**

```bash
cd /home/toge/src/lexborpp
rm -rf build
./build.sh
```

期待: エラー / 警告なしで完了 (Task 1 と同じビルド成功)。

- [ ] **Step 11.2: 全テスト実行**

```bash
./test.sh
```

期待: `100% tests passed, 0 tests failed` (Task 1 と同じ結果)。

- [ ] **Step 11.3: NTTP 経路の個別テスト**

```bash
./build/test/all_test "nttp"
```

期待: NTTP 関連のケースがすべて pass。コンパイル時 CSS parser / evaluator が新構成でも動作することを確認。

- [ ] **Step 11.4: range adapter の個別テスト**

```bash
./build/test/all_test "range adapters"
```

期待: `tag` / `id` / `clazz` / `attr` の range adapter 関連ケースが pass。

- [ ] **Step 11.5: 失敗時の対応**

Task 11.2 〜 11.4 のいずれかが失敗した場合:

1. 直近に編集した内部ヘッダの include 漏れ / 順序誤りを疑う
2. `clang++ -std=c++26 -I include -I ~/vm/vcpkg/installed/x64-linux-static/include ...` で個別 TU を再コンパイルし、エラーメッセージから原因ヘッダを特定
3. 修正後 Task 11.1 に戻ってフルリビルド

---

### Task 12: `AGENTS.md` 更新

**Files:**
- Modify: `AGENTS.md`

- [ ] **Step 12.1: 32 行目 (リポジトリ構成表) を更新**

`AGENTS.md` の 32 行目を以下に置換:

```markdown
| `include/lexborpp.hpp` | 公開ヘッダ。**umbrella** で、`include/lexborpp/` 配下の 8 ファイル (`core.hpp`, `tag_name.hpp`, `serialize_runtime.hpp`, `nttp_parser.hpp`, `nttp_match.hpp`, `nttp_range_adapters.hpp`, `nttp_query.hpp`, `ranges_specializations.hpp`) を再 include する。利用者向けの include パスは `<lexborpp.hpp>` のまま。 |
```

- [ ] **Step 12.2: スタイル節の「ヘッダオンリー」記述を修正**

`AGENTS.md` の 61 行目を以下に置換:

```markdown
- ヘッダオンリーのため、API 変更後は `include/lexborpp.hpp` (umbrella) および `include/lexborpp/*.hpp` の挙動差分も確認する。
```

---

### Task 13: `README.md` 更新

**Files:**
- Modify: `README.md`

- [ ] **Step 13.1: 「収録ファイル」節の `include/lexborpp.hpp` 説明を追記**

`README.md` の「収録ファイル」節 (286–294 行目) を確認。`include/lexborpp.hpp` の項目を以下に置換:

```markdown
- `include/lexborpp.hpp`
  - 公開 API の umbrella ヘッダ。実体は `include/lexborpp/` 配下の関心事ごとの 8 ヘッダに分かれており、本ヘッダがそれを再 `#include` する。利用者側はこれまで通り `#include <lexborpp.hpp>` だけで全 API が使える。
- `include/lexborpp/`
  - 内部実装の分割ヘッダ (core / tag_name / serialize_runtime / nttp_parser / nttp_match / nttp_range_adapters / nttp_query / ranges_specializations)。直接 include することは想定しない。
```

---

### Task 14: 単一コミットに反映

**Files:** (なし — コミットのみ)

- [ ] **Step 14.1: `git status` で変更対象を確認**

```bash
cd /home/toge/src/lexborpp
git status
```

期待: 以下のファイルが unstaged / untracked として表示される:

- 新規: `include/lexborpp.hpp`, `include/lexborpp/*.hpp` (8 ファイル), `AGENTS.md`, `README.md`
- 変更: なし (umbrella の中身が丸ごと置き換わるため、新規ファイル扱いになる)

- [ ] **Step 14.2: 全変更をステージング**

```bash
git add include/lexborpp.hpp include/lexborpp/
git add AGENTS.md README.md
git status
```

期待: 11 ファイル (umbrella 1 + 内部 8 + AGENTS.md 1 + README.md 1) が staged。

- [ ] **Step 14.3: コミット**

```bash
git commit -m "refactor: split lexborpp.hpp into umbrella + 8 internal headers

公開 include パス <lexborpp.hpp> は維持したまま、内部実装を関心事ごと
の 8 ヘッダ (core / tag_name / serialize_runtime / nttp_parser /
nttp_match / nttp_range_adapters / nttp_query / ranges_specializations)
に物理分割する。公開 API は無変更。テストは既存 Catch2 ケースを無修正
で pass することを確認済み。詳細: docs/superpowers/specs/
2026-06-04-lexborpp-header-split-design.md"
```

期待: `[main <hash>] refactor: split ...` のコミットが新規作成される。

- [ ] **Step 14.4: コミットログ確認**

```bash
git log --oneline -5
```

期待: 新しいコミットが最新に表示される。

- [ ] **Step 14.5: 最終ビルド + テスト (リグレッションなし確認)**

```bash
cd /home/toge/src/lexborpp
rm -rf build
./build.sh
./test.sh
```

期待: 全 pass。コミット後の状態で再現性があることを最終確認。

---

## 自己レビュー (実装前)

- [x] **Spec coverage:** 全 11 ファイル (新規 8 + 変更 3) をカバー。Task 2–9 が新規 8 ヘッダ、Task 10 が umbrella、Task 12–13 が AGENTS.md/README.md、Task 11/14 が検証。
- [x] **Placeholder scan:** 「TBD / TODO / 後から / 適切に」の類なし。すべてのステップで具体的な行範囲 / コード / コマンドを示す。
- [x] **Type consistency:** 既存識別子 (`is_non_element_node`, `as_element`, `node_walker`, `query_selector`, `tag`, `id`, `clazz`, `attr`, `enable_borrowed_range` 等) はすべて元のまま。`detail` namespace の内部シンボルもすべて元のまま。
- [x] **依存方向:** core → nttp_parser → nttp_match → nttp_query / nttp_range_adapters / ranges_specializations。include 順は spec 通りで循環なし。
- [x] **リスク軽減:** Task 11 で段階的にテストし、失敗時のデバッグ手順も明示。
