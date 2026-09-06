#ifndef ATSHOGI_ORBIFOLD_MORSE_NEON_H
#define ATSHOGI_ORBIFOLD_MORSE_NEON_H

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif
#include <cstdint>
#include <cstddef>

namespace atshogi::simd {

constexpr size_t BOARD_SQUARES = 81; // 9x9 Shogi Board
constexpr size_t MORSE_BATCH_SIZE = 16;

/**
 * @brief Structure of Arrays (SoA) for Orbifold Morse Potential and Gradient Flow.
 */
struct OrbifoldMorseBatchSoA {
    alignas(16) float morse_val[MORSE_BATCH_SIZE];      // [in] f(x) Morse potential
    alignas(16) float grad_val[MORSE_BATCH_SIZE];       // [out] -∇f(x) or tauRepulsion
    alignas(16) int32_t critical_index[MORSE_BATCH_SIZE]; // [out] 0: Mate Attractor / Repulsion, 1: Normal
};

/**
 * @brief Evaluates Morse gradient flow with Baas-Sullivan dynamic repulsion to avoid repetition traps.
 * ARM NEON equivalent of project_and_evaluate_morse_avx512 using vbslq_f32 (Bitwise Select).
 */
void project_and_evaluate_morse_neon(OrbifoldMorseBatchSoA* batch, float tauRepulsion);

/**
 * @brief Computes discrete Morse gradient vectors over 81 board squares using ARM NEON (128-bit SIMD).
 */
void computeMorseGradientsNEON(const float* morseValues, float* outGradX, float* outGradY);

/**
 * @brief Normalizes 81 gradient vectors in-place using ARM NEON SIMD reciprocal square root.
 */
void normalizeGradientsNEON(float* gradX, float* gradY);

} // namespace atshogi::simd

#ifdef __cplusplus
extern "C" {
#endif

void atshogi_orbifold_morse_evaluate_neon(void* batchPtr, float tauRepulsion);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_ORBIFOLD_MORSE_NEON_H
