# PR #6 追加レビュー指摘 1–5 修正設計

## 目的

PR #6 の追加レビュー指摘 1–5 を修正し、重複 ID を含む検索結果、インストール済み
Lexbor 依存関係の解決、raw 静的リンク、ローカルビルドスクリプトの各経路を一貫させる。
既存の公開 API と `document_id_index::find()` の「重複 ID は最初のノード」という契約は維持する。

## 非目標

- 重複 ID の全ノードを保持する新しいインデックス API は追加しない。
- CSS selector のサポート範囲を拡張しない。
- `build.sh` をクロスプラットフォーム汎用スクリプトへ拡張しない。
- DOM 更新後のスナップショットを自動追従させない。

## 方針

### 1. 重複 ID では通常走査へフォールバックする

`document_id_index` の map entry に `first` と `ambiguous` を保持し、構築時の
root pointer も保持する。既存の `find(id)`、`contains(id)`、`size()` の意味は
変えず、重複検出を `is_ambiguous(id)`、検索範囲の包含判定を
`covers_scope(scope)` で公開する。`covers_scope(scope)` は index root が scope
自身または scope の祖先である場合だけ `true` を返す。

runtime / NTTP の indexed query は、右端 compound に ID があり、
`index.covers_scope(node)` が true で、かつ `!index.is_ambiguous(id)` の場合だけ
単一ノード fast path を使う。それ以外は通常の DOM 走査を呼ぶ。したがって
重複 ID、検索範囲より狭い index root、スコープ外候補、追加 simple selector を含む
検索でも通常検索と同じ first/all 結果になる。インデックス作成前後の DOM 編集に
ついては従来どおり `rebuild()` が必要である。

### 2. インストール用の安定した Lexbor wrapper target を持つ

lexborpp 自身が所有する `lexborpp_lexbor` INTERFACE target を作り、
export 名を `lexborpp::lexbor` とする。`lexborpp::lexborpp` は
ビルド時に見つかった外部 Lexbor target 名ではなく、この wrapper だけに依存する。

build tree では wrapper を解決済みの
`lexbor::lexbor_static`、`lexbor::lexbor`、raw library の順に配線する。ただし
install interface では外部 Lexbor target 名や raw path を空にし、
`$<BUILD_INTERFACE:...>` だけに閉じ込める。export file は
`lexborpp::lexborpp -> lexborpp::lexbor` という安定した内部参照のみを含む。

install consumer では package config が利用者側の依存関係を先に解決し、export
target を読み込んだ後に `lexborpp::lexbor` の `INTERFACE_LINK_LIBRARIES` /
`INTERFACE_INCLUDE_DIRECTORIES` / `INTERFACE_COMPILE_DEFINITIONS` を再設定する。
`lexborppTargets.cmake` に `lexbor::lexbor_static`、`lexbor::lexbor`、ビルド
マシンの絶対 header / library path が出力されないことを回帰テストで確認する。

### 3. raw dependency fallback を package config から到達可能にする

package config では `find_package(lexbor CONFIG QUIET)` を使い、target が見つからない
場合に `find_path()` / `find_library()` へ進む。`find_dependency()` の失敗で
package config が即終了する構成は使わない。どちらの経路でも解決できなければ
`lexborpp_FOUND=FALSE` と理由を設定して return する。

raw lookup は
`LEXBORPP_RAW_LEXBOR_LINKAGE=AUTO|STATIC|SHARED` で探索規則まで固定する。
cache variable が未設定なら `AUTO` を既定値にする。

| 値 | 探索対象 | 選択時の linkage |
| --- | --- | --- |
| `AUTO` | `lexbor_static`、見つからなければ generic `lexbor` | 先頭なら static、generic なら shared |
| `STATIC` | `lexbor_static`、見つからなければ generic `lexbor` | どちらも static |
| `SHARED` | generic `lexbor` のみ | shared |

許可する値は `AUTO`、`STATIC`、`SHARED` の3つだけであり、それ以外と
大小文字違いは構成エラーとする。`SHARED` で
`lexbor_static` しか存在しない場合、または `STATIC` / `SHARED` 指定の探索で
library が見つからない場合は、不完全な target を作らず失敗する。
generic `lexbor.lib` / `liblexbor.a` は basename だけでは import library と
static library を判別できないため、利用者が linkage を知っている場合は
`STATIC` / `SHARED` を明示する。header / library のみの top-level build と
installed package consumer の双方で同じ規則を使う。

### 4. raw 静的 Lexbor に `LEXBOR_STATIC` を伝搬する

