# ATShogi: 勾配ノルム急増度に基づく悪手自動逆算アルゴリズム (Blunder Detection Engine) 設計書

## 1. 数理モデリング：勾配残差による悪手度 $\lambda_0$ の動的決定

ATShogi のテイラー展開カーネル（`TaylorKernelNEON.cpp`）は、局面 $s$ におけるトポロジカルな評価勾配ベクトル $\mathbf{g} \in \mathbb{R}^{81}$ を算定します。

### 1.1 期待勾配と実際勾配の差分（Blunder Metric）
- $\mathbf{g}_{\text{expected}} \in \mathbb{R}^{81}$: 完全定跡（安定多様体）が示す最善手を指した後の期待局面における勾配ベクトル
- $\mathbf{g}_{\text{actual}} \in \mathbb{R}^{81}$: 相手が実際に指した局面における勾配ベクトル

相手が悪手（Blunder）を指して不安定多様体（急峻な崖）に足を踏み入れた瞬間、勾配残差ベクトル：

$$\Delta \mathbf{g} = \mathbf{g}_{\text{actual}} - \mathbf{g}_{\text{expected}} \in \mathbb{R}^{81}$$

のノルムが急増（スパイク）します。

---

### 1.2 平方根（sqrt）を排除した代数型 Hill 評価関数
平方根命令（`vsqrt`）を排除するため、**平方ノルム（Square Norm） $D^2$** を直接用いた飽和型 Hill 関数により、初期悪手度 $\lambda_0$ を代数的に逆算します：

$$D^2 = \|\Delta \mathbf{g}\|_2^2 = \sum_{i=1}^{81} (g_{\text{actual}, i} - g_{\text{expected}, i})^2$$

$$\lambda_0(D^2) = \lambda_{\max} \cdot \frac{D^2}{D^2 + \theta^2}$$

- $\lambda_{\max} = 1.0$: 最大補正強度
- $\theta^2$: 半飽和閾値パラメータ（勾配差感度の中間点）

---

## 2. C++ (ARM NEON) 高速 SIMD 実装

moto g05 の 1MB L3 キャッシュを汚染しないよう、81要素のベクトル減算・二乗・累積加算を 4 つの NEON レジスタアキュムレータで並列展開（FMA: `vmlaq_f32`）し、パイプラインのレイテンシストールを完全防止します。

```cpp
float calculate_blunder_lambda0(
    const float* __restrict g_expected,
    const float* __restrict g_actual,
    float theta_sq,
    float lambda_max
) {
    float32x4_t sum_vec0 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec1 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec2 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec3 = vdupq_n_f32(0.0f);

    // 16要素単位でアンロール (80要素を高速処理)
    for (int i = 0; i < 80; i += 16) {
        float32x4_t e0 = vld1q_f32(&g_expected[i]);
        float32x4_t a0 = vld1q_f32(&g_actual[i]);
        float32x4_t diff0 = vsubq_f32(a0, e0);
        sum_vec0 = vmlaq_f32(sum_vec0, diff0, diff0);
        // ... (diff1, diff2, diff3 も同様に並列積和)
    }

    // 4アキュムレータ統合 + 81マス目の端数処理
    // ...
    return lambda_max * (d_sq / (d_sq + theta_sq));
}
```

---

## 3. 全自動フィードバックループ

```
 [ 相手の指し手 ]
       │
       ▼
 1. 実際局面の勾配 g_actual を取得 (テイラーカーネル)
       │
       ▼
 2. 予定していた最善勾配 g_expected との差分平方ノルム D^2 を算出 (BlunderDetector)
       │
       ▼
 3. D^2 から Hill 関数を用いて 悪手度 λ_0 を動的決定 (0.0 ～ 1.0)
       │
       ▼
 4. オンライン差分テンソル ΔT に λ_0 を乗算
       │
       ▼
 5. 定跡テンソルを一元更新: T_total = T_static + λ_0 * γ^n * λ_S * ΔT (DecayKernel)
       │
       ▼
 6. 次手以降、C++は一切の分岐なしに T_total を縮約参照 (0ms で「とがめ手順」を実行)
```

---

## 4. ソースコード対応表

| ファイルパス | 言語 | 役割 |
|---|---|---|
| [`app/src/main/cpp/BlunderDetector.h`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/BlunderDetector.h) | C++ ヘッダ | `calculate_blunder_lambda0` 宣言 |
| [`app/src/main/cpp/BlunderDetector.cpp`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/BlunderDetector.cpp) | C++ 実装 | 4並列アキュムレータ NEON FMA 差分平方ノルム＆Hill評価 |
| [`src/OrientedMatroid/BlunderDetector.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/BlunderDetector.hs) | Haskell | `detectBlunderSeverity`, `detectBlunderSeverityPtr` FFI |
| [`app/src/main/cpp/CMakeLists.txt`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/CMakeLists.txt) | CMake | `BlunderDetector.cpp` ビルドターゲット登録 |
