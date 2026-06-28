# AGENTS.md

`lexborpp` の作業エージェント向けメモ。Lexbor を C++23/26 から扱いやすくするヘッダオンリーラッパー。

## クイックスタート

ビルドとテスト:

```bash
./build.sh          # configure + build (build/ を作成)
./test.sh           # cd build && ctest -V
```

特定の Catch2 ケースだけ回したいとき:

```bash
./build/test/all_test "range adapters tag id clazz attr filter expected nodes"
```

## ビルドの注意点

- `build.sh` は **`~/vm/vcpkg`** を `VCPKG_ROOT` として固定で参照している (`readlink -f` で解決)。別の場所に vcpkg がある場合は環境変数を export してもスクリプト側で上書きされるので、スクリプトを編集するか該当パスにシンボリックリンクを置く。
- vcpkg トリプレットは **`x64-linux-static`** がハードコード。macOS / Windows ではそのまま動かない。`./build.sh static` でも `build_static/` ができるだけで、トリプレット自体は同じ。
- `conanfile.py` がルートに存在すれば conan にフォールバックする分岐あり。両方が揃っている環境では vcpkg 経路が優先。
- コンパイラは **C++26 を優先、なければ C++23**。C++20 ではビルド不可 (`std::expected`、borrowed_range 対応などで C++23 が必須)。
- CMake 最低バージョンは **3.25**。

## リポジトリ構成

| パス | 役割 |
| --- | --- |
| `include/lexborpp.hpp` | 公開ヘッダ。**umbrella** で、`include/lexborpp/` 配下の 11 ファイル (`core.hpp`, `tag_name.hpp`, `serialize_runtime.hpp`, `nttp_parser.hpp`, `nttp_match.hpp`, `nttp_range_adapters.hpp`, `nttp_query.hpp`, `ranges_specializations.hpp`, `document_id_index.hpp`, `runtime_css_parser.hpp`, `runtime_css_match.hpp`) を再 include する。利用者向けの include パスは `<lexborpp.hpp>` のまま。 |
| `CMakeLists.txt` | ルート。`lexborpp::lexborpp` (INTERFACE) を export |
| `cmake/lexborppConfig.cmake.in` | `find_package(lexborpp CONFIG)` 用のテンプレ |
| `test/test_lexborpp.cpp` | Catch2 テスト本体 |
| `test/test_support.hpp` | `html_document_fixture` (RAII で Lexbor ドキュメントを保持) |
| `test/CMakeLists.txt` | `file(GLOB test_*.cpp)`、ターゲット名は `all_test` |
| `benchmark/bench_css_selector.cpp` | NTTP / Runtime / Naive の比較ベンチマーク |
| `benchmark/BENCHMARK.md` | ベンチマーク計測結果 |
| `vcpkg.json` | 依存: `lexbor`, `catch2` |
| `build.sh` / `test.sh` | vcpkg / conan 経由のラッパ |
| `context7.json` | context7 公開用。**API キーなので取り扱い注意** |

## API の癖 (README 以上の細目)

- **ルートノードも検索結果に含まれる**: `query_selector` / `query_selector_all` いずれの経路も開始ノード自身を探索対象に含む。`node_walker` も同様。
- **NTTP 版 CSS selector がある**: `query_selector<"div#a.b">(root)` / `query_selector_all<"...">(root)` でコンパイル時解析される。**未対応の構文はコンパイルエラー**。対応: `type`, `*`, `#id`, `.class`, `[attr=...]` (演算子: `= ~= |= ^= $= *=`), `,`, 子結合 `>`, 隣接兄弟 `+`, 後続兄弟 `~`。pseudo-class は未対応。
- **walkers は borrowed_range**: Lexbor の生ポインタ (`lxb_dom_node_t*`) をそのまま yield する。元の `lxb_html_document_t` の寿命内でのみ使用可。`document_ptr` よりも早く破棄しないこと。
- **`has_class` はトークン分割** / **`clazz<"...">` は完全一致**: 仕様の差。README の「注意点」セクション参照。
- **エラー契約**: `parse_html` は `std::expected<document_ptr, lxb_status_t>`、失敗時に `lxb_status_t` を返す。`query_selector` の失敗は `nullptr`、`query_selector_all` の失敗は空配列。

## テスト運用

- テストは Catch2 v3 (`Catch2::Catch2WithMain`)。CTest 登録は `-r junit` で JUnit 出力。
- 新しいテストファイルは `test/test_*.cpp` に置けば `file(GLOB)` で自動拾われる。明示的な再生成 (`cmake --build`) を忘れずに。
- Lexbor ドキュメントは `html_document_fixture` 経由で使うとパース失敗を例外で通知してくれる。直接 `lxb_html_document_create/destroy` を触る必要はない。

## スタイル / ワークフロー

- フォーマッタ・リンタの自動設定は **なし** (`.clang-format`, `.pre-commit-config.yaml` ともに未配置)。CI もない。
- GCC 系では `-Wall -Wextra -pedantic -O3 -march=native -g3` が既定 (`CMakeLists.txt`)。追加の警告は `target_compile_options` で個別に。
- ヘッダオンリーのため、API 変更後は umbrella `include/lexborpp.hpp` および `include/lexborpp/*.hpp` だけでなく `test/test_lexborpp.cpp` の挙動差分も確認する。
- C++26 機能を使う場合、コンパイラが対応しなければ自動的に C++23 にフォールバックする (`CMakeLists.txt:9-18`)。ただしコード側で C++26 前提の機能を入れると C++23 経路では壊れる。

## やってはいけないこと

- `context7.json` の `public_key` をコミットし直したり、外部公開リポジトリにそのまま残すのは避ける (このリポジトリ自体が公開なので現状 OK だが、フォークや移動時に注意)。
- `build.sh` の `VCPKG_ROOT` を安易に書き換えず、まずは環境側の `~/vm/vcpkg` を整える。
- `query_selector<"...">` の NTTP に未対応の構文を入れるとコンパイル時にエラーで止まる。動作確認より前に構文サポート状況を確認する。
