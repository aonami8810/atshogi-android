/**
 * ATShogi-OM Extreme
 * atshogi_mera_kernel.cpp
 *
 * 512-ply Multi-scale Renormalization Tensor Dynamic Local Contraction (Causal Cone) Kernel
 */

#include <stdint.h>
#include <float.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#endif

extern "C" {

/**
 * Contract two potential bonds via Disentangler (U) and Isometry (V).
 */
void contract_mera_step_neon(
    float* out_potentials,
    const float* left_pots,
    const float* right_pots,
    const float* tensor_U,
    const float* tensor_V,
    int chi
) {
#if defined(HAS_NEON)
    // NEON SIMD Implementation for ARM
    alignas(16) float detangled_temp[32761]; // max chi=181 -> 181*181 = 32761
    
    // 1. Detangling contraction with U
    for (int ab = 0; ab < chi * chi; ab += 4) {
        float32x4_t v_temp = vdupq_n_f32(0.0f);

        for (int c = 0; c < chi; ++c) {
            float32x4_t v_left = vdupq_n_f32(left_pots[c]);

            for (int d = 0; d < chi; d += 4) {
                float32x4_t v_right = vld1q_f32(&right_pots[d]);
                int u_idx = (c * chi + d) * (chi * chi) + ab;
                float32x4_t v_u = vld1q_f32(&tensor_U[u_idx]);
                float32x4_t v_prod = vmulq_f32(v_left, v_right);
                v_temp = vmlaq_f32(v_temp, v_prod, v_u);
            }
        }
        vst1q_f32(&detangled_temp[ab], v_temp);
    }

    // 2. Isometry projection with V
    for (int i = 0; i < chi; ++i) {
        float32x4_t v_out_sum = vdupq_n_f32(0.0f);

        for (int ab = 0; ab < chi * chi; ab += 4) {
            float32x4_t v_temp_val = vld1q_f32(&detangled_temp[ab]);
            int v_idx = ab * chi + i;
            float32x4_t v_v = vld1q_f32(&tensor_V[v_idx]);
            v_out_sum = vmlaq_f32(v_out_sum, v_temp_val, v_v);
        }
        
        out_potentials[i] = vgetq_lane_f32(v_out_sum, 0) +
                            vgetq_lane_f32(v_out_sum, 1) +
                            vgetq_lane_f32(v_out_sum, 2) +
                            vgetq_lane_f32(v_out_sum, 3);
    }
#else
    // --- 劇的に最適化された O(χ⁴) 標準ポータブルスカラフォールバック ---
    
    // 中間テンソル Temp[a][b] をローカルスタック上に配置 (chi <= 181 に対応、最大 32,761 要素)
    // スタック破壊を防ぐため動的スタックアロケート、または固定十分量でバッファを確保
    float Temp[181 * 181];

    // Phase 1: ディタングル縮約 [O(χ⁴)]
    // Temp[a, b] = sum_{c, d} left[c] * right[d] * U[c, d, a, b]
    for (int a = 0; a < chi; ++a) {
        for (int b = 0; b < chi; ++b) {
            float detangle_sum = 0.0f;
            int ab_offset = a * chi + b;
            
            for (int c = 0; c < chi; ++c) {
                float left_val = left_pots[c];
                int c_offset = c * chi;
                
                for (int d = 0; d < chi; ++d) {
                    float right_val = right_pots[d];
                    
                    // Uのメモリレイアウト: [c][d][a][b] => Row-major インデックス計算
                    int u_idx = ((c_offset + d) * chi + a) * chi + b;
                    detangle_sum += left_val * right_val * tensor_U[u_idx];
                }
            }
            Temp[ab_offset] = detangle_sum;
        }
    }

    // Phase 2: アイソメトリ等長射影 [O(χ³)]
    // Out[i] = sum_{a, b} Temp[a, b] * V[a, b, i]
    for (int i = 0; i < chi; ++i) {
        float projection_sum = 0.0f;
        
        for (int a = 0; a < chi; ++a) {
            int a_offset = a * chi;
            for (int b = 0; b < chi; ++b) {
                int ab_idx = a_offset + b;
                
                // Vのメモリレイアウト: [a][b][i]
                int v_idx = ab_idx * chi + i;
                projection_sum += Temp[ab_idx] * tensor_V[v_idx];
            }
        }
        out_potentials[i] = projection_sum;
    }
#endif
}

} // extern "C"
