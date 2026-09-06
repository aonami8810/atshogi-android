#include <stdint.h>
#include <float.h>
#include <stdio.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#elif defined(__AVX512F__)
#include <immintrin.h>
#define HAS_AVX512 1
#endif

// 局面の遷移関係（ハッセ図の1-細胞）をパックしたSoAセグメント構造体
struct SparseHasseTransitionSegment {
    float weights[16][8];          // トロピカルコスト（1-細胞の長さ：手番コスト）
    uint32_t target_indices[16][8]; // 遷移先局面インデックス（オービフォールド圧縮済み）
};

extern "C" {

/**
 * 16局面並列で、余境界作用素に基づくポテンシャル一括決定スイープを実行する。
 * T_new(x) = min_{j=0..7} ( T_old(target_j) + weight_j )
 */
void sweep_tropical_boundary_avx512_neon(
    float* potentials_array,
    const SparseHasseTransitionSegment* seg,
    const uint32_t* active_indices,
    uint32_t* out_changed_mask
) {
    *out_changed_mask = 0;

#if defined(HAS_AVX512)
    // --- AVX-512 実装 (ホスト PC 高速生成用) ---
    __m512i v_indices = _mm512_loadu_si512((const __m512i*)active_indices);
    __m512 v_current_pots = _mm512_i32gather_ps(v_indices, potentials_array, 4);
    __m512 v_min_pots = _mm512_set1_ps(FLT_MAX);

    for (int j = 0; j < 8; ++j) {
        alignas(64) uint32_t targets[16];
        alignas(64) float weights[16];
        for (int lane = 0; lane < 16; ++lane) {
            targets[lane] = seg->target_indices[lane][j];
            weights[lane] = seg->weights[lane][j];
        }

        __m512i v_targets = _mm512_loadu_si512((const __m512i*)targets);
        __m512 v_weights = _mm512_loadu_ps(weights);
        __m512 v_target_pots = _mm512_i32gather_ps(v_targets, potentials_array, 4);

        // トロピカル和 (T_target + weight) と最小値の更新 (min)
        __m512 v_sum = _mm512_add_ps(v_target_pots, v_weights);
        v_min_pots = _mm512_min_ps(v_min_pots, v_sum);
    }

    __m512 v_diff = _mm512_sub_ps(v_current_pots, v_min_pots);
    __m512 v_epsilon = _mm512_set1_ps(1e-5f);
    __mmask16 m_changed = _mm512_cmp_ps_mask(v_diff, v_epsilon, _CMP_GT_OQ);
    *out_changed_mask = (uint32_t)m_changed;

    if (m_changed > 0) {
        _mm512_mask_i32scatter_ps(potentials_array, m_changed, v_indices, v_min_pots, 4);
    }

#elif defined(HAS_NEON)
    // --- ARM NEON 実装 (Android / moto g05 互換用) ---
    uint32_t final_mask = 0;

    for (int b = 0; b < 16; b += 4) {
        float32_t curr_pots[4];
        for (int l = 0; l < 4; ++l) {
            curr_pots[l] = potentials_array[active_indices[b + l]];
        }
        float32x4_t v_current = vld1q_f32(curr_pots);
        float32x4_t v_min = vdupq_n_f32(FLT_MAX);

        for (int j = 0; j < 8; ++j) {
            float32_t targets[4];
            float32_t weights[4];
            for (int l = 0; l < 4; ++l) {
                targets[l] = potentials_array[seg->target_indices[b + l][j]];
                weights[l] = seg->weights[b + l][j];
            }
            float32x4_t v_tgt_pot = vld1q_f32(targets);
            float32x4_t v_weight = vld1q_f32(weights);

            float32x4_t v_sum = vaddq_f32(v_tgt_pot, v_weight);
            v_min = vminq_f32(v_min, v_sum);
        }

        float32x4_t v_diff = vsubq_f32(v_current, v_min);
        float32x4_t v_eps = vdupq_n_f32(1e-5f);
        uint32x4_t v_cmp = vcgtq_f32(v_diff, v_eps);

        uint32_t mask[4];
        vst1q_u32(mask, v_cmp);
        float32_t final_pots[4];
        vst1q_f32(final_pots, v_min);

        for (int l = 0; l < 4; ++l) {
            if (mask[l]) {
                potentials_array[active_indices[b + l]] = final_pots[l];
                final_mask |= (1U << (b + l));
            }
        }
    }
    *out_changed_mask = final_mask;

#else
    // --- 標準ポータブルスカラフォールバック ---
    printf("Sweep scalar lane chunk start\n"); fflush(stdout);
    uint32_t final_mask = 0;
    for (int lane = 0; lane < 16; ++lane) {
        uint32_t idx = active_indices[lane];
        float current_pot = potentials_array[idx];
        float min_val = FLT_MAX;

        for (int j = 0; j < 8; ++j) {
            uint32_t tgt = seg->target_indices[lane][j];
            float w = seg->weights[lane][j];
            float val = potentials_array[tgt] + w;
            if (val < min_val) {
                min_val = val;
            }
        }

        if ((current_pot - min_val) > 1e-5f) {
            potentials_array[idx] = min_val;
            final_mask |= (1U << lane);
        }
    }
    *out_changed_mask = final_mask;
#endif
}

}
