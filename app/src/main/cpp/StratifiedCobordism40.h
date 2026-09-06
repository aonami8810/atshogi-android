#ifndef ATSHOGI_STRATIFIED_COBORDISM40_H
#define ATSHOGI_STRATIFIED_COBORDISM40_H

#include <cstdint>

namespace atshogi::cobordism {

/**
 * @brief Evaluates 5th-order Hermite smooth transition polynomial: S(t) = 6t^5 - 15t^4 + 10t^3
 * Ensures exact C^2 continuity with zero 1st and 2nd derivatives at boundary points.
 */
inline float hermiteSmooth5th(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return (6.0f * t * t * t * t * t) - (15.0f * t * t * t * t) + (10.0f * t * t * t);
}

/**
 * @brief Blends 4 stratified manifold layers across piece count k in [0, 40].
 * - Layer 0 (k <= 7):   Terminal EGTB Strong Solution
 * - Layer 1 (8 <= k <= 14): Cobordism Bridge
 * - Layer 2 (15 <= k <= 28): Tactical Middlegame (Morse Cancellation)
 * - Layer 3 (29 <= k <= 40): Grand Opening (Global Homotopy Manifold)
 */
float evaluateStratifiedCobordism40(float vEnd, float vTrans, float vMid, float vOpen, int k);

} // namespace atshogi::cobordism

#ifdef __cplusplus
extern "C" {
#endif

float atshogi_stratified_cobordism40_eval(float vEnd, float vTrans, float vMid, float vOpen, int k);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_STRATIFIED_COBORDISM40_H
