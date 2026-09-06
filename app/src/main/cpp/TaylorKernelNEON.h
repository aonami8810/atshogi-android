#ifndef ATSHOGI_TAYLOR_KERNEL_NEON_H
#define ATSHOGI_TAYLOR_KERNEL_NEON_H

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#include <cstddef>

namespace atshogi::simd {

/**
 * @brief Computes 2nd-order Taylor expansion evaluation updates across board features using ARM NEON 128-bit FMA (Fused Multiply-Add).
 *
 * @param featureWeights In: Feature weight array.
 * @param deltaX In: Perturbation vector delta X.
 * @param hessianDiag In: Diagonal elements of the Hessian matrix.
 * @param outEvalDeltas Out: Output evaluation updates array.
 * @param count Number of features (multiple of 4 recommended).
 */
void evaluateTaylorExpansionNEON(
    const float* featureWeights,
    const float* deltaX,
    const float* hessianDiag,
    float* outEvalDeltas,
    size_t count
);

} // namespace atshogi::simd

#endif // ATSHOGI_TAYLOR_KERNEL_NEON_H
