#include <stdint.h>
#include <float.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#elif defined(__AVX512F__)
#include <immintrin.h>
#define HAS_AVX512 1
#endif

// 81マス × 81マスの局所接続を表す更新セグメント（v2仕様）
struct SparseHasseTransitionSegmentV2 {
    // 1局面あたり、最大8つの遷移先を SoA 形式で格納
    // weights には、事前計算された対数エントロピー c_base + μ * ln(B(u)) が直接格納される
    float weights[16][8];  
    uint32_t target_indices[16][8];

    // 駒獲得ノード（k -> k-1 次元縮退）用のアンカー電位。
    // 通常のノードの場合は FLT_MAX が埋め込まれている（ディリクレ境界の自動フラグ）
    float intermediate_sinks[16];  
};

extern "C" {

#include <math.h> // for logf

/**
 * 高速着手生成・分岐数・駒獲得検知バインド
 * @param active_indices 16局面のHasseインデックス
 * @param seg 境界セグメントV2構造体ポインタ
 * @param current_potentials 現在のポテンシャル配列 (k階)
 * @param k_minus_1_potentials 解決済みのポテンシャル配列 (k-1階)
 */
void get_hasse_branching_and_sinks(
    const uint32_t* active_indices,
    struct SparseHasseTransitionSegmentV2* seg,
    const float* current_potentials,
    const float* k_minus_1_potentials
) {
    for (int lane = 0; lane < 16; ++lane) {
        uint32_t idx = active_indices[lane];
        
        // 1. 中間シンク（駒獲得ホモトピー収縮）判定
        // 本来は合法手生成器で駒の捕獲を検知する。ここではモックとして1%の確率で発火。
        bool is_capture = (idx % 129 == 0); 
        if (is_capture) {
            float material_bonus = -300.0f; 
            // 解決済みの k-1 階層からアンカー電位を引き抜く
            // ここではモックとして k_minus_1_potentials を 0 として扱う
            seg->intermediate_sinks[lane] = 5000.0f + material_bonus;
        } else {
            seg->intermediate_sinks[lane] = FLT_MAX;
        }

        // 2. 対数エントロピー容量コストの算出
        // 本来は合法手生成器の数。ここでは擬似的に 15〜30手 の分岐と仮定。
        int branching_factor = 15 + (idx % 15);
        float c_base = 5.0f;
        float mu = 2.0f; // 情報幾何学的なリーマン曲率補正
        float entropy_cost = c_base + mu * logf((float)branching_factor);

        for (int j = 0; j < 8; ++j) {
            // トポロジカルな隣接局面に遷移
            seg->target_indices[lane][j] = (idx + j + 1) % 531441;
            seg->weights[lane][j] = entropy_cost;
        }
    }
}

/**
 * 16局面並列で、対数エントロピーコストおよび中間シンク（駒獲得ノード）を考慮した
 * トポロジカル大域スイープを実行する（0ms完結、ノンブロッキング）。
 *
 * @param potentials_array [in/out] 全体オービフォールド空間のポテンシャル場 (float[])
 * @param seg              [in]     エントロピー・中間シンク構造体 (SoA)
 * @param active_indices   [in]     現在スイープ対象のノードインデックス（16要素）
 * @param out_changed_mask [out]    値の更新が発生したレーンのビットマスク（16bit）
 */
void sweep_tropical_boundary_v2(
    float* potentials_array,
    const SparseHasseTransitionSegmentV2* seg,
    const uint32_t* active_indices,
    uint32_t* out_changed_mask
) {
    *out_changed_mask = 0;

#if defined(HAS_AVX512)
    // --- AVX-512 極限並列実装 ---
    // 1. 各車線（16レーン）の現在のポテンシャルを一括ロード
    __m512i v_indices = _mm512_loadu_si512((const __m512i*)active_indices);
    __m512 v_current_pots = _mm512_i32gather_ps(v_indices, potentials_array, 4);

    // 2. 中間シンク（駒獲得アンカー電位）をロード
    __m512 v_sinks = _mm512_loadu_ps(seg->intermediate_sinks);
    __m512 v_flt_max = _mm512_set1_ps(FLT_MAX);

    // 3. 中間シンクが存在するレーンを判定するビットマスク
    // (sink < FLT_MAX ならば、そこは駒獲得による次元収縮 Dirichlet 極)
    __mmask16 m_is_sink = _mm512_cmp_ps_mask(v_sinks, v_flt_max, _CMP_LT_OQ);

    // 4. トロピカル最短路 (min-plus) の並列計算
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

        // トロピカル和: T_target + (c_base + μ * ln(B(u)))
        __m512 v_sum = _mm512_add_ps(v_target_pots, v_weights);
        v_min_pots = _mm512_min_ps(v_min_pots, v_sum);
    }

    // 5. 中間シンクレーンに対して、アンカー電位を優先マージ (条件分岐の排除)
    __m512 v_final_pots = _mm512_mask_blend_ps(m_is_sink, v_min_pots, v_sinks);

    // 6. 値の変化を検出 (誤差 1e-5 以上)
    __m512 v_diff = _mm512_sub_ps(v_current_pots, v_final_pots);
    __m512 v_epsilon = _mm512_set1_ps(1e-5f);
    __mmask16 m_changed = _mm512_cmp_ps_mask(v_diff, v_epsilon, _CMP_GT_OQ);
    *out_changed_mask = (uint32_t)m_changed;

    // 7. 更新が発生した局面のみ、メイン配列に散布ストア (Scatter)
    if (m_changed > 0) {
        _mm512_mask_i32scatter_ps(potentials_array, m_changed, v_indices, v_final_pots, 4);
    }

#elif defined(HAS_NEON)
    // --- ARM NEON (AArch64) 4並列展開実装 ---
    uint32_t final_mask = 0;

    for (int b = 0; b < 16; b += 4) {
        uint32x4_t v_idx = vld1q_u32(&active_indices[b]);

        // 4車線の現在電位をギャザーロード
        float32_t curr_pots[4];
        float32_t sinks[4];
        for (int l = 0; l < 4; ++l) {
            curr_pots[l] = potentials_array[active_indices[b + l]];
            sinks[l] = seg->intermediate_sinks[b + l];
        }
        float32x4_t v_current = vld1q_f32(curr_pots);
        float32x4_t v_sinks = vld1q_f32(sinks);
        float32x4_t v_flt_max = vdupq_n_f32(FLT_MAX);

        // スイープ計算の実行
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

        // 中間シンクマスクの判定: (v_sinks < FLT_MAX)
        uint32x4_t v_is_sink = vcltq_f32(v_sinks, v_flt_max);

        // マスク選択 (v_is_sink のレーンは v_sinks、それ以外は v_min を選択)
        float32x4_t v_final = vbslq_f32(v_is_sink, v_sinks, v_min);

        // 差分比較
        float32x4_t v_diff = vsubq_f32(v_current, v_final);
        float32x4_t v_eps = vdupq_n_f32(1e-5f);
        uint32x4_t v_cmp = vcgtq_f32(v_diff, v_eps);

        uint32_t mask[4];
        vst1q_u32(mask, v_cmp);
        float32_t final_pots[4];
        vst1q_f32(final_pots, v_final);

        for (int l = 0; l < 4; ++l) {
            if (mask[l]) {
                potentials_array[active_indices[b + l]] = final_pots[l];
                final_mask |= (1U << (b + l));
            }
        }
    }
    *out_changed_mask = final_mask;

#else
    // --- ポータブル・フォールバック ---
    uint32_t final_mask = 0;
    for (int lane = 0; lane < 16; ++lane) {
        uint32_t idx = active_indices[lane];
        float current_pot = potentials_array[idx];
        float final_pot = FLT_MAX;

        // 1. 中間シンク（駒獲得）のディリクレ境界判定
        if (seg->intermediate_sinks[lane] < FLT_MAX) {
            final_pot = seg->intermediate_sinks[lane];
        } else {
            // 2. 通常のトロピカル最小路更新
            for (int j = 0; j < 8; ++j) {
                uint32_t tgt = seg->target_indices[lane][j];
                float w = seg->weights[lane][j];
                float val = potentials_array[tgt] + w;
                if (val < final_pot) {
                    final_pot = val;
                }
            }
        }

        // 3. 差分判定と書き戻し
        if ((current_pot - final_pot) > 1e-5f) {
            potentials_array[idx] = final_pot;
            final_mask |= (1U << lane);
        }
    }
    *out_changed_mask = final_mask;
#endif
}

}
