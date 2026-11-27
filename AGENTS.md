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

- `build.sh` は未設定時の `VCPKG_ROOT` を `~/vm/vcpkg` にしつつ、呼び出し側から設定された path を上書きせずに使う。無効な path / vcpkg checkout は明示的なエラーで停止する。
- `VCPKG_TARGET_TRIPLET` も呼び出し側の値を尊重し、未設定時は `x64-linux-static`。値は `-DVCPKG_TARGET_TRIPLET=...` として CMake cache に明示的に渡す。`./build.sh static` は build directory を `build_static/` に変えるだけで、triplet を別の値へ固定しない。
- `VCPKG_OVERLAY_PORTS` は呼び出し側が設定した場合だけ引き継ぎ、スクリプト側の既定値は設定しない。
- 既存 build directory の `CMAKE_TOOLCHAIN_FILE` が今回指定した vcpkg toolchain と異なる場合、configure は停止する。`VCPKG_ROOT` を変更するときは clean な directory または新しい build directory を使う。
- 最適化フラグは CMake の **`LEXBORPP_NATIVE_ARCH`** オプションに一本化されている (`build.sh` は ON で渡す)。`-Ofast` (fast-math) は使わない。
- コンパイラは **C++26 を優先、なければ C++23**。C++20 ではビルド不可 (`std::expected`、borrowed_range 対応などで C++23 が必須)。
- CMake 最低バージョンは **3.25**。

## リポジトリ構成

| パス | 役割 |
| --- | --- |
| `include/lexborpp.hpp` | 公開ヘッダ。**umbrella** で、`include/lexborpp/` 配下の 11 ファイル (`core.hpp`, `tag_name.hpp`, `serialize_runtime.hpp`, `nttp_parser.hpp`, `nttp_match.hpp`, `nttp_range_adapters.hpp`, `nttp_query.hpp`, `ranges_specializations.hpp`, `document_id_index.hpp`, `runtime_css_parser.hpp`, `runtime_css_match.hpp`) を再 include する。利用者向けの include パスは `<lexborpp.hpp>` のまま。 |
| `CMakeLists.txt` | ルート。`lexborpp::lexborpp` (INTERFACE) を export |
| `cmake/lexborppConfig.cmake.in` | `find_package(lexborpp CONFIG)` 用のテンプレ。利用側で Lexbor を再解決する |
| `cmake/lexborppResolveLexbor.cmake` | config target / raw header・library の共通 Lexbor resolver |
| `test/test_lexborpp.cpp` | Catch2 テスト本体 |
| `test/test_support.hpp` | `html_document_fixture` (RAII で Lexbor ドキュメントを保持) |
| `test/CMakeLists.txt` | `file(GLOB test_*.cpp)`、ターゲット名は `all_test` |
| `benchmark/bench_css_selector.cpp` | NTTP / Runtime / Naive の比較ベンチマーク |
| `benchmark/BENCHMARK.md` | ベンチマーク計測結果 |
| `vcpkg.json` | 依存: `lexbor`, `catch2` |
| `build.sh` / `test.sh` | vcpkg 経由のビルド / テストラッパ (`test.sh` は第1引数でビルドディレクトリ指定可) |
| `context7.json` | context7 公開用。**API キーなので取り扱い注意** |

## CMake の Lexbor 解決

- install export はビルド時に使った外部 Lexbor target 名や絶対 path を埋め込まず、安定した内部 wrapper `lexborpp::lexbor` だけを `lexborpp::lexborpp` から参照する。
- 利用側の package config は、`lexbor::lexbor_static` → `lexbor::lexbor` の順に config target を探し、なければ raw header / library fallback を使う。
- raw fallback の cache variable は `LEXBORPP_RAW_LEXBOR_LINKAGE=AUTO|STATIC|SHARED`。`AUTO` の generic `lexbor` は shared 扱いなので、generic 名の static library は `STATIC` を明示する。
- raw static では `LEXBOR_STATIC` を伝搬する。shared では伝搬しない。不正な linkage 値や指定内容に合う header / library が不足する組み合わせは configure 失敗にする。

## API の癖 (README 以上の細目)

