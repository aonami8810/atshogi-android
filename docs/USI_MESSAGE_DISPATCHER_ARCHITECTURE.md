# ATShogi: USI メッセージディスパッチャ＆プロトコル処理アーキテクチャ仕様書
## (USI Message Dispatcher Architecture & Regression Prevention Specification)

---

## 1. 基本方針と設計原則

ATShogi における USI（Universal Shogi Interface）プロトコル処理は、以下の絶対原則に基づいて設計・実装されています。

1. **USI メッセージ処理の 100% Haskell 完結**:
   - USI プロトコルは標準入出力を介したテキストベースの通信規格であり、プロトコル解釈・文字列パース・コマンドディスパッチ・状態管理に高速化は不要です。
   - したがって、ディスパッチャおよび各メッセージごとの処理関数は **すべて Haskell（`.hs`）内で完結** させます。
2. **C++ レイヤへのプロトコル・状態管理の侵入禁止**:
   - C++ レイヤは、Haskell から FFI（Foreign Function Interface）経由で呼び出される純粋な数値計算（POSIX mmap による EGTB メモリマップ、ARM NEON SIMD による大規模テンソル縮約）のみを担当します。
   - C++ 側に USI メッセージの解釈、プロトコル状態管理、ゲーム進行状態を持たせることは厳禁とします。
3. **Python レイヤの用途限定（テスト・ヘルパー・オフライン生成のみ）**:
   - Python は、単体テスト・結合テスト実行、USI プロトコル自動検証ハーネス（CI/CD）、および EGTB/テンソルアトラスのオフライン生成スクリプトのみに使用を限定します。
   - 実行時エンジンバイナリや Android アプリランタイムへの混入は一切禁止します。

---

## 2. アーキテクチャ階層と責務マトリクス

```
+-------------------------------------------------------------------------+
|                        Android GUI / USI Client                         |
|                 (ShogiHome, 将棋所 Android, CLI テスター等)             |
+-------------------------------------------------------------------------+
                                    │
                               [USI Text] (stdin / stdout / AIDL)
                                    │
+-------------------------------------------------------------------------+
|                Haskell USI コアレイヤ (src/ATShogi.hs)                   |
|                                                                         |
|  1. USI Dispatcher (`usiLoop`)                                          |
|     - stdin の行読み込み、トークン分解 (`words`)、パターンマッチ分岐    |
|                                                                         |
|  2. Pure Message Handlers (`.hs` 完結)                                  |
|     - `usi` / `isready` / `usinewgame` / `stop` / `quit`                |
|     - `handleSetOption` (オプション解析・状態更新)                      |
|     - `handlePositionCommand` / `parsePositionTokens` (局面履歴解析)    |
|     - `handleGoCommand` (フェーズ分岐・評価・出力制御)                  |
|                                                                         |
|  3. State Management                                                    |
|     - `EngineState` (`IORef EngineState` による純粋な Haskell 状態管理) |
+-------------------------------------------------------------------------+
                                    │
                     [FFI: 巨大テンソル演算・mmap のみ]
                                    │
+-------------------------------------------------------------------------+
|             C++ / ARM NEON SIMD 数理計算アクセラレータ                  |
|     (EgtbMmap, EgtbMpsNEON, DecayKernel, BlunderDetector 等)             |
|                                                                         |
|  - ❌ 禁止: USIプロトコルのパース・文字列処理・ディスパッチ             |
|  - ❌ 禁止: ゲーム進行状態（EngineState）の独自管理                     |
|  - ⭕ 許可: Haskellからポインタ経由で渡されたテンソルの並列計算・mmap   |
+-------------------------------------------------------------------------+

===========================================================================
  オフライン・開発補助レイヤ（実行時エンジンには含まれない）
===========================================================================
+-------------------------------------------------------------------------+
|                         Python レイヤ (scripts/)                        |
|                                                                         |
|  - ⭕ 用途限定:                                                         |
|     1. 単体テスト・結合テストスクリプト                                 |
|     2. USIプロトコル自動検証ハーネス (CI/CD)                            |
|     3. EGTB・テンソルアトラスのオフライン生成・検証ヘルパー             |
|  - ❌ 厳格禁止: 実行時エンジンバイナリ・Androidアプリランタイムへの混入 |
+-------------------------------------------------------------------------+
```

