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
- コンパイラ最適化: `-O3 -march=native` (`./build.sh` = `LEXBORPP_NATIVE_ARCH=ON` + Release)
- 計測日: 2026-08-22 (gcc / x86_64 Linux)

## 実行方法

```bash
./build.sh          # LEXBORPP_NATIVE_ARCH=ON で configure & build
./build/bench_css_selector
```

## 結果: query_selector (最初の1件)

| セレクタ | NTTP (μs) | Runtime (μs) | Naive (μs) | NTTP/Naive |
|---------|-----------|-------------|-----------|------------|
| `#leaf-b` (via tree) | 0.070 | 9.025 | 0.107 | 1.5x |
| `article.card` | 0.058 | 8.384 | 0.014 | 0.2x |
| `section#tree > article.card` | 0.066 | 8.270 | 0.016 | 0.2x |
| `section#tree article.card p` | 0.050 | 8.343 | 0.018 | 0.4x |
| `article#a + article#b` (adj sibling) | 0.119 | 8.365 | 0.064 | 0.5x |
| `article#a ~ article` (follow sibling) | 0.101 | 8.337 | 0.048 | 0.5x |
| `div[data-role=main]` | 0.058 | 8.201 | 0.039 | 0.7x |
| `p.match` | 0.227 | 8.332 | 0.190 | 0.8x |
| `p, span > b` (group) | 0.103 | 8.188 | 0.015 | 0.1x |

## 結果: query_selector_all (全件)

| セレクタ | NTTP (μs) | Runtime (μs) | Naive (μs) | NTTP/Naive |
|---------|-----------|-------------|-----------|------------|
| `article.card` | 0.271 | 8.415 | 0.231 | 0.9x |
| `section#tree article.card p` | 0.218 | 9.138 | 0.179 | 0.8x |
| `p, span > b` (group) | 0.208 | 8.522 | 0.453 | 2.2x |

## 総評

### Naive が高速なケース

小さな DOM では **Naive (range adapter) が NTTP より 2〜7倍高速**。

- `article.card`: Naive 0.014μs vs NTTP 0.058μs (Naive 約4x)
- `p, span > b`: Naive 0.015μs vs NTTP 0.103μs (Naive 約7x)

理由: range adapter (`tag<>`, `id<>`) は内部ループが非常に軽量。NTTP の再帰テンプレート呼び出しには関数呼び出しオーバーヘッドが乗る。DOM が小さければ、パース・マッチの静的最適化よりループの軽さが勝る。

### NTTP が高速なケース

深くネストされたセレクタや `query_selector_all` では **NTTP が Naive を上回る**。

- `p, span > b` (all): NTTP 0.208μs vs Naive 0.453μs (NTTP 2.2x)
- `#leaf-b` (via tree): NTTP 0.070μs vs Naive 0.107μs (NTTP 1.5x)

理由: NTTP は右から左の再帰で効率的に探索範囲を絞り込む。Naive は手動ループで複雑な結合子を再実装する必要があり、コードが膨張しやすい。

### Runtime は常に ~8μs

Runtime 版は毎回パース + spec 解釈があるため、セレクタの複雑さに関わらず **一定のレイテンシ**。NTTP や Naive の数十〜数百倍遅い。

> **注意**: Runtime 版は query 1 回ごとにパース結果の spec (三重ネスト配列, Max=12 で約 130KB+) をスタックに構築する。この固定コストがレイテンシの支配要因である。

### 選択指針

| ニーズ | 推奨方式 |
|--------|---------|
| 静的セレクタ + 大規模 DOM | **NTTP** — パースゼロ、コンパイル時最適化 |
| 静的セレクタ + 小規模 DOM | **Naive** — range adapter が最軽量 |
| 動的セレクタ | **Runtime** — 他の選択肢なし |
| 保守性 + 一貫したパフォーマンス | **NTTP** — Naive はセレクタごとの手書きが必要 |
