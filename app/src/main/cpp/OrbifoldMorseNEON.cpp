#include "OrbifoldMorseNEON.h"
#include <cmath>
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif

namespace atshogi::simd {

void project_and_evaluate_morse_neon(OrbifoldMorseBatchSoA* batch, float tauRepulsion) {
    if (!batch) return;

#if defined(__ARM_NEON) || defined(__aarch64__)
    // 16局面分を 4 つの 128-bit NEON ベクトル (float32x4_t) で並列処理
    const float32x4_t vFactor = vdupq_n_f32(-1.05f);
    const float32x4_t vTau = vdupq_n_f32(tauRepulsion);
    const float32x4_t vThreshold = vdupq_n_f32(-9000.0f);
    const float32x4_t vMateThreshold = vdupq_n_f32(-5000.0f);
    const int32x4_t vOne = vdupq_n_s32(1);
    const int32x4_t vZero = vdupq_n_s32(0);

    for (int i = 0; i < 16; i += 4) {
        // 1. 4局面分の現在のモース評価値 f(x) をロード
        float32x4_t vMorse = vld1q_f32(&batch->morse_val[i]);

        // 2. 通常時の最急降下勾配 -∇f(x) を算出
        float32x4_t vGrad = vmulq_f32(vMorse, vFactor);

        // 3. 【千日手検出】モースポテンシャルが「臨界崖」の境界値（-9000.0f）を下回ったレーンを検知
        uint32x4_t kSingular = vcleq_f32(vMorse, vThreshold);

        // 4. 【マスク制御ブレンド (vbslq_f32)】千日手ループ特異点に対し、勾配を強制反転させる「斥力ポテンシャル (vTau)」を上書き
        float32x4_t vFinalGrad = vbslq_f32(kSingular, vTau, vGrad);

        // 5. 【詰み判定】算出された最終勾配が「詰み境界 (-5000.0f)」をクリアしているかを判定
        uint32x4_t kMate = vcltq_f32(vFinalGrad, vMateThreshold);

        // 結果をSoAへ一括格納
        vst1q_f32(&batch->grad_val[i], vFinalGrad);

        // 詰みレーン（kMate）には Critical Index 0（Mate Attractor）をマッピング
        int32x4_t vIndices = vbslq_s32(kMate, vZero, vOne);
        vst1q_s32(&batch->critical_index[i], vIndices);
    }
#elif defined(__AVX512F__)
    const __m512 vFactor = _mm512_set1_ps(-1.05f);
    const __m512 vTau = _mm512_set1_ps(tauRepulsion);
    const __m512 vThreshold = _mm512_set1_ps(-9000.0f);
    const __m512 vMateThreshold = _mm512_set1_ps(-5000.0f);
    const __m512i vOne = _mm512_set1_epi32(1);
    const __m512i vZero = _mm512_setzero_si512();

    __m512 vMorse = _mm512_loadu_ps(&batch->morse_val[0]);
    __m512 vGrad = _mm512_mul_ps(vMorse, vFactor);
    __mmask16 kSingular = _mm512_cmp_ps_mask(vMorse, vThreshold, _CMP_LE_OQ);
    __m512 vFinalGrad = _mm512_mask_blend_ps(kSingular, vGrad, vTau);
    __mmask16 kMate = _mm512_cmp_ps_mask(vFinalGrad, vMateThreshold, _CMP_LT_OQ);
    _mm512_storeu_ps(&batch->grad_val[0], vFinalGrad);
    __m512i vIndices = _mm512_mask_blend_epi32(kMate, vOne, vZero);
    _mm512_storeu_si512((__m512i*)&batch->critical_index[0], vIndices);

#elif defined(__AVX2__)
    const __m256 vFactor = _mm256_set1_ps(-1.05f);
    const __m256 vTau = _mm256_set1_ps(tauRepulsion);
    const __m256 vThreshold = _mm256_set1_ps(-9000.0f);
    const __m256 vMateThreshold = _mm256_set1_ps(-5000.0f);
    const __m256i vOne = _mm256_set1_epi32(1);
    const __m256i vZero = _mm256_setzero_si256();

    for (int i = 0; i < 16; i += 8) {
        __m256 vMorse = _mm256_loadu_ps(&batch->morse_val[i]);
        __m256 vGrad = _mm256_mul_ps(vMorse, vFactor);
        __m256 vSingularMask = _mm256_cmp_ps(vMorse, vThreshold, _CMP_LE_OQ);
        __m256 vFinalGrad = _mm256_blendv_ps(vGrad, vTau, vSingularMask);
        __m256 vMateMask = _mm256_cmp_ps(vFinalGrad, vMateThreshold, _CMP_LT_OQ);
        _mm256_storeu_ps(&batch->grad_val[i], vFinalGrad);
        __m256i vIndices = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(vOne), _mm256_castsi256_ps(vZero), vMateMask));
        _mm256_storeu_si256((__m256i*)&batch->critical_index[i], vIndices);
    }