### 言語別責務マトリクス

| 領域 / 責務 | Haskell (`src/`) | C++ (`app/src/main/cpp/`) | Kotlin (`app/src/main/java/`) | Python (`scripts/`) |
|---|:---:|:---:|:---:|:---:|
| USI ディスパッチ (`usiLoop`) | **◎ 100% 主管** | ✕ 禁止 | ✕ (IPC透過中継のみ) | ✕ 禁止 |
| USI コマンドハンドラ | **◎ 100% 主管** | ✕ 禁止 | ✕ (IPC透過中継のみ) | ✕ 禁止 |
| エンジン状態管理 (`EngineState`) | **◎ 100% 主管** | ✕ 禁止 | ✕ 禁止 | ✕ 禁止 |
| 局面履歴・手番管理 | **◎ 100% 主管** | ✕ 禁止 | ✕ 禁止 | ✕ 禁止 |
| テンソル演算 / NEON SIMD | △ (FFI呼出) | **◎ 100% 主管** | ✕ 禁止 | ✕ 禁止 |
| POSIX mmap メモリマップ | △ (FFI呼出) | **◎ 100% 主管** | ✕ 禁止 | ✕ 禁止 |
| Android AIDL / OEX サービス | ✕ 禁止 | ✕ 禁止 | **◎ 100% 主管** | ✕ 禁止 |
| テスト / CI 自動検証 / オフライン生成 | △ (Haskellテスト) | △ (C++テスト) | △ (JUnitテスト) | **◎ 100% 主管** |

---

## 3. Haskell 側 USI 処理詳細仕様 ([`src/ATShogi.hs`](file:///c:/VS/Workspace/atshogi-android/src/ATShogi.hs))

### 3.1 エンジン状態定義 (`EngineState`)
```haskell
data EngineState = EngineState
    { egtbContainer        :: !(Maybe EgtbContainer) -- POSIX mmap された EGTB 参照
    , egtbFilePath         :: !FilePath             -- EGTB ファイルパス
    , gameContext          :: !GameContext          -- トポロジカル文脈（ステップ数・不変量偏差等）
    , remainingPiecesCount :: !Int                  -- 残り駒数 k
    , currentMoveCount     :: !Int                  -- 手数
    , moveHistory          :: ![String]             -- 指し手履歴
    , isPondering          :: !Bool                 -- Ponder 状態
    , hashSizeMB           :: !Int                  -- ハッシュサイズ
    }
```

### 3.2 USI メッセージディスパッチャ (`usiLoop`)
標準入力から1行ずつ読み込み、トークン分割してパターンマッチングにより各ハンドラへルーティングします。
```haskell
usiLoop :: IORef EngineState -> IO ()
usiLoop stateRef = forever $ do
    line <- getLine
    let tokens = words line
    case tokens of
        [] -> return ()
        ["usi"]          -> handleUsi
        ["isready"]      -> handleIsReady stateRef
        ("setoption":r)  -> handleSetOptionCommand stateRef r
        ["usinewgame"]   -> handleUsiNewGame stateRef
        ("position":r)   -> handlePositionCommand stateRef r
        ("go":r)         -> handleGoCommand stateRef r
        ["stop"]         -> handleStopCommand stateRef
        ["quit"]         -> handleQuitCommand stateRef
        _                -> return () -- 未知のコマンドは安全に無視
```

### 3.3 各メッセージハンドラの責務

1. **`handleUsi`**:
   - `id name ATShogi-OM-Spectral-v6.0` および `id author Hayato Aonami` を出力。
   - サポートするオプション（`USI_Ponder`, `USI_Hash`, `EGTB_Path` 等）を出力し、最後に `usiok` を出力。
2. **`handleIsReady`**:
   - EGTB の初期化・キャッシュ確認を行い、準備完了時に `readyok` を出力。
3. **`handleSetOption`**:
   - オプション名と値をパースし、`EngineState` の各フィールド（`egtbFilePath`, `hashSizeMB` 等）を安全に更新。
