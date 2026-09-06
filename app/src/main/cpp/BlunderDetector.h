#ifndef ATSHOGI_BLUNDER_DETECTOR_H
#define ATSHOGI_BLUNDER_DETECTOR_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculates blunder penalty initial intensity lambda_0 from squared gradient residual norm.
 * D^2 = sum_{i=0}^{80} (g_actual[i] - g_expected[i])^2
 * lambda_0 = lambda_max * (D^2 / (D^2 + theta_sq))
 *
 * @param g_expected Expected best-move gradient vector (float[81]).
 * @param g_actual   Actual opponent move gradient vector (float[81]).
 * @param theta_sq   Half-saturation threshold squared (theta^2).
 * @param lambda_max Maximum penalty intensity (typically 1.0).
 * @return float Calculated initial punishment intensity lambda_0.
 */
float calculate_blunder_lambda0(
    const float* __restrict g_expected,
    const float* __restrict g_actual,
    float theta_sq,
    float lambda_max
);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_BLUNDER_DETECTOR_H
