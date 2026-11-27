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

`document_id_index` の map entry に `first` と `ambiguous` を保持する。既存の
`find(id)`、`contains(id)`、`size()` の意味は変えず、重複検出だけを
`is_ambiguous(id)` で公開する。

runtime / NTTP の indexed query は、右端 compound に ID があり、かつ
`!index.is_ambiguous(id)` の場合だけ単一ノード fast path を使う。それ以外は
通常の DOM 走査を呼ぶ。したがって重複 ID、スコープ外候補、追加 simple selector を
含む検索でも通常検索と同じ first/all 結果になる。インデックス作成前後の DOM 編集に
ついては従来どおり `rebuild()` が必要である。

### 2. インストール用の安定した Lexbor wrapper target を持つ

lexborpp 自身が所有する `lexborpp_lexbor` INTERFACE target を作り、
export 名を `lexborpp::lexbor` とする。`lexborpp::lexborpp` は
ビルド時に見つかった外部 Lexbor target 名ではなく、この wrapper だけに依存する。

build tree では wrapper を解決済みの
`lexbor::lexbor_static`、`lexbor::lexbor`、raw library の順に配線する。install
consumer では package config が利用者側の依存関係を解決した後に
`lexborpp::lexbor` の `INTERFACE_*` properties を再設定する。export file に
ビルド環境固有の Lexbor target 名が残っていても、consumer での生成前に安定 target
へ差し替えられる。

### 3. raw dependency fallback を package config から到達可能にする

package config では `find_package(lexbor CONFIG QUIET)` を使い、target が見つからない
場合に `find_path()` / `find_library()` へ進む。`find_dependency()` の失敗で
package config が即終了する構成は使わない。どちらの経路でも解決できなければ
`lexborpp_FOUND=FALSE` と理由を設定して return する。

raw lookup は `lexbor_static` を先に探し、見つからない場合だけ `lexbor` を探す。
これにより選択したライブラリの linkage を判定できる。header / library のみの
top-level build と installed package consumer の双方で同じ規則を使う。

### 4. raw 静的 Lexbor に `LEXBOR_STATIC` を伝搬する

`lexbor_static` を選択した経路では wrapper の
`INTERFACE_COMPILE_DEFINITIONS` に `LEXBOR_STATIC` を設定する。consumer 側から
raw target を再配線する場合も同じ定義を設定し、MSVC で
`LXB_API == __declspec(dllimport)` になることを防ぐ。raw 共有ライブラリ経路では
この定義を付けない。

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
4. raw static compile command に `LEXBOR_STATIC` が含まれることを確認する。
5. `./build.sh static` 後の `CMakeCache.txt` が
   `VCPKG_TARGET_TRIPLET=x64-linux-static` になり、呼び出し側の `VCPKG_ROOT`
   override が維持されることを確認する。
6. 既存の build、CTest 3 target、WASI / Emscripten CI、MSVC CI を回す。

## 成功条件

- 対象 1–5 の各再現が修正される。
- 既存 API のシグネチャと重複 ID の first-wins 契約が変わらない。
- installed / add_subdirectory / raw dependency の consumer が構成・リンクできる。
- PR の既存 CMake / C++ / WASI / Emscripten 検証を回帰させない。
