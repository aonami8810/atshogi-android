#ifndef ATSHOGI_DECAY_KERNEL_H
#define ATSHOGI_DECAY_KERNEL_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Merges static trunk tensor and dynamic punishment delta tensor with spatiotemporal double decay lambda.
 * T_total = T_static + (lambda_0 * gamma^n * (delta_p2^2 / (delta_p2^2 + epsilon))) * T_delta
 *
 * @param static_ptr  Pointer to static trunk tensor in memory / L3 cache.
 * @param delta_ptr   Pointer to dynamic deviation delta tensor.
 * @param out_ptr     Destination buffer for merged tensor.
 * @param lambda_0    Base initial punishment intensity (0.0 to 1.0).
 * @param gamma       Temporal geometric decay rate per move (e.g. 0.85).
 * @param n           Moves elapsed since deviating from trunk.
 * @param delta_p2    Topological deviation from Gaifullin invariant audit (delta p_2).
 * @param epsilon     Regularization parameter (e.g. 1e-4).
 * @param size        Number of tensor elements.
 */
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
);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_DECAY_KERNEL_H
