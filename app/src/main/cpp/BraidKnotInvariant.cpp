#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
#include <stdint.h>

extern "C" {

float calculate_braid_linking_number_neon(
    const float* path_vectors,
    const float* optimal_vectors,
    int path_len
) {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32x4_t v_linking_sum = vdupq_n_f32(0.0f);

    for (int i = 0; i < (path_len - 1) * 3; i += 3) {
        float32x4_t p_curr = vld1q_f32(&path_vectors[i]);
        float32x4_t p_opt  = vld1q_f32(&optimal_vectors[i]);
        float32x4_t d_vector = vsubq_f32(p_curr, p_opt);
        float32x4_t v_sq = vmulq_f32(d_vector, d_vector);
        v_linking_sum = vaddq_f32(v_linking_sum, v_sq);
    }

    float sum = vgetq_lane_f32(v_linking_sum, 0) + 
                vgetq_lane_f32(v_linking_sum, 1) + 
                vgetq_lane_f32(v_linking_sum, 2);
#else
    float sum = 0.0f;
    for (int i = 0; i < (path_len - 1) * 3; ++i) {
        float diff = path_vectors[i] - optimal_vectors[i];
        sum += diff * diff;
    }
#endif
    return sum > 0.0f ? (1.0f / (1.0f + sum)) : 1.0f;
}

}