- **ルートノードも検索結果に含まれる**: `query_selector` / `query_selector_all` いずれの経路も開始ノード自身を探索対象に含む。`node_walker` も同様。
- **NTTP 版 CSS selector がある**: `query_selector<"div#a.b">(root)` / `query_selector_all<"...">(root)` でコンパイル時解析される。**未対応の構文はコンパイルエラー**。対応: `type`, `*`, `#id`, `.class`, `[attr=...]` (演算子: `= ~= |= ^= $= *=`), `,`, 子結合 `>`, 隣接兄弟 `+`, 後続兄弟 `~`。pseudo-class は未対応。
- **walkers は borrowed_range**: Lexbor の生ポインタ (`lxb_dom_node_t*`) をそのまま yield する。元の `lxb_html_document_t` の寿命内でのみ使用可。`document_ptr` よりも早く破棄しないこと。
- **`has_class` はトークン分割** / **`clazz<"...">` は完全一致**: 仕様の差。README の「注意点」セクション参照。
- **エラー契約**: `parse_html` は `std::expected<document_ptr, lxb_status_t>`、失敗時に `lxb_status_t` を返す。実行時版 `query_selector` / `query_selector_all` も `std::expected` を返し、不正 selector・不正引数は `std::errc`、一部の割り当て失敗は `std::errc::not_enough_memory`。妥当な selector で未一致の場合だけ `nullptr` / 空 vector を成功値として返す。NTTP 版は raw pointer / vector を返す。
- **`document_id_index` はスナップショット**: 構築時の root と id map を保持し、DOM 編集 (id 変更・ノード削除) には追従しない。編集後や root 変更後は `rebuild()` 必須。重複時も `find(id)` は最初のノードを返し、`size()` は一意な id 数。`is_ambiguous(id)` で重複判定、`covers_scope(scope)` で index root が scope 自身 / 祖先かを判定できる。
- **indexed query のフォールバック**: runtime / NTTP の fast path は右端 compound の一意な id かつ index root が query scope を覆う場合だけ使う。重複 id または scope 外の index は通常走査に戻るため、first / all の結果は非 indexed query と一致する。

## テスト運用

- テストは Catch2 v3 (`Catch2::Catch2WithMain`)。CTest 登録は `-r junit` で JUnit 出力。
- 新しいテストファイルは `test/test_*.cpp` に置けば `file(GLOB)` で自動拾われる。明示的な再生成 (`cmake --build`) を忘れずに。
- Lexbor ドキュメントは `html_document_fixture` 経由で使うとパース失敗を例外で通知してくれる。直接 `lxb_html_document_create/destroy` を触る必要はない。
- installed package / add_subdirectory / raw static・shared / missing dependency の CMake consumer matrix:

```bash
ctest --test-dir build -R "^package_consumer_matrix$" -V -C Release
```

## スタイル / ワークフロー

- フォーマッタ・リンタの自動設定は **なし** (`.clang-format`, `.pre-commit-config.yaml` ともに未配置)。CI は `.github/workflows/ci.yml` (linux / msvc / macos / clang / emscripten の 5 ジョブ)。
- 警告フラグ (`-Wall -Wextra -pedantic`) は top-level ビルド時のみ `add_compile_options` で適用 (GNU/Clang 判定、clang にも有効)。`-g3` は Debug 構成のみ。最適化は Release 構成で `-O2`、`LEXBORPP_NATIVE_ARCH=ON` 時は `-O3 -march=native`。
- ヘッダオンリーのため、API 変更後は umbrella `include/lexborpp.hpp` および `include/lexborpp/*.hpp` だけでなく `test/test_lexborpp.cpp` の挙動差分も確認する。
- C++26 機能を使う場合、コンパイラが対応しなければ自動的に C++23 にフォールバックする (`CMakeLists.txt`)。ただしコード側で C++26 前提の機能を入れると C++23 経路では壊れる。

## やってはいけないこと

- `context7.json` の `public_key` をコミットし直したり、外部公開リポジトリにそのまま残すのは避ける (このリポジトリ自体が公開なので現状 OK だが、フォークや移動時に注意)。
- `build.sh` のローカル vcpkg path / triplet をハードコードせず、呼び出し側の `VCPKG_ROOT` / `VCPKG_TARGET_TRIPLET` で切り替える。
- `query_selector<"...">` の NTTP に未対応の構文を入れるとコンパイル時にエラーで止まる。動作確認より前に構文サポート状況を確認する。
