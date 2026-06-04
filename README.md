# lexborpp

`lexborpp` は、[Lexbor](https://github.com/lexbor/lexbor) の HTML / DOM / CSS Selectors API を C++ から扱いやすくするための軽量なヘッダオンリーラッパーです。

生の `lxb_dom_node_t*` をそのまま扱いながら、`std::expected`、`std::optional`、`std::ranges` を使って次のような処理を短く書けます。

- RAII で HTML ドキュメントを所有する
- ID / class / CSS selector でノードを探す
- 属性・テキスト・HTML 文字列を取り出す
- DOM をその場で編集する
- Lexbor のノード列を ranges / views と組み合わせて絞り込む

Lexbor 自体は C 製の高速な HTML パーサです。公式ドキュメントでも HTML パースと CSS Selectors モジュールが公開されており、`lexborpp` はその上に薄い C++ API を重ねています。

## 特徴

- ヘッダオンリーライブラリ
- `lexborpp::document_ptr` と `parse_html()` による RAII ベースのパース
- `query_selector()` / `query_selector_all()` による CSS selector 検索
- `query_selector<"...">()` / `query_selector_all<"...">()` による NTTP 事前解析 selector 検索
- `set_attr()` / `remove_attr()` / `set_text_content()` による DOM 編集
- `outer_html()` / `inner_html()` / `get_deep_text()` によるシリアライズ補助
- `node_walker` / `node_sibling_walker` / `node_prev_sibling_walker` / `attr_walker`
- `tag` / `id` / `clazz` / `attr` の range アダプタ
- CMake から `lexborpp::lexborpp` として利用可能

## 必要環境

- CMake 3.25 以上
- C++23 以上に対応したコンパイラ
  - `CMakeLists.txt` では C++26 を優先し、未対応なら C++23 を使います
- [Lexbor](https://github.com/lexbor/lexbor)

テストを実行する場合は追加で Catch2 が必要です。リポジトリの `vcpkg.json` には次の依存関係が入っています。

- `lexbor`
- `catch2`

## 導入方法

### 1. `add_subdirectory` で取り込む

```cmake
add_subdirectory(external/lexborpp)

target_link_libraries(your_target
  PRIVATE
    lexborpp::lexborpp
)
```

`lexborpp` はヘッダオンリーですが、内部では Lexbor にリンクします。`find_package(lexbor CONFIG REQUIRED)` できる環境、または Lexbor のヘッダ / ライブラリを CMake が見つけられる環境が必要です。

### 2. vcpkg manifest mode で configure / build する

```bash
cmake -B build \
  -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
```

### 3. インストールして `find_package` で使う

このプロジェクトは CMake package config をインストールできます。

```bash
cmake --install build --prefix /absolute/path/to/install
```

利用側では次のように参照できます。

```cmake
find_package(lexborpp CONFIG REQUIRED)

target_link_libraries(your_target
  PRIVATE
    lexborpp::lexborpp
)
```

## 使用例

### HTML をパースして CSS selector で取得する

```cpp
#include <cstdlib>
#include <iostream>

#include <lexborpp.hpp>

auto main() -> int {
  auto constexpr html = R"HTML(
<!doctype html>
<div id="root">
  <ul class="items">
    <li id="a" class="item">first</li>
    <li id="b" class="item featured">second <b>item</b></li>
  </ul>
</div>
)HTML";

  auto doc_expected = lexborpp::parse_html(html);
  if (!doc_expected.has_value()) {
    std::cerr << "failed to parse html\n";
    return EXIT_FAILURE;
  }

  auto* root = lexborpp::get_root(doc_expected.value());
  auto* featured = lexborpp::query_selector(root, "li.featured");
  if (featured == nullptr) {
    std::cerr << "element not found\n";
    return EXIT_FAILURE;
  }

  if (!lexborpp::set_attr(lexborpp::as_element(featured), "data-state", "ready")) {
    std::cerr << "failed to set attribute\n";
    return EXIT_FAILURE;
  }

  std::cout << "text: " << lexborpp::get_deep_text(featured) << '\n';
  std::cout << "html: " << lexborpp::outer_html(featured) << '\n';

  for (auto* node : lexborpp::query_selector_all(root, "li.item")) {
    if (auto const id = lexborpp::get_attr_value(node, "id"); id.has_value()) {
      std::cout << "id=" << *id << '\n';
    }
  }

  return EXIT_SUCCESS;
}
```

### NTTP で selector を事前解析して使う

```cpp
#include <lexborpp.hpp>

auto find_featured_item(lxb_dom_node_t* root) -> lxb_dom_node_t* {
  return lexborpp::query_selector<"div#content > p.target">(root);
}
```

この経路は selector 文字列をコンパイル時に解析し、実行時は Lexbor の DOM に対する照合だけを行います。現時点では、`type` / `*` / `#id` / `.class` / `[...]` / `=, ~=, |=, ^=, $=, *=` / `,` / child `>` / adjacent sibling `+` / following sibling `~` をサポートし、未対応の pseudo-class はコンパイルエラーになります。

出力イメージ:

```text
text: second item
html: <li id="b" class="item featured" data-state="ready">second <b>item</b></li>
id=a
id=b
```

### ranges と組み合わせてノードを絞り込む

`node_walker` は開始ノード自身を含め、その子孫を深さ優先で列挙します。

```cpp
#include <iostream>
#include <ranges>

#include <lexborpp.hpp>

auto dump_ready_items(lxb_dom_node_t* root) -> void {
  for (auto* node : lexborpp::node_walker{root}
                     | lexborpp::tag<LXB_TAG_LI>
                     | lexborpp::attr<"data-state", "ready">) {
    std::cout << lexborpp::get_deep_text(node) << '\n';
  }
}
```

## 主な API

### ドキュメント管理

| API | 説明 |
| --- | --- |
| `document_ptr` | `lxb_html_document_t*` を RAII で保持する `std::unique_ptr` エイリアス |
| `parse_html(html)` | `std::expected<document_ptr, lxb_status_t>` を返す |
| `get_root(doc)` | ドキュメントのルートノードを返す |

### 検索

| API | 説明 |
| --- | --- |
| `get_element_by_id(node, id)` | 開始ノードと子孫を走査して最初に一致した `id` を返す |
| `has_class(node, class_name)` | `class` 属性を空白区切りトークンとして判定する |
| `has_class(node, {"a", "b"})` | 指定したすべての class を持つか判定する |
| `get_first_element_by_class(node, class_name)` | `class` 属性文字列が**完全一致**する最初の要素を返す |
| `get_elements_by_class(node, class_name)` | 開始ノードを含め、class トークンが一致する全要素を返す |
| `query_selector(node, selector)` | CSS selector に一致する最初の要素を返す |
| `query_selector_all(node, selector)` | CSS selector に一致する全要素を返す |
| `query_selector<"...">(node)` | NTTP で渡した CSS selector に一致する最初の要素を返す |
| `query_selector_all<"...">(node)` | NTTP で渡した CSS selector に一致する全要素を返す |

### 属性・テキスト・シリアライズ

| API | 説明 |
| --- | --- |
| `get_attr_value(node, name)` | 属性値を `std::optional<std::string_view>` で返す |
| `get_first_child_text(node)` | 直下の最初のテキストノードを返す |
| `get_all_children_text(node[, sep])` | 直下のテキストノードだけを連結して返す |
| `get_deep_text(node[, sep])` | 子孫全体のテキストを返す |
| `to_string(node)` | テキストノードなら文字列を返す |
| `to_name_string(attr)` | 属性名を返す |
| `to_string(attr)` | 属性値を返す |
| `outer_html(node)` | ノード自身を含む HTML を返す |
| `inner_html(node)` | 子ノードだけをシリアライズする |
| `tag_name(tag_id)` | Lexbor のタグ ID を大文字の代表名（`DIV`, `TEXT`等）で返す |

### DOM 編集

| API | 説明 |
| --- | --- |
| `as_element(node)` | `lxb_dom_node_t const*` を `lxb_dom_element_t*` に変換する |
| `set_attr(element, name, value)` | 属性を追加 / 更新する |
| `remove_attr(element, name)` | 属性を削除する |
| `set_text_content(node, text)` | 既存の子ノードを消してテキストノードへ置き換える |

### Walker / range アダプタ

| API | 説明 |
| --- | --- |
| `node_walker{node}` | 開始ノードを含め、子孫ノードを深さ優先で列挙する |
| `node_sibling_walker{node}` | 指定ノード自身から後続兄弟を列挙する |
| `node_prev_sibling_walker{node}` | 指定ノードの直前兄弟から逆方向に列挙する |
| `attr_walker{node}` | 要素の属性を列挙する |
| `attr_walker{attr}` | 指定属性から後続属性を列挙する |
| `tag<LXB_TAG_...>` | 指定したタグ ID のいずれかに一致するノードへ絞り込む |
| `id<"...">` | `id` 属性の完全一致で絞り込む |
| `clazz<"...">` | 指定した `class` 属性文字列のいずれかへの完全一致で絞り込む |
| `attr<"name", "value">` | 属性名と属性値の完全一致で絞り込む |

## テスト

```bash
./build.sh
./test.sh
```

手動で configure / build / test する場合は次です。

```bash
cmake -B build \
  -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --parallel
ctest --test-dir build -V
```

特定のケースだけ確認したい場合は次も使えます。

```bash
./build/test/all_test "range adapters tag id clazz attr filter expected nodes"
```

## 注意点

- `has_class()` と `get_elements_by_class()` は `class="alpha beta"` をトークン分割して判定します
- `get_first_element_by_class()` と `clazz<"...">` は `class` 属性文字列の完全一致です
- `get_first_child_text()` / `get_all_children_text()` は直下のテキストノードだけを対象にします
- `get_deep_text()` は子孫全体のテキストを対象にします
- `query_selector()` は無効な selector や初期化失敗時に `nullptr` を返し、`query_selector_all()` は空配列を返します
- `query_selector()` / `query_selector_all()` と `query_selector<"...">()` / `query_selector_all<"...">()` は、いずれも**開始ノード自身を探索対象に含みます**（root 参加挙動を統一しています）
- `query_selector<"...">()` / `query_selector_all<"...">()` はコンパイル時解析済みの selector を使います。未対応構文はコンパイルエラーになります
- walker / range アダプタは Lexbor の生ポインタをそのまま返すため、元の `lxb_html_document_t` の寿命内でだけ使ってください

## 収録ファイル

- `include/lexborpp.hpp`
  - 公開 API の umbrella ヘッダです。実体は `include/lexborpp/` 配下の関心事ごとに分かれた 8 ヘッダに分かれており、本ヘッダがそれを再 `#include` します。利用者側はこれまで通り `#include <lexborpp.hpp>` だけで全 API が使えます。
- `include/lexborpp/`
  - 内部実装の分割ヘッダ群 (core / tag_name / serialize_runtime / nttp_parser / nttp_match / nttp_range_adapters / nttp_query / ranges_specializations)。直接 `#include` することは想定していません。
- `cmake/lexborppConfig.cmake.in`
  - `find_package(lexborpp CONFIG REQUIRED)` 用の package config テンプレートです
- `build.sh`
  - vcpkg ツールチェーン前提で configure / build します
- `test/test_lexborpp.cpp`
  - 公開 API の期待動作を網羅する Catch2 テストです

## ライセンス

MIT ライセンスで提供します。詳細は `LICENSE` を参照してください。
