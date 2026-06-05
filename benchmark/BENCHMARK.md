# lexborpp CSS Selector Benchmark

## 概要

`lexborpp` の CSS セレクタ検索において、3つの実装方式の処理時間を比較する。

| 方式 | 説明 |
|------|------|
| **NTTP** | `query_selector<"div#content">(root)` — コンパイル時にパース済み。専用マッチ関数がテンプレート再帰で生成される |
| **Runtime** | `query_selector(root, "div#content")` — 実行時にパース + AST 解釈 |
| **Naive** | `node_walker{root} \| tag<> \| id<>` + 手動ループ — lexborpp の range adapter を直接組み合わせる |

## テスト環境

- HTML サイズ: 942 bytes (ネスト付きセクション/記事/属性)
- 繰り返し回数: 10,000 回
- コンパイラ最適化: `-O3 -march=native`

## 実行方法

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench_css_selector
./build/bench_css_selector
```

## 結果: query_selector (最初の1件)

| セレクタ | NTTP (μs) | Runtime (μs) | Naive (μs) | NTTP/Naive |
|---------|-----------|-------------|-----------|------------|
| `#leaf-b` (deep) | 0.081 | 3.764 | 0.092 | 1.1x |
| `article.card` | 0.062 | 2.775 | 0.022 | 0.4x |
| `section#tree > article.card` | 0.071 | 3.114 | 0.026 | 0.4x |
| `section#tree article.card p` | 0.068 | 2.972 | 0.027 | 0.4x |
| `article#a + article#b` | 0.107 | 2.754 | 0.063 | 0.6x |
| `article#a ~ article` | 0.114 | 2.676 | 0.047 | 0.4x |
| `div[data-role=main]` | 0.057 | 2.736 | 0.042 | 0.7x |
| `p.match` | 0.254 | 2.965 | 0.225 | 0.9x |
| `p, span > b` | 0.112 | 2.780 | 0.016 | 0.1x |

## 結果: query_selector_all (全件)

| セレクタ | NTTP (μs) | Runtime (μs) | Naive (μs) | NTTP/Naive |
|---------|-----------|-------------|-----------|------------|
| `article.card` | 0.422 | 3.117 | 0.241 | 0.6x |
| `section#tree article.card p` | 0.243 | 3.124 | 0.198 | 0.8x |
| `p, span > b` | 0.274 | 2.979 | 0.464 | 1.7x |

## 総評

### Naive が高速なケース

小さな DOM では **Naive (range adapter) が NTTP より 2〜7倍高速**。

- `article.card`: Naive 0.022μs vs NTTP 0.062μs (Naive 2.8x)
- `p, span > b`: Naive 0.016μs vs NTTP 0.112μs (Naive 7.0x)

理由: range adapter (`tag<>`, `id<>`) は内部ループが非常に軽量。NTTP の再帰テンプレート呼び出しには関数呼び出しオーバーヘッドが乗る。DOM が小さければ、パース・マッチの静的最適化よりループの軽さが勝る。

### NTTP が高速なケース

深くネストされたセレクタや `query_selector_all` では **NTTP が Naive を上回る**。

- `p, span > b` (all): NTTP 0.274μs vs Naive 0.464μs (NTTP 1.7x)
- `#leaf-b` (deep): NTTP 0.081μs vs Naive 0.092μs (NTTP 1.1x)

理由: NTTP は右から左の再帰で効率的に探索範囲を絞り込む。Naive は手動ループで複雑な結合子を再実装する必要があり、コードが膨張しやすい。

### Runtime は常に ~3μs

Runtime 版は毎回パース + AST 解釈があるため、セレクタの複雑さに関わらず **一定のレイテンシ**。NTTP や Naive の 10〜80倍遅い。

### 選択指針

| ニーズ | 推奨方式 |
|--------|---------|
| 静的セレクタ + 大規模 DOM | **NTTP** — パースゼロ、コンパイル時最適化 |
| 静的セレクタ + 小規模 DOM | **Naive** — range adapter が最軽量 |
| 動的セレクタ | **Runtime** — 他の選択肢なし |
| 保守性 + 一貫したパフォーマンス | **NTTP** — Naive はセレクタごとの手書きが必要 |
