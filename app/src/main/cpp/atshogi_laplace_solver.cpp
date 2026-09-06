/**
 * ATShogi-OM Extreme
 * atshogi_laplace_solver.cpp
 *
 * オービフォールド上の共形ラプラス方程式（等ポテンシャル場）決定論的ソルバー
 */

#include <stdint.h>
#include <float.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#elif defined(__AVX512F__)
#include <immintrin.h>
#define HAS_AVX512 1
#endif

// オービフォールド上の局所接続（ラプラシアンの隣接係数）を表すSoA構造体
// 各ノード（局面）は平均4つのトポロジカル近傍と接続される
struct OrbifoldLaplacianSegment {
    float adjacency_weights[16][4];     // 隣接セルとの結合重み w_ij
    uint32_t neighbor_indices[16][4];   // 隣接セルのインデックス
};

extern "C" {

/**
 * 16ノード並列で、Jacobi反復によるラプラス調和ポテンシャルの1ステップ更新を実行する。
 * T_new(x) = (sum_{j=0..3} w_ij * T_old(neighbor_j)) / sum_w
 *
 * @param potentials_array [in/out] 状態空間全体のポテンシャル配列 (float[])
 * @param next_potentials  [out]    更新後のポテンシャル配列 (float[])
 * @param seg              [in]     オービフォールド接続トポロジー SoA
 * @param active_indices   [in]     現在反復対象のノードインデックス（16要素）
 * @param boundary_mask    [in]     王の位置などの境界条件フラグ (16bitマスク。1のレーンは値を更新せず固定)
 */
void solve_laplace_iteration_step(
    const float* potentials_array,
    float* next_potentials,
    const OrbifoldLaplacianSegment* seg,
    const uint32_t* active_indices,
    uint32_t boundary_mask
) {
#if defined(HAS_AVX512)
    // --- AVX-512 実装 (ホスト PC 高速生成用) ---
    __m512i v_indices = _mm512_loadu_si512((const __m512i*)active_indices);
    __m512 v_current_pots = _mm512_i32gather_ps(v_indices, potentials_array, 4);

    // 累積器
    __m512 v_weighted_sum = _mm512_setzero_ps();
    __m512 v_weight_total = _mm512_setzero_ps();

    for (int j = 0; j < 4; ++j) {
        alignas(64) uint32_t neighbors[16];
        alignas(64) float weights[16];
        for (int lane = 0; lane < 16; ++lane) {
            neighbors[lane] = seg->neighbor_indices[lane][j];
            weights[lane] = seg->adjacency_weights[lane][j];
        }

        __m512i v_neigh_idx = _mm512_loadu_si512((const __m512i*)neighbors);
        __m512 v_w = _mm512_loadu_ps(weights);

        // 隣接ノードのポテンシャルをギャザーロード
        __m512 v_neigh_pots = _mm512_i32gather_ps(v_neigh_idx, potentials_array, 4);

        // 加重合計 w * T と重みの総和の積算 (FMA: _mm512_fmadd_ps)
        v_weighted_sum = _mm512_fmadd_ps(v_w, v_neigh_pots, v_weighted_sum);
        v_weight_total = _mm512_add_ps(v_weight_total, v_w);
    }

    // T_new = weighted_sum / weight_total
    __m512 v_new_pots = _mm512_div_ps(v_weighted_sum, v_weight_total);

    // 境界条件（ディリクレ極：王の位置）は更新せず、元のポテンシャルを維持する
    __mmask16 m_boundary = (__mmask16)boundary_mask;
    __m512 v_final_pots = _mm512_mask_blend_ps(m_boundary, v_new_pots, v_current_pots);

    // 計算結果の書き出し (Scatter)
    _mm512_i32scatter_ps(next_potentials, v_indices, v_final_pots, 4);

#elif defined(HAS_NEON)
    // --- ARM NEON 実装 (Android / moto g05 互換用) ---
    for (int b = 0; b < 16; b += 4) {
        float32_t curr_pots[4];
        for (int l = 0; l < 4; ++l) {
            curr_pots[l] = potentials_array[active_indices[b + l]];
        }
        float32x4_t v_current = vld1q_f32(curr_pots);

        float32x4_t v_weighted_sum = vdupq_n_f32(0.0f);
        float32x4_t v_weight_total = vdupq_n_f32(0.0f);

        for (int j = 0; j < 4; ++j) {
            float32_t neighbors[4];
            float32_t weights[4];
            for (int l = 0; l < 4; ++l) {
                neighbors[l] = potentials_array[seg->neighbor_indices[b + l][j]];
                weights[l] = seg->adjacency_weights[b + l][j];
            }
            float32x4_t v_neigh = vld1q_f32(neighbors);
            float32x4_t v_w = vld1q_f32(weights);

            // NEON FMA (vmulq & vaddq)
            v_weighted_sum = vmlaq_f32(v_weighted_sum, v_neigh, v_w);
            v_weight_total = vaddq_f32(v_weight_total, v_w);
        }

        // T_new = weighted_sum / weight_total (NEON除算)
        // NEONには直接の除算命令がないシステム向けに、逆数近似と乗算ステップ、あるいはポータブルスカラー除算
        float32_t sum_array[4];
        float32_t total_array[4];
        vst1q_f32(sum_array, v_weighted_sum);
        vst1q_f32(total_array, v_weight_total);

        float32_t out_pots[4];
        for (int l = 0; l < 4; ++l) {
            // 境界マスクビットの評価
            uint32_t is_boundary = (boundary_mask >> (b + l)) & 1U;
            if (is_boundary) {
                out_pots[l] = curr_pots[l]; // ディリクレ固定
            } else {
                out_pots[l] = total_array[l] > 0.0f ? (sum_array[l] / total_array[l]) : 0.0f;
            }
            next_potentials[active_indices[b + l]] = out_pots[l];
        }
    }

#else
    // --- 標準ポータブルスカラフォールバック ---
    for (int lane = 0; lane < 16; ++lane) {
        uint32_t idx = active_indices[lane];
        uint32_t is_boundary = (boundary_mask >> lane) & 1U;

        if (is_boundary) {
            next_potentials[idx] = potentials_array[idx]; // ディリクレ極固定
        } else {
            float sum_weighted = 0.0f;
            float sum_weights = 0.0f;
            for (int j = 0; j < 4; ++j) {
                uint32_t neigh = seg->neighbor_indices[lane][j];
                float w = seg->adjacency_weights[lane][j];
                sum_weighted += w * potentials_array[neigh];
                sum_weights += w;
            }
            next_potentials[idx] = sum_weights > 0.0f ? (sum_weighted / sum_weights) : 0.0f;
        }
    }
#endif
}

}
