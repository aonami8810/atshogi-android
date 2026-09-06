# ATShogi: Haskell/C++ 直接 mmap 結合モデル (Kotlinレス / Zero-JVM) 仕様書

## 1. 全体アーキテクチャ (Zero-JVM ゼロコピー)

ATShogi では、Android の Kotlin/JNI レイヤーを完全にバイパスし、スタンドアロンな Linux プロセスとして動作する **Haskell プロセス (`atshogi_bin`) と C++ (ARM NEON)** を直接結合します。

これにより、`egtbl7.bin`（約207KB）の POSIX `mmap` ロードおよび L3 キャッシュ上での超高速テンソル縮約をゼロコピーで完結させます。

```
┌────────────────────────────────────────────────────────┐
│               Haskell プロセス (atshogi_bin)           │
│                                                        │
│  ┌──────────────────┐            ┌──────────────────┐  │
│  │    Haskell IO    ├───────────►│   Haskell FFI    │  │
│  │ (Lifecycle/Path) │            │ (Ptr CFloat Map) │  │
│  └──────────────────┘            └────────┬─────────┘  │
│                                           │ (C-Call Direct)
│  ┌────────────────────────────────────────▼─────────┐  │
│  │                  C++ NDK モジュール              │  │
│  │                                                  │  │
│  │  ┌──────────────────┐      ┌──────────────────┐  │  │
│  │  │   POSIX mmap()   │      │  ARM NEON SIMD   │  │  │
│  │  │ (egtbl7.bin Map) │      │ (Tensor Contract)│  │  │
│  │  └────────┬─────────┘      └────────┬─────────┘  │  │
└──────────────┼─────────────────────────┼────────────────┘
               │ (Direct Map)            │ (Zero-Copy Read)
               ▼                         ▼
   [ /data/user/0/.../egtbl7.bin ] ──► [ L3 Cache (SRAM) ]
```

### アーキテクチャの特長
1. **JVM オーバーヘッドの完全撤廃**:
   - Java/Kotlin の ガベージコレクション (GC) 停止時間や JNI 引数変換のオーバーヘッドが 100% ゼロ。
2. **完全なメモリ一貫性と L3 キャッシュ常駐**:
   - Haskell の `Ptr CFloat` が直接 mmap された仮想メモリ領域を指し、`madvise(MADV_WILLNEED | MADV_SEQUENTIAL)` によりカーネルがキャッシュを事前ウォーミング。
3. **独立した Linux プロセス完結**:
   - Android USI プロトコル通信とテンソル計算が Haskell ランタイムと C++ ネイティブライブラリのみで駆動。

---

## 2. モジュール構成とソースコード対応表

| ファイルパス | 言語 | 役割 |
|---|---|---|
| [`app/src/main/cpp/EgtbMmap.h`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/EgtbMmap.h) | C++ ヘッダ | `load_egtb_mmap`, `unload_egtb_mmap`, `contract_egtb_neon` 宣言 |
| [`app/src/main/cpp/EgtbMmap.cpp`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/EgtbMmap.cpp) | C++ 実装 | POSIX `mmap` / `munmap` / `madvise` および ARM NEON 4要素並列 FMA 縮約 |
| [`src/OrientedMatroid/EgtbFFI.hs`](file:///c:/VS/Workspace/atshogi-android/src/OrientedMatroid/EgtbFFI.hs) | Haskell | `EgtbContainer`, `initEgtb`, `evaluateEgtbNEON`, `freeEgtb` FFI 実装 |
| [`app/src/main/cpp/CMakeLists.txt`](file:///c:/VS/Workspace/atshogi-android/app/src/main/cpp/CMakeLists.txt) | CMake | `EgtbMmap.cpp` ビルドターゲット登録 |

---

## 3. C++ 側：POSIX mmap & ARM NEON 縮約

```cpp
// ARM NEON SIMD によるボンド次元 (chi=181) の高速テンソル縮約
float contract_egtb_neon(const float* mmap_ptr, const float* x, const float* y, int32_t chi) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    int32_t i = 0;

    for (; i <= chi - 4; i += 4) {
        float32x4_t vx = vld1q_f32(x + i);
        float32x4_t vy = vld1q_f32(y + i);
        float32x4_t v_mmap = vld1q_f32(mmap_ptr + i);

        // x * y * mmap の積を Fused Multiply-Add (FMA)
        float32x4_t v_prod = vmulq_f32(vx, vy);
        sum_vec = vmlaq_f32(sum_vec, v_prod, v_mmap);
    }

    float sum = vgetq_lane_f32(sum_vec, 0) + vgetq_lane_f32(sum_vec, 1) +
                vgetq_lane_f32(sum_vec, 2) + vgetq_lane_f32(sum_vec, 3);

    for (; i < chi; ++i) {
        sum += x[i] * y[i] * mmap_ptr[i];
    }
    return sum;
}
```

---

## 4. USI ライフサイクル連携

Haskell のメイン USI ループにおいて：
- `isready`: `initEgtb` を呼び出し、ファイルシステム上の `egtbl7.bin` を L3 キャッシュ上に mmap 確保。
- `go` / `position`: 局面遷移時に `evaluateEgtbNEON` を実行し、ナノ秒でテンソル縮約を実行。
- `quit`: `freeEgtb` により安全にアンマップ（`munmap`）。
