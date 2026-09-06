#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif
#include <stdint.h>
#include <stddef.h>

extern "C" {

void analyze_persistent_king_safety_neon(
    const float* defense_potentials,
    int king_index,
    float* out_birth,
    float* out_death
) {
    int neighbors[8] = {
        king_index - 10, king_index - 9, king_index - 8,
        king_index - 1,                  king_index + 1,
        king_index + 8,  king_index + 9, king_index + 10
    };

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    float32_t local_pots[8];
    for (int i = 0; i < 8; ++i) {
        int n_idx = neighbors[i];
        local_pots[i] = (n_idx >= 0 && n_idx < 81) ? defense_potentials[n_idx] : 0.0f;
    }
    float32x4_t v_pots_low = vld1q_f32(&local_pots[0]);
    float32x4_t v_pots_high = vld1q_f32(&local_pots[4]);
    float32x4_t v_max = vmaxq_f32(v_pots_low, v_pots_high);
    float32_t birth = vmaxvq_f32(v_max);
    float32x4_t v_min = vminq_f32(v_pots_low, v_pots_high);
    float32_t death = vminvq_f32(v_min);
    *out_birth = birth;
    *out_death = death;
#else
    float birth = -1e9f;
    float death = 1e9f;
    for (int i = 0; i < 8; ++i) {
        int n_idx = neighbors[i];
        float val = (n_idx >= 0 && n_idx < 81) ? defense_potentials[n_idx] : 0.0f;
        if (val > birth) birth = val;
        if (val < death) death = val;
    }
    *out_birth = birth;
    *out_death = death;
#endif
}

}