#else
    // ポータブルスカラフォールバック
    for (int i = 0; i < 16; ++i) {
        if (batch->morse_val[i] <= -9000.0f) {
            batch->grad_val[i] = tauRepulsion; // 斥力による強制引き戻し
            batch->critical_index[i] = 0;
        } else {
            batch->grad_val[i] = -(batch->morse_val[i] * 1.05f);
            if (batch->grad_val[i] < -5000.0f) {
                batch->critical_index[i] = 0; // 詰みアトラクター
            } else {
                batch->critical_index[i] = 1; // 通常局所領域
            }
        }
    }
#endif
}

void computeMorseGradientsNEON(const float* morseValues, float* outGradX, float* outGradY) {
    // Shogi Board grid: 9 ranks x 9 files = 81 squares
    // Central finite differences for interior squares using 4-element SIMD vectors
    
#if defined(__ARM_NEON) || defined(__aarch64__)
    // Process 4 squares at a time using ARM NEON float32x4_t
    size_t i = 0;
    const size_t simdLimit = BOARD_SQUARES - (BOARD_SQUARES % 4);

    const float32x4_t vHalf = vdupq_n_f32(0.5f);

    for (; i < simdLimit; i += 4) {
        // Calculate 2D grid coordinates for 4 linear indices
        // sq0..sq3: rank = idx / 9, file = idx % 9
        float gradX_tmp[4];
        float gradY_tmp[4];

        for (int k = 0; k < 4; ++k) {
            size_t idx = i + k;
            int rank = static_cast<int>(idx / 9);
            int file = static_cast<int>(idx % 9);

            // X-gradient (file dimension): right (file+1) - left (file-1)
            float rightVal = (file < 8) ? morseValues[idx + 1] : morseValues[idx];
            float leftVal  = (file > 0) ? morseValues[idx - 1] : morseValues[idx];
            gradX_tmp[k] = rightVal - leftVal;

            // Y-gradient (rank dimension): down (rank+1) - up (rank-1)
            float downVal = (rank < 8) ? morseValues[idx + 9] : morseValues[idx];
            float upVal   = (rank > 0) ? morseValues[idx - 9] : morseValues[idx];
            gradY_tmp[k] = downVal - upVal;
        }

        float32x4_t vGradX = vld1q_f32(gradX_tmp);
        float32x4_t vGradY = vld1q_f32(gradY_tmp);

        // Apply 0.5 finite difference scaling using NEON SIMD
        vGradX = vmulq_f32(vGradX, vHalf);
        vGradY = vmulq_f32(vGradY, vHalf);

        vst1q_f32(&outGradX[i], vGradX);
        vst1q_f32(&outGradY[i], vGradY);
    }
#else
    size_t i = 0;
#endif

    // Scalar tail loop for remaining elements
    for (; i < BOARD_SQUARES; ++i) {
        int rank = static_cast<int>(i / 9);
        int file = static_cast<int>(i % 9);

        float rightVal = (file < 8) ? morseValues[i + 1] : morseValues[i];
        float leftVal  = (file > 0) ? morseValues[i - 1] : morseValues[i];
        outGradX[i] = 0.5f * (rightVal - leftVal);

        float downVal = (rank < 8) ? morseValues[i + 9] : morseValues[i];
        float upVal   = (rank > 0) ? morseValues[i - 9] : morseValues[i];
        outGradY[i] = 0.5f * (downVal - upVal);
    }
}

void normalizeGradientsNEON(float* gradX, float* gradY) {
    size_t i = 0;
#if defined(__ARM_NEON) || defined(__aarch64__)
    const size_t simdLimit = BOARD_SQUARES - (BOARD_SQUARES % 4);
    const float32x4_t vEps = vdupq_n_f32(1e-6f);

    for (; i < simdLimit; i += 4) {
        float32x4_t vX = vld1q_f32(&gradX[i]);
        float32x4_t vY = vld1q_f32(&gradY[i]);

        // magSq = vX^2 + vY^2
        float32x4_t vMagSq = vmlaq_f32(vmulq_f32(vX, vX), vY, vY);
        vMagSq = vaddq_f32(vMagSq, vEps);

        // ARM NEON reciprocal square root estimate + Newton-Raphson refinement
        float32x4_t vInvMagEst = vrsqrteq_f32(vMagSq);
        float32x4_t vStep = vrsqrtsq_f32(vmulq_f32(vInvMagEst, vInvMagEst), vMagSq);
        float32x4_t vInvMag = vmulq_f32(vInvMagEst, vStep);

        vX = vmulq_f32(vX, vInvMag);
        vY = vmulq_f32(vY, vInvMag);

        vst1q_f32(&gradX[i], vX);
        vst1q_f32(&gradY[i], vY);
    }
#endif

    for (; i < BOARD_SQUARES; ++i) {
        float magSq = gradX[i] * gradX[i] + gradY[i] * gradY[i] + 1e-6f;
        float invMag = 1.0f / std::sqrt(magSq);
        gradX[i] *= invMag;
        gradY[i] *= invMag;
    }
}

} // namespace atshogi::simd

extern "C" {

void atshogi_orbifold_morse_evaluate_neon(void* batchPtr, float tauRepulsion) {
    if (!batchPtr) return;
    atshogi::simd::project_and_evaluate_morse_neon(
        static_cast<atshogi::simd::OrbifoldMorseBatchSoA*>(batchPtr),
        tauRepulsion
    );
}

}
