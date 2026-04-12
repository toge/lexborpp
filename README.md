# lexborpp

`lexborpp` は、[Lexbor](https://github.com/lexbor/lexbor) の HTML / DOM API を C++ から扱いやすくするための軽量なヘッダオンリーラッパーです。

このリポジトリでは、Lexbor が持つ高速な HTML パーサと DOM ツリー操作をそのまま活かしつつ、`std::string_view`、`std::optional`、`std::ranges` を使って次のような処理を簡潔に書けます。

- ID や class によるノード検索
- 属性値や直下テキストの取得
- 子孫ノード・兄弟ノード・属性列挙の range 走査
- Lexbor の DOM ノードを C++ の ranges アダプタと組み合わせた処理

Lexbor 自体は C 製の高速・標準準拠な HTML パーサで、HTML / DOM / CSS / URL などをモジュール単位で提供しています。`lexborpp` はそのうち、このリポジトリで利用している HTML / DOM 操作を C++ 向けに薄く包むことを目的にしています。

## 特徴

- ヘッダオンリーライブラリ
- `Lexbor` の DOM ノードをそのまま扱える
- `get_element_by_id`、`get_first_element_by_class`、`get_attr_value` などの小さな補助関数を提供
- `node_walker`、`node_sibling_walker`、`attr_walker` が `std::ranges` の view として使える
- CMake から `INTERFACE` ライブラリとして利用できる

## 必要環境

- CMake 3.25 以上
- C++23 以上に対応したコンパイラ
  - `CMakeLists.txt` では C++26 を優先し、未対応なら C++23 を使います
- [Lexbor](https://github.com/lexbor/lexbor)

テストを実行する場合は、追加で以下が必要です。

- Catch2

このリポジトリの `vcpkg.json` には次の依存関係が含まれています。

- `lexbor`
- `catch2`

## 導入方法

### 1. このリポジトリを依存として取り込む

既存の CMake プロジェクトに `add_subdirectory` で取り込むのが一番簡単です。

```cmake
add_subdirectory(external/lexborpp)

target_link_libraries(your_target
  PRIVATE
    lexborpp::lexborpp
)
```

`lexborpp` 自体はヘッダオンリーですが、内部で `lexbor::lexbor` にリンクします。そのため、事前に Lexbor を CMake から見つけられる状態にしてください。

### 2. vcpkg を使って依存関係を解決する

このリポジトリには `vcpkg.json` があるため、vcpkg を使う場合は Lexbor と Catch2 をまとめて解決できます。

たとえば configure 時に vcpkg のツールチェーンを指定します。

```bash
cmake -B build \
  -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
```

リポジトリ付属の `build.sh` も、vcpkg ツールチェーンを使って `build/` を構成してからビルドする前提になっています。

### 3. インストールして `find_package` で使う

このプロジェクトは CMake package config をインストールできるようになっています。インストール後は、利用側で次のように参照できます。

```cmake
find_package(lexborpp CONFIG REQUIRED)

target_link_libraries(your_target
  PRIVATE
    lexborpp::lexborpp
)
```

## 最小の使用例

以下は、HTML を Lexbor でパースし、`lexborpp` の補助関数と walker を使って情報を取り出す例です。

```cpp
#include <cstdlib>
#include <iostream>
#include <ranges>

#include <lexbor/html/html.h>
#include "lexborpp.hpp"

int main() {
  using namespace lexborpp;

  char constexpr html[] = R"HTML(
<!doctype html>
<html>
  <body>
    <div id="content" class="entry" data-role="main">Hello<span>world</span></div>
  </body>
</html>
)HTML";

  auto* document = lxb_html_document_create();
  if (document == nullptr) {
    std::cerr << "failed to create document\n";
    return EXIT_FAILURE;
  }

  auto const cleanup = [&]() {
    lxb_html_document_destroy(document);
  };

  auto const status = lxb_html_document_parse(
      document,
      reinterpret_cast<lxb_char_t const*>(html),
      sizeof(html) - 1);
  if (status != LXB_STATUS_OK) {
    std::cerr << "failed to parse html\n";
    cleanup();
    return EXIT_FAILURE;
  }

  auto* root = lxb_dom_interface_node(document);
  auto* content = get_element_by_id(root, "content");
  if (content == nullptr) {
    std::cerr << "element not found\n";
    cleanup();
    return EXIT_FAILURE;
  }

  if (auto const role = get_attr_value(content, "data-role"); role.has_value()) {
    std::cout << "data-role: " << *role << '\n';
  }

  if (auto const text = get_all_children_text(content, "|"); text.has_value()) {
    std::cout << "direct text: " << *text << '\n';
  }

  for (auto* node : node_walker{content}
                     | std::views::filter([](lxb_dom_node_t* node) noexcept {
                         return !is_non_element_node(node);
                       })) {
    auto const id = get_attr_value(node, "id");
    std::cout << "tag=" << tag_name(lxb_dom_node_tag_id(node));
    if (id.has_value()) {
      std::cout << " id=" << *id;
    }
    std::cout << '\n';
  }

  cleanup();
  return EXIT_SUCCESS;
}
```

出力イメージ:

```text
data-role: main
direct text: Hello
tag=SPAN
```

`node_walker{content}` は開始ノード自身ではなく、その子孫ノードを深さ優先で走査します。上の例では `<div id="content">` の内側にある `<span>` が列挙されます。

## 利用できる主な補助機能

README では実用上よく使うものだけを挙げます。

- `get_element_by_id(node, id)`
  - 指定ノード以下から `id` が一致する最初の要素を探します
- `get_first_element_by_class(node, class_name)`
  - `class` 属性文字列が完全一致する最初の要素を返します
- `get_attr_value(node, attr_name)`
  - 属性値を `std::optional<std::string_view>` で返します
- `get_first_child_text(node)`
  - 直下の最初のテキストノードを返します
- `get_all_children_text(node[, sep])`
  - 直下のテキストノードを連結して返します
- `node_walker`
  - 子孫ノードを深さ優先で列挙します
- `node_sibling_walker`
  - 指定ノードから後続兄弟を順に列挙します
- `attr_walker`
  - 属性を range として列挙します
- `tag_name(tag_id)`
  - Lexbor のタグ ID を文字列化します

## テスト

このリポジトリでは Catch2 と CTest を使っています。

すでに `build/` が構成済みであれば、次でテストできます。

```bash
./test.sh
```

手動で configure / build / test する場合は次のようになります。

```bash
cmake -B build \
  -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
ctest --test-dir build -V
```

## 収録ファイル

- `include/lexborpp.hpp`
  - 公開ヘッダ。補助関数と range ベース walker を定義します
- `test/test_lexborpp.cpp`
  - 主要 API の使い方と期待動作を示すテスト群です
- `test/test_support.hpp`
  - Lexbor ドキュメント生成と破棄をまとめたテスト用補助です

## 注意点

- `get_first_element_by_class` は class 名の単語分割ではなく、`class` 属性文字列との完全一致で判定します
- `get_all_children_text` / `get_first_child_text` は直下のテキストノードのみを対象にし、孫要素以下のテキストは含みません
- walker は Lexbor の生ポインタをそのまま扱うため、元の `lxb_html_document_t` の寿命が切れた後に使ってはいけません

## ライセンス

MITライセンスで提供します。
詳細は `LICENSE` ファイルを参照してください。

