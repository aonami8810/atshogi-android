#include "DecayKernel.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

extern "C" {

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
    if (!static_ptr || !delta_ptr || !out_ptr || size <= 0) return;

    // 1. Temporal geometric decay computation: gamma^n via fast multiplication loop
    float lambda_T = 1.0f;
    for (int i = 0; i < n; ++i) {
        lambda_T *= gamma;
    }

    // 2. Spatial/topological squeeze computation: delta_p2^2 / (delta_p2^2 + epsilon)
    float dp2_sq = delta_p2 * delta_p2;
    float lambda_S = dp2_sq / (dp2_sq + epsilon);

    // Final spatiotemporal dynamic interpolation coefficient lambda
    float lambda = lambda_0 * lambda_T * lambda_S;

    int i = 0;

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // 3. ARM NEON 4-element parallel SIMD merge: T_total = T_static + lambda * delta_T
    float32x4_t v_lambda = vdupq_n_f32(lambda);

    // Loop unrolled 4 elements per iteration using Fused Multiply-Add (FMA)
    for (; i <= size - 4; i += 4) {
        float32x4_t v_static = vld1q_f32(static_ptr + i);
        float32x4_t v_delta  = vld1q_f32(delta_ptr + i);

        // out = static + lambda * delta
        float32x4_t v_out = vmlaq_f32(v_static, v_delta, v_lambda);

        vst1q_f32(out_ptr + i, v_out);
    }
#endif

    // Residual elements & non-ARM fallback
    for (; i < size; ++i) {
        out_ptr[i] = static_ptr[i] + lambda * delta_ptr[i];
    }
}

} // extern "C"
