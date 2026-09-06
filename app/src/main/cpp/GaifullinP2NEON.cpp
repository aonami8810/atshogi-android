#include "GaifullinP2NEON.h"
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace atshogi::simd {

void evaluateGaifullinP2NEON(
    const double* __restrict p2CurrentVector,
    const double* __restrict p2NextVector,
    double* __restrict outDeltaP2Vector,
    uint32_t* __restrict outAnomalyMask
) {
    if (!p2CurrentVector || !p2NextVector || !outDeltaP2Vector || !outAnomalyMask) return;

#if defined(__aarch64__)
    // 64-bit ARMv8: float64x2_t を用いた 2 要素ごとの NEON ベクトル処理
    uint32_t mask = 0;
    const float64x2_t vThresh = vdupq_n_f64(1e-9);

    for (int i = 0; i < 8; i += 2) {
        float64x2_t v_curr = vld1q_f64(&p2CurrentVector[i]);
        float64x2_t v_next = vld1q_f64(&p2NextVector[i]);
        float64x2_t v_diff = vabsq_f64(vsubq_f64(v_next, v_curr));
        vst1q_f64(&outDeltaP2Vector[i], v_diff);

        uint64x2_t v_cmp = vcgtq_f64(v_diff, vThresh);
        uint64_t cmp_lanes[2];
        vst1q_u64(cmp_lanes, v_cmp);

        if (cmp_lanes[0] != 0) mask |= (1U << i);
        if (cmp_lanes[1] != 0) mask |= (1U << (i + 1));
    }
    *outAnomalyMask = mask;
#else
    // ポータブルスカラフォールバック
    uint32_t mask = 0;
    for (int i = 0; i < 8; ++i) {
        double diff = p2NextVector[i] - p2CurrentVector[i];
        if (diff < 0) diff = -diff;
        outDeltaP2Vector[i] = diff;
        if (diff > 1e-9) {
            mask |= (1U << i); // 異常手をビットマスクへ隔離
        }
    }
    *outAnomalyMask = mask;
#endif
}

GaifullinAuditResult auditGaifullinP2NEON(const float* p2Metrics, float threshold) {
    GaifullinAuditResult result{};
    result.laneStatuses = 0;

#if defined(__ARM_NEON) || defined(__aarch64__)
    // Load 8 float values into two 128-bit NEON registers (4 floats each)
    float32x4_t vLanes03 = vld1q_f32(&p2Metrics[0]);
    float32x4_t vLanes47 = vld1q_f32(&p2Metrics[4]);

    const float32x4_t vThresh = vdupq_n_f32(threshold);

    // Compare metrics against threshold (vcgtq_f32: Vector Greater Than)
    uint32x4_t vComp03 = vcgtq_f32(vLanes03, vThresh);
    uint32x4_t vComp47 = vcgtq_f32(vLanes47, vThresh);

    // Extract mask bits
    uint32_t mask03[4];
    uint32_t mask47[4];
    vst1q_u32(mask03, vComp03);
    vst1q_u32(mask47, vComp47);

    vst1q_f32(&result.laneP2Values[0], vLanes03);
    vst1q_f32(&result.laneP2Values[4], vLanes47);

    for (int i = 0; i < 4; ++i) {
        if (mask03[i] != 0) {
            result.laneStatuses |= (1U << i);
        }
        if (mask47[i] != 0) {
            result.laneStatuses |= (1U << (i + 4));
        }
    }
#else
    for (int i = 0; i < 8; ++i) {
        result.laneP2Values[i] = p2Metrics[i];
        if (p2Metrics[i] > threshold) {
            result.laneStatuses |= (1U << i);
        }
    }
#endif

    return result;
}

} // namespace atshogi::simd

extern "C" {

uint32_t atshogi_gaifullin_p2_audit(const float* p2Metrics, float threshold) {
    return atshogi::simd::auditGaifullinP2NEON(p2Metrics, threshold).laneStatuses;
}

void atshogi_gaifullin_p2_evaluate_neon(
    const double* p2CurrentVector,
    const double* p2NextVector,
    double* outDeltaP2Vector,
    uint32_t* outAnomalyMask
) {
    atshogi::simd::evaluateGaifullinP2NEON(p2CurrentVector, p2NextVector, outDeltaP2Vector, outAnomalyMask);
}

}

