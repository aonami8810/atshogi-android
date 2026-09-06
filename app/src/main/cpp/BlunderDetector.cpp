#include "BlunderDetector.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

extern "C" {

float calculate_blunder_lambda0(
    const float* __restrict g_expected,
    const float* __restrict g_actual,
    float theta_sq,
    float lambda_max
) {
    if (!g_expected || !g_actual) return 0.0f;

    float d_sq = 0.0f;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // 4 independent accumulators in NEON registers to eliminate pipeline dependency stalls
    float32x4_t sum_vec0 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec1 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec2 = vdupq_n_f32(0.0f);
    float32x4_t sum_vec3 = vdupq_n_f32(0.0f);

    // 16-element unrolled loop processing 80 elements (16 * 5 = 80)
    int i = 0;
    for (; i < 80; i += 16) {
        float32x4_t e0 = vld1q_f32(&g_expected[i]);
        float32x4_t a0 = vld1q_f32(&g_actual[i]);
        float32x4_t e1 = vld1q_f32(&g_expected[i + 4]);
        float32x4_t a1 = vld1q_f32(&g_actual[i + 4]);
        float32x4_t e2 = vld1q_f32(&g_expected[i + 8]);
        float32x4_t a2 = vld1q_f32(&g_actual[i + 8]);
        float32x4_t e3 = vld1q_f32(&g_expected[i + 12]);
        float32x4_t a3 = vld1q_f32(&g_actual[i + 12]);

        float32x4_t diff0 = vsubq_f32(a0, e0);
        float32x4_t diff1 = vsubq_f32(a1, e1);
        float32x4_t diff2 = vsubq_f32(a2, e2);
        float32x4_t diff3 = vsubq_f32(a3, e3);

        sum_vec0 = vmlaq_f32(sum_vec0, diff0, diff0);
        sum_vec1 = vmlaq_f32(sum_vec1, diff1, diff1);
        sum_vec2 = vmlaq_f32(sum_vec2, diff2, diff2);
        sum_vec3 = vmlaq_f32(sum_vec3, diff3, diff3);
    }

    float32x4_t sum_intermediate1 = vaddq_f32(sum_vec0, sum_vec1);
    float32x4_t sum_intermediate2 = vaddq_f32(sum_vec2, sum_vec3);
    float32x4_t sum_total_vec = vaddq_f32(sum_intermediate1, sum_intermediate2);

    d_sq = vgetq_lane_f32(sum_total_vec, 0) +
           vgetq_lane_f32(sum_total_vec, 1) +
           vgetq_lane_f32(sum_total_vec, 2) +
           vgetq_lane_f32(sum_total_vec, 3);

    // 81st square residual handling
    float last_diff = g_actual[80] - g_expected[80];
    d_sq += last_diff * last_diff;
#elif defined(__AVX512F__)
    // AVX-512: 16-element vectors. 80 elements = 5 iterations.
    __m512 sum_vec = _mm512_setzero_ps();
    int i = 0;
    for (; i < 80; i += 16) {
        __m512 e0 = _mm512_loadu_ps(&g_expected[i]);
        __m512 a0 = _mm512_loadu_ps(&g_actual[i]);
        __m512 diff0 = _mm512_sub_ps(a0, e0);
        sum_vec = _mm512_fmadd_ps(diff0, diff0, sum_vec);
    }
    float d_sq_arr[16];
    _mm512_storeu_ps(d_sq_arr, sum_vec);
    for(int k=0; k<16; ++k) d_sq += d_sq_arr[k];

    float last_diff = g_actual[80] - g_expected[80];
    d_sq += last_diff * last_diff;

#elif defined(__AVX2__)
    // AVX2: 8-element vectors. 80 elements = 10 iterations. Unrolled to 2 accumulators.
    __m256 sum_vec0 = _mm256_setzero_ps();
    __m256 sum_vec1 = _mm256_setzero_ps();
    int i = 0;
    for (; i < 80; i += 16) {
        __m256 e0 = _mm256_loadu_ps(&g_expected[i]);
        __m256 a0 = _mm256_loadu_ps(&g_actual[i]);
        __m256 e1 = _mm256_loadu_ps(&g_expected[i + 8]);
        __m256 a1 = _mm256_loadu_ps(&g_actual[i + 8]);

        __m256 diff0 = _mm256_sub_ps(a0, e0);
        __m256 diff1 = _mm256_sub_ps(a1, e1);

        sum_vec0 = _mm256_fmadd_ps(diff0, diff0, sum_vec0);
        sum_vec1 = _mm256_fmadd_ps(diff1, diff1, sum_vec1);
    }
    __m256 sum_total = _mm256_add_ps(sum_vec0, sum_vec1);
    
    // Horizontal sum over __m256
    __m256 t1 = _mm256_hadd_ps(sum_total, sum_total);
    __m256 t2 = _mm256_hadd_ps(t1, t1);
    float d_sq_arr[8];
    _mm256_storeu_ps(d_sq_arr, t2);
    d_sq = d_sq_arr[0] + d_sq_arr[4];

    float last_diff = g_actual[80] - g_expected[80];
    d_sq += last_diff * last_diff;

#else
    // Optimized scalar fallback
    for (int i = 0; i < 81; ++i) {
        float diff = g_actual[i] - g_expected[i];
        d_sq += diff * diff;
    }
#endif

    // Hill saturation evaluation (Single FPU division, zero square roots)
    if (d_sq <= 0.0f) return 0.0f;
    return lambda_max * (d_sq / (d_sq + theta_sq));
}

} // extern "C"
