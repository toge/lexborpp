# lexborpp ヘッダ分割設計

| 項目 | 値 |
| --- | --- |
| ステータス | 承認済み (ブレインストーミング 2026-06-04) |
| スコープ | `include/lexborpp.hpp` の内部分割 (公開 API への影響なし) |
| 主動機 | 可読性・ナビゲート性 (編集時 1 タブで扱えるサイズに収める) |

## 背景

`include/lexborpp.hpp` は公開 API 全部を含む **シングルヘッダ** として意図的に運用されている (AGENTS.md 32 行目)。その性質は維持したまま、ソースの論理的関心事ごとにファイル分割し、エディタ上でのナビゲート性を改善する。

## ゴール

- 公開 include パス: **`<lexborpp.hpp>` ひとつ** (利用者体験は不変)
- 内部実装: `include/lexborpp/` 配下の分割ヘッダ
- 各分割ヘッダは独立して読み進められるサイズ (< 1,100 行目安)
- 既存テスト (`test/test_lexborpp.cpp`) が無修正で pass
- CMake、vcpkg、ビルドスクリプト、下流リポジトリは無変更

## 非ゴール (やらない)

- 公開 API の変更・追加・削除
- 新機能追加 (NTTP parser の機能拡張など)
- 性能改善
- conan / vcpkg 構成の刷新
- `cmake/lexborppConfig.cmake.in` の変更

## ファイル構成

分割前:

```
include/lexborpp.hpp       # 2,558 行、全 API
```

分割後:

```
include/
├── lexborpp.hpp                      # 公開ヘッダ (umbrella)
└── lexborpp/
    ├── core.hpp                      # ヘルパー + walker 4 種 + DOM 操作 API
    ├── tag_name.hpp                  # LXB_TAG_* switch
    ├── serialize_runtime.hpp         # 詳細 deleter + 文字列化 + 実行時 CSS query
    ├── nttp_parser.hpp               # NTTP 用 enum/struct/コンパイル時 parser
    ├── nttp_match.hpp                # キャッシュ + match + query impl
    ├── nttp_range_adapters.hpp       # tag / id / clazz / attr range adapter
    ├── nttp_query.hpp                # query_selector<"..."> / query_selector_all<"...">
    └── ranges_specializations.hpp    # std::ranges::enable_borrowed_range
```

## 各ファイルに含める行範囲

`include/lexborpp.hpp` の現行版を基準に、次のとおり切り出す。

| ファイル | 元の行範囲 | 想定行数 | 含まれる主な宣言 |
| --- | --- | --- | --- |
| `core.hpp` | 1–103, 488–988 | ~600 | `to_string`, `is_non_element_node`, `as_element`, `node_walker`, `node_sibling_walker`, `node_prev_sibling_walker`, `attr_walker`, `has_class`, `get_first_element_by_class`, `get_elements_by_class`, `get_deep_text`, `set_attr`, `remove_attr`, `set_text_content`, `document_deleter`, `document_ptr`, `parse_html`, `get_root`, `get_element_by_id`, `get_attr_value`, `get_first_child_text`, `get_all_children_text`, `get_following_element_by_op`, `get_sibling_element_by_op` |
| `tag_name.hpp` | 990–1197 | ~210 | `tag_name` switch |
| `serialize_runtime.hpp` | 1199–1369 | ~170 | `detail::css_parser_deleter`, `detail::css_selectors_deleter`, `detail::selectors_deleter`, `detail::serialize_callback`, `outer_html`, `inner_html`, 実行時 `query_selector`, 実行時 `query_selector_all` |
| `nttp_parser.hpp` | 1387–2032 | ~650 | `detail::fixed_string`, `detail::selector_combinator`, `detail::selector_simple_kind`, `detail::selector_attribute_match`, `detail::is_space`, `detail::is_name_terminator`, `detail::ascii_lower`, `detail::iequals`, `detail::skip_spaces`, `detail::parse_name`, `detail::parse_quoted_value`, `detail::selector_simple_spec`, `detail::selector_prefilter_kind`, `detail::selector_prefilter_spec`, `detail::selector_group_shape`, `detail::selector_compound_spec`, `detail::selector_group_spec`, `detail::selector_spec`, `detail::simple_selector_priority`, `detail::build_prefilter`, `detail::classify_group_shape`, `detail::node_qualified_name`, `detail::prev_element_sibling`, `detail::parent_element`, `detail::match_attribute`, `detail::is_simple_selector_start`, `detail::parse_simple_selector` (宣言), `detail::parse_compound_selector` (宣言), `detail::parse_selector_spec`, `detail::parse_simple_selector` (定義), `detail::parse_compound_selector` (定義) |
| `nttp_match.hpp` | 2034–2468 | ~430 | `detail::selector_group_cache_key`, `detail::selector_group_cache_key_hash`, `detail::selector_group_cache`, `detail::selector_query_context`, `detail::selector_storage_capacity_v`, `detail::compiled_selector_v`, `detail::create_query_context`, `detail::match_simple_selector`, `detail::match_compound_selector`, `detail::match_prefilter`, `detail::match_selector_group`, `detail::match_selector_group_by_shape`, `detail::match_selector`, `detail::match_selector_group_compiled`, `detail::match_selector_compiled_impl`, `detail::match_selector_compiled`, `detail::query_selector_impl` (Max), `detail::query_selector_impl` (NTTP), `detail::query_selector_all_impl` (Max), `detail::query_selector_all_impl` (NTTP) |
| `nttp_range_adapters.hpp` | 1371–1385, 2470–2514 | ~60 | `tag<Tags...>`, `id<Id>`, `clazz<Classes...>`, `attr<Attr, Value>` |
| `nttp_query.hpp` | 2516–2538 | ~25 | `query_selector<Selector>`, `query_selector_all<Selector>` |
| `ranges_specializations.hpp` | 2540–2556 | ~20 | `std::ranges::enable_borrowed_range` 特殊化 (4 walker) |