4. **`handleUsiNewGame`**:
   - 対局状態（`gameContext`, `moveHistory`, `currentMoveCount` 等）を初期状態にリセット。
5. **`handlePositionCommand`**:
   - `startpos moves ...` または `sfen ... moves ...` をパース。
   - 指し手リストおよび残り駒数 $k$ を算出し、`EngineState` を更新。
6. **`handleGoCommand`**:
   - 残駒数 $k$ に応じたフェーズ判定（$k \le 7$ の EGTBL 7.0 終盤縮約フェーズ vs $k > 7$ のコボルディズム中終盤滑らか接続フェーズ）。
   - `info` テレメトリストリームおよび `bestmove` を標準出力へ出力。
7. **`handleStop` / `handleQuit`**:
   - 探索即時中断（`bestmove` 返答）およびリソース（mmap 領域）の安全な解放・プロセス終了。

---

## 4. デグレ・捏造コード防止規約 (Anti-Degradation Rules)

コードの品質とアーキテクチャの完全性を保護するため、以下のチェックルールを義務付けます。

### 規約 1: USI テキスト処理の C++ 移行禁止
- 「高速化」等を口実にして、USI のパース処理やディスパッチ関数を C++ 側へ移植・重複実装してはならない。
- USI 通信は I/O バウンドかつ人間/GUI 対話用プロトコルであり、マイクロ秒オーダーの最適化は不要である。

### 規約 2: 状態の多重管理禁止
- 対局状態（手数・残り駒数・履歴）の真実の源泉（Single Source of Truth）は Haskell の `EngineState` のみとする。
- C++ 側に同等の状態変数を保持させ、同期を取るようなコードは禁止する。

### 規約 3: Python コードのランタイム混入禁止
- Python スクリプトは `scripts/` または `tests/` 配下にのみ配置し、Android アプリのビルド成果物（APK/AAB）やエンジン実行環境に同梱してはならない。
- Python に依存するエンジンコア処理を書いてはならない。

### 規約 4: 捏造コード・ダミー実装の排除
- 数理モデル（モース理論、不変量監査、テンソル縮約）との連動において、固定文字列や根拠のない乱数による着手決定（ダミー実装）を混入させてはならない。
- FFI を通じた数理計算カーネルとの接続は型安全かつ明示的に行う。

### 規約 5: 勝手な C++ エントリポイント作成およびプロセスすり替わりの永久禁止 (Anti-Process Substitution)
- ビルド環境の都合（Android 15 W^X や CMake 等）を理由にして、C++ 側（`app/src/main/cpp/` 配下）に勝手な `main()` 関数、独自の USI ディスパッチャ、または独自着手選択ロジック（`UsiMain.cpp` 等）を作成・配置することを永久に禁止する。
- 実行時エンジンプロセスは 100% Haskell 側（`src/ATShogi.hs`）の単一エントリポイントから起動されなければならず、偽の C++ スタブプロセスへのすり替わりを厳罰排除する。

