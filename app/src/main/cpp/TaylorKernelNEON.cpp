#include "TaylorKernelNEON.h"
#include <cstddef>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace atshogi::simd {

void evaluateTaylorExpansionNEON(
    const float* featureWeights,
    const float* deltaX,
    const float* hessianDiag,
    float* outEvalDeltas,
    size_t count
) {
    size_t i = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    const size_t simdLimit = count - (count % 4);
    const float32x4_t vHalf = vdupq_n_f32(0.5f);

    for (; i < simdLimit; i += 4) {
        float32x4_t vW = vld1q_f32(&featureWeights[i]);
        float32x4_t vDX = vld1q_f32(&deltaX[i]);
        float32x4_t vH = vld1q_f32(&hessianDiag[i]);

        // 1st order term: W * dX
        float32x4_t vFirstOrder = vmulq_f32(vW, vDX);

        // 2nd order term: 0.5 * H * (dX^2)
        float32x4_t vDXSq = vmulq_f32(vDX, vDX);
        float32x4_t vSecondOrder = vmulq_f32(vHalf, vmulq_f32(vH, vDXSq));

        // Total update = vFirstOrder + vSecondOrder
        float32x4_t vDelta = vaddq_f32(vFirstOrder, vSecondOrder);

        vst1q_f32(&outEvalDeltas[i], vDelta);
    }
#endif
    
    // Residual elements
    for (; i < count; ++i) {
        float dx = deltaX[i];
        outEvalDeltas[i] = featureWeights[i] * dx + 0.5f * hessianDiag[i] * dx * dx;
    }
}

} // namespace atshogi::simd