## 依存関係と include 順

umbrella `lexborpp.hpp` は次の順で `#include` する。下流に依存するファイルが上に積まれるようにする。

```
#include "lexborpp/core.hpp"                 # ① 最下流。is_non_element_node, walker, get_attr_value, has_class など他全部が使う
#include "lexborpp/tag_name.hpp"             # ② core に依存しない
#include "lexborpp/serialize_runtime.hpp"    # ③ core の outer_html が core の関数を使う
#include "lexborpp/nttp_parser.hpp"          # ④ core の is_non_element_node を使う
#include "lexborpp/nttp_match.hpp"           # ⑤ nttp_parser + core の get_attr_value / has_class
#include "lexborpp/nttp_range_adapters.hpp"  # ⑥ core の get_attr_value
#include "lexborpp/nttp_query.hpp"           # ⑦ nttp_match + core の node_walker
#include "lexborpp/ranges_specializations.hpp" # ⑧ core の walker 型
```

依存は片方向 (上流から下流のみ) で循環なし。詳細:

- `tag_name.hpp` および `ranges_specializations.hpp` はそれぞれ独立
- `serialize_runtime.hpp` は Lexbor の C API のみ直接利用。`core.hpp` には依存しないが、編集順序の分かりやすさのため `core.hpp` の直後に置く
- `nttp_parser.hpp` → `nttp_match.hpp` の順: parser で型・テンプレートを宣言し、match で利用する
- `nttp_range_adapters.hpp` と `nttp_query.hpp` は独立 (前者: range adapter / 後者: query 関数)。`nttp_query.hpp` は `nttp_match.hpp` と `core.hpp` の両方に依存

各分割ヘッダは内部で `lexbor/html/parser.h` 等の Lexbor C API ヘッダを必要に応じて `#include` する。Lexbor ヘッダの再 `#include` はガードで多重防止されるため問題ない。

## umbrella の形

`include/lexborpp.hpp` の新実装は最小限の include 集合のみとする。

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

C++ 標準ヘッダ (`<algorithm>`, `<string>`, `<ranges>` 等) は `core.hpp` 側に集約し、umbrella には **書かない**。Lexbor C API ヘッダも同様。こうすると「umbrella は本プロジェクトの内部構造の目次」だけになる。

## 変更対象ファイル一覧

| 種別 | パス | 変更内容 |
| --- | --- | --- |
| 新規 | `include/lexborpp/core.hpp` | 1–103 + 488–988 を切り出し |
| 新規 | `include/lexborpp/tag_name.hpp` | 990–1197 を切り出し |
| 新規 | `include/lexborpp/serialize_runtime.hpp` | 1199–1369 を切り出し |
| 新規 | `include/lexborpp/nttp_parser.hpp` | 1387–2032 を切り出し |
| 新規 | `include/lexborpp/nttp_match.hpp` | 2034–2468 を切り出し |
| 新規 | `include/lexborpp/nttp_range_adapters.hpp` | 1371–1385 + 2470–2514 を切り出し |
| 新規 | `include/lexborpp/nttp_query.hpp` | 2516–2538 を切り出し |
| 新規 | `include/lexborpp/ranges_specializations.hpp` | 2540–2556 を切り出し |
| 変更 | `include/lexborpp.hpp` | 内容を umbrella 8 行に置き換え |
| 変更 | `AGENTS.md` | 32 行目を新構成に書き換え、スタイル節も追従 |
| 変更 | `README.md` | 「収録ファイル」節に umbrella である旨を追記 |

`CMakeLists.txt` / `vcpkg.json` / `build.sh` / `test.sh` / `test/*` / `cmake/*` は **無変更**。

## 検証方針

- `test/test_lexborpp.cpp` が無修正で pass すること (機能等価性の確認)
- `./build.sh` → `./test.sh` が緑
- 分割後の各ヘッダが単独で grep 等したときに「宣言が定義より先にない」ことを確認 (C++ のテンプレート要件)
- Catch2 1 ケース実行 (`./build/test/all_test "nttp"`) も追加で実施し、分割が NTTP 評価器に副作用を及ぼしていないことを確認

## ロールアウト

この作業は以下の単一ブランチ (現在の `main` 派生) で 1 コミットにまとめて反映する。

1. 新規分割ヘッダ 8 本を先に作成
2. umbrella `lexborpp.hpp` を 8 行に置き換え
3. ビルド + テストが緑であることを確認
4. ドキュメント (`AGENTS.md`, `README.md`) を更新
5. すべての変更を 1 コミットにまとめて push

下流 (例: `tenkijp_monitor`) 側のパッチは不要。

## リスクと緩和

| リスク | 緩和策 |
| --- | --- |
| include 順を誤って循環依存になる | `core.hpp` を最下流に固定し、依存方向を一本化する。テンプレート宣言と定義の順序は現状ファイルを尊重する |
| テンプレート実体化タイミングのずれで未解決シンボル | 現状ファイルと同じ宣言順を保ち、forward declaration を 1 ファイル内に閉じ込める |
| 余計な Lexbor ヘッダを引き込みビルド時間が増える | 各分割ヘッダは実際に使う Lexbor API のヘッダのみを include (現状の `lexborpp.hpp` と同じ 5 本で足りる) |
| エディタ / clangd のインデックスが壊れる | 分割は `#include` ツリー構造を変えるだけで、`target_include_directories` は `include/` のままなので影響なし |
