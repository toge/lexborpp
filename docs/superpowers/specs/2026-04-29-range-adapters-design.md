# Issue #4: C++20 Ranges アダプタ最小仕上げ設計

## 背景

`lexborpp` には `node_walker` などの range 対応 walker があり、`tag` / `id` / `clazz` / `attr` もすでに単一公開ヘッダ `include/lexborpp.hpp` 内に実装されている。

一方で issue #4 の要求と比較すると、現状は次の点だけが未整理である。

- `fixed_string` が利用する `std::copy_n` の直接依存が明示されていない
- range アダプタの predicate に `noexcept` と null 安全性の意図が明示されていない
- ユーザーは単一ヘッダ維持を希望しており、`include/lexborpp/range.hpp` への分離は今回の対象外

## 目的

既存の `tag` / `id` / `clazz` / `attr` API とテスト資産を活かしたまま、issue #4 を最小変更で完了できる状態に整える。

## 採用方針

### 1. 公開 API の配置

- `include/lexborpp.hpp` を唯一の公開ヘッダとして維持する
- range アダプタの公開名と記法は変更しない
  - `lexborpp::tag<LXB_TAG_...>`
  - `lexborpp::id<"...">`
  - `lexborpp::clazz<"...">`
  - `lexborpp::attr<"...", "...">`

### 2. 実装変更の範囲

- `fixed_string` 用に `<algorithm>` を直接インクルードする
- `tag` / `id` / `clazz` / `attr` の predicate を `noexcept` にする
- `tag` は `nullptr` を渡されても `false` になるようにする
- `id` / `clazz` / `attr` は既存の `get_attr_value` を継続利用し、比較ルールは変更しない

### 3. 意図的に変えない点

- `clazz` は class トークン一致ではなく、issue 本文どおり class 属性文字列の完全一致とする
- `attr` は属性名と属性値の完全一致とする
- range アダプタの定義位置を別ヘッダへ分離しない
- 既存の walker 実装や他 API には触れない

## テスト方針

- 既存の `test/test_lexborpp.cpp` にある range adapter テストを維持する
- 実装変更で新たな回帰点が生じた場合のみ、最小のテストを追加する
- 既存のビルド手順とテスト手順をそのまま使う

## 想定される成果

- `lexborpp::id<"name">` を含む既存記法がそのまま利用できる
- 依存関係が明示され、コンパイラ依存の偶然に寄らずにビルドしやすくなる
- 挙動変更なしで issue #4 の要求を満たす

## 実装時の注意

- Doxygen コメントの日本語スタイルは既存実装に合わせる
- 変更は range アダプタ周辺に限定し、無関係な整理は行わない
