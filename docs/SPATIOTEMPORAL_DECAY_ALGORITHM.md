# ATShogi: オンライン補間 $\lambda$ の動的時空減衰アルゴリズム設計書

## 1. 動的 $\lambda$ の数理モデル：時空二重減衰 (Spatiotemporal Double Decay)

ATShogi では、オフライン完全定跡（安定多様体 $\mathcal{T}_{\text{static}}$）と、オンラインでのとがめ定跡（不安定多様体からの収縮流 $\Delta \mathcal{T}$）を単一の連続テンソル場として一元化します：

$$\mathcal{T}_{\text{total}} = \mathcal{T}_{\text{static}} + \lambda(n, \mathbf{T}) \cdot \Delta \mathcal{T}$$

補間係数 $\lambda(n, \mathbf{T})$ は、相手が疑問手を指した瞬間に最大化され、**時間（経過手数）**と**空間（トポロジーの回復度）**の二重の軸からリアルタイムかつ動的に減衰（Decay）し、探索を滑らかに定跡の幹（Trunk）へと引き戻します。

$$\lambda(n, \mathbf{T}) = \lambda_0 \cdot \lambda_T(n) \cdot \lambda_S(\mathbf{T})$$

ここで：
- $\lambda_0 \in [0, 1]$: 初期とがめ強度（相手の悪手の深刻度）

---

### ① 時間的幾何減衰 (Temporal Geometric Decay)
定跡から外れてからの経過手数 $n$（相手が外した手数を $n=0$ とする）に応じて、幾何級数的に影響度を減衰させます：

$$\lambda_T(n) = \gamma^n \quad (\gamma \in [0.70, 0.95], \text{推奨値: } \gamma = 0.85)$$

- 相手が定跡を外れた直後: $\lambda_T(0) = 1.0$
- 3〜5手進むと急速に影響力が弱まり、安定多様体へ収束。

### ② 空間的・トポロジカル収縮 (Spatial/Topological Squeeze)
現在の局面テンソル $\mathbf{T}$ と、完全定跡の幹のポテンシャル場とのコホモロジー的ねじれを計測します。
Gaifullin 局所不変量の残差 $\Delta p_2(\mathbf{T})$（ポントリャーギン類監査）を用い、元の安定多様体に近づく（$\Delta p_2 \to 0$）ほど、強制的に $\lambda$ を 0 に押し潰します：

$$\lambda_S(\mathbf{T}) = \frac{(\Delta p_2(\mathbf{T}))^2}{(\Delta p_2(\mathbf{T}))^2 + \epsilon}$$

- **$\Delta p_2$ が大きいとき（混乱・乱戦状態）**: $\lambda_S \approx 1$（とがめテンソルの影響力を最大発揮）
- **$\Delta p_2 \to 0$（とがめ成功・調和状態復帰）**: $\lambda_S \to 0$（動的補正を消去し、静的ベース定跡に完全復帰）
- $\epsilon = 10^{-4}$: 特異点回避の正則化パラメータ

---

## 2. C++ (ARM NEON) 高速代数マージカーネル

超越関数（`pow`, `exp`）を排除し、積和演算（FMA: `vmlaq_f32`）のみで構成される超高速 NEON SIMD カーネルです。

```cpp
void merge_tensors_neon(
    const float* __restrict static_ptr,
    const float* __restrict delta_ptr,
    float* __restrict out_ptr,
    float lambda_0,
    float gamma,
    int n,
    float delta_p2,
    float epsilon,
    int size
) {
    // 1. 時間的幾何減衰: gamma^n
    float lambda_T = 1.0f;
    for (int i = 0; i < n; ++i) lambda_T *= gamma;

    // 2. 空間的収縮: dp2^2 / (dp2^2 + eps)
    float dp2_sq = delta_p2 * delta_p2;
    float lambda_S = dp2_sq / (dp2_sq + epsilon);

    float lambda = lambda_0 * lambda_T * lambda_S;

    // 3. ARM NEON 4要素並列 FMA: out = static + lambda * delta
    float32x4_t v_lambda = vdupq_n_f32(lambda);
    int i = 0;
    for (; i <= size - 4; i += 4) {
        float32x4_t v_static = vld1q_f32(static_ptr + i);
        float32x4_t v_delta  = vld1q_f32(delta_ptr + i);
        float32x4_t v_out    = vmlaq_f32(v_static, v_delta, v_lambda);
        vst1q_f32(out_ptr + i, v_out);
    }
    for (; i < size; ++i) {
        out_ptr[i] = static_ptr[i] + lambda * delta_ptr[i];
    }
}
```

---

## 3. Haskell 状態追跡モジュール (`OrientedMatroid.DecayDecider`)

```haskell
data GameContext = GameContext
    { stepsOffTrunk      :: !Int   -- ^ 定跡から外れてからの経過手数 (n)
    , gaifullinDeviation :: !Float -- ^ 不変量のトポロジカルな歪み (delta p_2)
    , baseLambda         :: !Float -- ^ 初期とがめ強度 (lambda_0)
    } deriving (Show, Eq)
```

---

## 4. ソースコード対応表

| ファイルパス | 言語 | 役割 |
|---|---|---|
| [`app/src/main/cpp/DecayKernel.h`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/DecayKernel.h) | C++ ヘッダ | `merge_tensors_neon` 宣言 |
| [`app/src/main/cpp/DecayKernel.cpp`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/DecayKernel.cpp) | C++ 実装 | ARM NEON FMA 時空減衰テンソルマージ |
| [`src/OrientedMatroid/DecayDecider.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/DecayDecider.hs) | Haskell | `GameContext`, `computeSpatiotemporalLambda`, `mergeTensors` FFI |
| [`app/src/main/cpp/CMakeLists.txt`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/CMakeLists.txt) | CMake | `DecayKernel.cpp` ビルドターゲット登録 |
