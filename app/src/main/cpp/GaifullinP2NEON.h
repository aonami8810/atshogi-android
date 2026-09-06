#ifndef ATSHOGI_GAIFULLIN_P2_NEON_H
#define ATSHOGI_GAIFULLIN_P2_NEON_H

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#include <cstdint>

namespace atshogi::simd {

constexpr int GAIFULLIN_LANES = 8;

struct GaifullinAuditResult {
    uint32_t laneStatuses; // Bitmask of 8 lanes: 0 = OK (Pass), 1 = Violation/Warning
    float laneP2Values[GAIFULLIN_LANES];
};

/**
 * @brief Audits Gaifullin P2 topological invariant difference between current and next states across 8 lanes.
 * ARM NEON equivalent of evaluateGaifullinP2AVX512.
 *
 * @param p2CurrentVector [in] Current state p2 (8 lanes)
 * @param p2NextVector    [in] Next state p2 (8 lanes)
 * @param outDeltaP2Vector [out] Delta δp2 = |p2(next) - p2(curr)|
 * @param outAnomalyMask  [out] Bitmask of anomalous/illegal move transitions (δp2 > 1e-9)
 */
void evaluateGaifullinP2NEON(
    const double* __restrict p2CurrentVector,
    const double* __restrict p2NextVector,
    double* __restrict outDeltaP2Vector,
    uint32_t* __restrict outAnomalyMask
);

/**
 * @brief Audits Gaifullin P2 topological invariant across 8 computational lanes using 2x float32x4 ARM NEON vectors.
 *
 * @param p2Metrics Input array of 8 P2 topological invariant metric values.
 * @param threshold Warning threshold for invariant violation.
 * @return GaifullinAuditResult Status bitmask and computed P2 metrics per lane.
 */
GaifullinAuditResult auditGaifullinP2NEON(const float* p2Metrics, float threshold);

} // namespace atshogi::simd

#ifdef __cplusplus
extern "C" {
#endif

void atshogi_gaifullin_p2_evaluate_neon(
    const double* p2CurrentVector,
    const double* p2NextVector,
    double* outDeltaP2Vector,
    uint32_t* outAnomalyMask
);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_GAIFULLIN_P2_NEON_H