`lexbor_static` を選択した経路では wrapper の
`INTERFACE_COMPILE_DEFINITIONS` に `LEXBOR_STATIC` を設定する。consumer 側から
raw target を再配線する場合も同じ定義を設定し、MSVC で
`LXB_API == __declspec(dllimport)` になることを防ぐ。raw 共有ライブラリ経路では
この定義を付けない。generic `lexbor` を `LEXBORPP_RAW_LEXBOR_LINKAGE=STATIC`
で使う場合も、static と明示されたので `LEXBOR_STATIC` を設定する。linkage の自動判定
を曖昧な名前に基づいて行わない。

### 5. `build.sh` の triplet と VCPKG_ROOT を明示する

`VCPKG_ROOT` は未設定なら従来の `~/vm/vcpkg` を使い、設定済みならそれを尊重する。
triplet は環境変数または既定値 `x64-linux-static` を取り、CMake cache variable
`-DVCPKG_TARGET_TRIPLET=...` として必ず渡す。既存の
`./build.sh` / `./build.sh static` のビルドディレクトリ契約は維持する。

## エラー契約

- 重複 ID による indexed fast path の失敗を、無一致や部分結果として公開しない。
- raw dependency を解決できない場合は `find_package(lexborpp CONFIG)` を失敗させ、
  不完全な imported target を生成しない。
- raw static で `LEXBOR_STATIC` が付かなければ構成エラーにする。
- `build.sh` は無効な `VCPKG_ROOT` で明示的に停止し、resolve 失敗を黙って使わない。

## テスト

1. Catch2 で `id="x"` が2個あるDOMを作り、runtime / NTTP の
   first / all、`#x` と `#x.wanted` が indexed / non-indexed で一致することを固定する。
2. `document_id_index::find()` は重複 ID でも従来どおり最初のノードを返し、
   `is_ambiguous()` が `true` になることを確認する。
3. インストール consumer を次の3形態で構成・リンクする。
   - vcpkg の `lexbor::lexbor_static`
   - `lexbor::lexbor` のみを提供する Lexbor package config
   - package config なしの raw header + static library
4. raw linkage matrix を実コンパイル / リンクで確認する。
   - `AUTO + lexbor_static`: `LEXBOR_STATIC` が定義され、リンク成功
   - `AUTO + generic lexbor`: `LEXBOR_STATIC` が未定義で、リンク成功
   - `STATIC + generic lexbor`: `LEXBOR_STATIC` が定義され、リンク成功
   - `SHARED + generic lexbor`: `LEXBOR_STATIC` が未定義で、リンク成功
   - `SHARED + lexbor_static のみ`: configure 失敗
   - 無効な linkage 値: configure 失敗
   - MSVC shared consumer: `LXB_API` が import 用になる実ヘッダをコンパイルし、
     `LEXBOR_STATIC` を要求する側のスモークでは失敗させない
   Visual Studio generator でも compile_commands.json の有無に依存せず、
   ソース中の `#ifdef LEXBOR_STATIC` / `#ifndef` assertion と実リンクで判定する。
5. `./build.sh static` 後の `CMakeCache.txt` が
   `VCPKG_TARGET_TRIPLET=x64-linux-static` になり、呼び出し側の `VCPKG_ROOT`
   override が維持されることを確認する。
6. `add_subdirectory()` consumer、raw 共有 library consumer、依存が完全に存在しない
   consumer を CTest に追加し、それぞれリンク成功 / configure 失敗理由を確認する。
7. 重複 ID の先頭候補が query scope 外、先頭候補が追加 simple selector を
   満たさない、後続候補が満たす、複数候補を all で返す各ケースを runtime /
   NTTP で確認する。さらに index root が query root より狭く、index 外の重複 ID
   だけが selector に一致する逆方向 scope ケースも確認する。
8. 既存の build、CTest 3 target、WASI / Emscripten CI、MSVC CI を回す。

## ドキュメント

実装時に README と AGENTS.md を更新し、以下を明記する。

- indexed query は重複 ID キーで fast path を使わず、通常走査と同じ結果にする。
- `document_id_index::find()` のみ先頭ノードを返す first-wins 契約を維持する。
- `is_ambiguous(id)` / `covers_scope(scope)` の追加と、`rebuild()` による再構築が
  必要なケースを記載する。
- installed package は config package だけでなく raw header / library fallback を使える。
- raw static の linkage が名前から曖昧な場合は
  `LEXBORPP_RAW_LEXBOR_LINKAGE=STATIC` を利用する。

## 成功条件

- 対象 1–5 の各再現が修正される。
- 既存 API のシグネチャと重複 ID の first-wins 契約が変わらない。
- installed / add_subdirectory / raw dependency の consumer が構成・リンクできる。
- PR の既存 CMake / C++ / WASI / Emscripten 検証を回帰させない。
