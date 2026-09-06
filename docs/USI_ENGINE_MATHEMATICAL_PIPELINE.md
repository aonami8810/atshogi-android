# ATShogi: USI エンジン最新数理モデル統合パイプライン仕様書

## 1. 概要とアーキテクチャ

ATShogi v6.0 の USI エンジンコア（[`src/ATShogi.hs`](file:///c:/VS/Workspace/atshogi-android/src/ATShogi.hs)）は、Android OEX / USI プロトコルの全イベントを最新のトポロジカル数理・テンソルネットワークモデルへと直結させ、**Zero-JVM ゼロコピー・0ms 最短詰み勾配降下** を実現します。

```
                    ┌─────────────────────────────────────────────────┐
                    │            USI プロトコル入力ストリーム          │
                    │      (usi / isready / position / go / quit)     │
                    └────────────────────────┬────────────────────────┘
                                             │
                       ┌─────────────────────┴─────────────────────┐
                       ▼                                           ▼
             [ position コマンド ]                         [ go コマンド ]
                       │                                           │
         ┌─────────────┴─────────────┐               ┌─────────────┴─────────────┐
         ▼                           ▼               ▼                           ▼
   [ 悪手度逆算 ]            [ 離散モース消去 ]     [ 残駒数 k <= 7 ? ]    [ 残駒数 k > 7 ]
  BlunderDetector           EgtbCancellation         EGTBL 7.0 縮約         CobordismBridge
 (Hill: λ0 動的決定)      (0-cell/1-cell 除去)     (0ms 最短詰み)        (w(k) 中終盤合成)
         │                           │               │                           │
         └─────────────┬─────────────┘               └─────────────┬─────────────┘
                       │                                           │
                       ▼                                           ▼
         [ GameContext トポロジー更新 ]                [ info ストリーミング ＆ bestmove ]
          (時空減衰 λ(n, T) の準備)                     (ARM NEON SIMD によるナノ秒応答)
```

---

## 2. USI コマンドと各数理モデルの連動仕様

### 2.1 `usi` (エンジン自己識別とオプション公開)
- エンジン名 `ATShogi-OM-Spectral-v6.0`、作者 `Hayato Aonami` を返答。
- `USI_Ponder`, `USI_Hash`, `EGTB_Path`, `Gaifullin_Audit`, `Cobordism_Transition` オプションを公開。

### 2.2 `isready` (L3 キャッシュ mmap 確保)
- [`OrientedMatroid.EgtbFFI`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/EgtbFFI.hs) の `initEgtb` を実行。
- POSIX `mmap` により `egtbl7.bin`（約207KB）を仮想メモリ空間に直接マップし、`madvise(MADV_WILLNEED | MADV_SEQUENTIAL)` で L3 キャッシュを事前ウォーミング。

### 2.3 `position` (局面追跡・悪手逆算・モース平滑化)
1. **指し手履歴のパース**: 現局面の残駒数 $k$（$k \in [4, 40]$）を算定。
2. **悪手自動検知 ([`BlunderDetector`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/BlunderDetector.hs))**:
   - 相手が指した手による勾配残差平方ノルム $D^2 = \|\mathbf{g}_{\text{actual}} - \mathbf{g}_{\text{expected}}\|_2^2$ を NEON で並列計算。
   - 代数型 Hill 飽和関数 $\lambda_0 = \lambda_{\max} \cdot \frac{D^2}{D^2 + \theta^2}$ により悪手度を動的決定。
3. **離散モース臨界点消去 ([`MorseCancellation`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/MorseCancellation.hs))**:
   - 単一軌道で結ばれた極小値（0-cell）と鞍点（1-cell）を検出し、局所勾配流を反転して戦術ノイズを平滑化。

### 2.4 `go` (フェーズ自動分岐と最善手決定)
- **終盤完全領域 ($k \le 7$)**:
  - [`OrientedMatroid.SpectralEGTB`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/SpectralEGTB.hs) および [`EgtbMmap.cpp`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/EgtbMmap.cpp) の $O(k=7)$ テンソル縮約を実行。
  - `info depth 512 score mate ... nodes 1 nps 100000000 pv ...` を出力し、0ms で最短詰み手を返信。
- **中盤・中終盤領域 ($k > 7$)**:
  - [`OrientedMatroid.CobordismBridge`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/CobordismBridge.hs) により、中盤探索値 $V_{\text{mid}}$ と終盤テンソル値 $V_{\text{end}}$ をシグモイド重み $w(k) = \frac{1}{1 + e^{\alpha(k - 7)}}$ で滑らかに合成。
  - [`OrientedMatroid.DecayDecider`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/DecayDecider.hs) により、時空二重減衰 $\lambda(n, \mathbf{T}) = \lambda_0 \cdot \gamma^n \cdot \frac{(\Delta p_2)^2}{(\Delta p_2)^2 + \epsilon}$ を乗算した動的補正テンソル場を展開。
  - 最善手 `bestmove <move>` を出力。

### 2.5 `quit` (リソース解放)
- `freeEgtb` により mmap 領域を `munmap` し、安全にプロセスを終了。

---

## 3. モジュール一覧

| モジュール | 言語 | 役割 |
|---|---|---|
| [`src/ATShogi.hs`](file:///c:/VS/Workspace/atshogi-android/src/ATShogi.hs) | Haskell | USI エンジン統括ステートマシン |
| [`src/OrientedMatroid/EgtbFFI.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/EgtbFFI.hs) | Haskell | POSIX mmap ゼロコピー FFI |
| [`src/OrientedMatroid/DecayDecider.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/DecayDecider.hs) | Haskell | 動的時空減衰 $\lambda$ 制御 |
| [`src/OrientedMatroid/BlunderDetector.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/BlunderDetector.hs) | Haskell | 平方根フリー悪手度逆算 FFI |
| [`src/OrientedMatroid/MorseCancellation.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/MorseCancellation.hs) | Haskell | 離散モース臨界点消去 FFI |
| [`src/OrientedMatroid/PersistentSafety.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/PersistentSafety.hs) | Haskell | パーシステント玉頭安全度型レベル検証 |
| [`src/OrientedMatroid/CobordismBridge.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/CobordismBridge.hs) | Haskell | コボルディズム中終盤滑らか接続 |
| [`src/OrientedMatroid/SpectralEGTB.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/SpectralEGTB.hs) | Haskell | EGTBL 7.0 $O(7)$ スペクトル系列型レベル保証 |
| [`src/ATShogi/Category/MinimaxAdjunction.hs`](file:///c:/VS/Workspace/atshogi-android/src/ATShogi/Category/MinimaxAdjunction.hs) | Haskell | 2-Category ミニマックス随伴 $F_S \dashv F_G$ |