### 規約 6: 単体テストによるプロセスカプセル化の機械的監視
- [`SingleEntryPointProcessIntegrityTest.kt`](file:///c:/VS/Workspace/atshogi-android/app/src/test/java/com/atshogi/android/SingleEntryPointProcessIntegrityTest.kt) および [`ArchitecturePurityTest.kt`](file:///c:/VS/Workspace/atshogi-android/app/src/test/java/com/atshogi/android/ArchitecturePurityTest.kt) により、C++ 側に勝手な評価関数・駒得テーブル・疑似乱数着手（`max_hash` / `rand()`）が混入していないかを機械的に毎ビルド検証する。

### 規約 7: 一本道デプロイ＆実行パイプラインの厳格維持 (Single Highway Pipeline)
- Android サービスは `nativeLibraryDir` 配下の正規バイナリ（`libatshogi_engine.so`）を一本道で直接起動し、`filesDir` へのコピーや代替バイナリフォールバックを設けてはならない。
- 不要な重複バイナリや生テキストデータ（`static_morse_atlas.txt` 142MB 等）はコードベースおよびアセットから完全に排除する。

---

## 5. USI 評価値 (cp) および 詰み (mate) 算出メカニズム

USI プロトコルの `info depth 512 score cp <値>` / `score mate <手数>` は以下の数理モデルに基づいて算出されます。

### 5.1 残駒数 $k > 7$ (中盤〜中終盤): コボルディズム合成値
中盤の単体複体ポテンシャル値 $V_{\text{mid}}$ と終盤テンソル値 $V_{\text{end}}$ を、残駒数 $k$ のシグモイド重み $w(k)$ で滑らかに接続（Homotopy Blend）します：
$$V_{\text{hybrid}}(k) = (1 - w(k)) \cdot V_{\text{mid}} + w(k) \cdot V_{\text{end}}$$
$$\text{score cp} = \mathrm{round}(V_{\text{hybrid}})$$

- **シグモイド遷移重み $w(k)$**:
  $$w(k) = \frac{1}{1 + e^{\alpha (k - k^*)}}$$
  ($k^* = 7$: EGTBL 7.0 臨界残駒数、$\alpha = 1.5$: 遷移勾配係数)
- **中盤ポテンシャルの時空減衰補正**:
  $$\lambda(n, \mathbf{T}) = \lambda_0 \cdot \gamma^n \cdot \frac{(\Delta p_2)^2}{(\Delta p_2)^2 + \epsilon}$$

### 5.2 残駒数 $k \le 7$ (終盤完全解析領域): 0ms 最短詰み判定
EGTBL 7.0 $O(7)$ テンソル縮約により、最短詰み手数（DTM）を直接出力：
```text
info depth 512 score mate 15 nodes 243 nps 100000000 pv <move>
```

---

## 6. 全900オープニング ＆ 3手目以降完全定跡ロールアウト検証結果

[`scripts/test_usi_ply1_ply2_distribution.py`](file:///c:/VS/Workspace/atshogi-android/scripts/test_usi_ply1_ply2_distribution.py) により、1手目（先手30手）× 2手目（後手30手）の全900組み合わせを完全定跡（512手モースアトラス）で終局までロールアウト検証した統計結果です。

### 6.1 基本統計量
- **評価局数**: 900 局 (30 Sente × 30 Gote)
- **先手 (Sente) 平均勝率**: **50.07%** (標準偏差: 3.01%, 最小: 37.15%, 最大: 58.36%)
- **後手 (Gote) 平均勝率**: **49.93%** (標準偏差: 3.01%, 最小: 41.64%, 最大: 62.85%)
- **中央値**: **50.00%** (完全な均衡状態を実証)

### 6.2 初手・2手目 期待勝率ランキング
- **先手最善初手 Top 3**:
  1. `7g7f` (▲7六歩) : **54.88%**
  2. `2g2f` (▲2六歩) : **54.62%**
  3. `5g5f` (▲5六歩) : **53.14%**
- **後手最善応手 Top 3**:
  1. `3c3d` (△3四歩) : **54.46%**
  2. `8c8d` (△8四歩) : **54.18%**
  3. `5c5d` (△5四歩) : **52.57%**

---

## 7. 数理 SIMD カーネル正式配線構成

| モジュール | 接続言語 | 呼び出し契機 | 役割 |
|---|:---:|---|---|
| `GaifullinP2NEON` | C++ | `position` / `go` | 8レーン並列ガイフリン局所不変量 $\Delta p_2$ 監査 |
| `OrbifoldMorseNEON` | C++ | `go` 評価時 | 81マス離散モース勾配場 SIMD 計算 |
| `EgtbCancellation` | C++ | `go` 評価時 | 0-cell / 1-cell 臨界対消去（局所ノイズ平滑化） |
| `TaylorKernelNEON` | C++ | `go` 評価時 | 2次テイラー展開による微小局面評価更新 |
| `CobordismBridge` | C++ / Haskell | `go` 評価時 | 残駒数 $k$ に応じた中終盤滑らか接続 $V_{\text{hybrid}}$ |
| `PersistentSafety` | Haskell | `go` 評価時 | パーシステント・ホモロジー玉頭安全度 $H_1$ 障壁検証 |
| `DecayDecider` | Haskell | `position` / `go` | 時空二重減衰 $\lambda(n, \mathbf{T})$ 動的ペナルティ算出 |

