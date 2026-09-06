// atshogi-laplace-conformal-dynamic.cpp
#include <stdint.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#endif

extern "C" {

void solve_laplace_iteration_step_dynamic(
    float* potentials,
    const uint8_t* obstacle_mask,
    int sente_king_idx,
    int gote_king_idx,
    float mass_term_m2
) {
    float next_potentials[81];
    
    potentials[sente_king_idx] = 1000.0f;
    potentials[gote_king_idx]  = -1000.0f;

    for (int i = 0; i < 81; ++i) {
        if (i == sente_king_idx || i == gote_king_idx) {
            next_potentials[i] = potentials[i];
            continue;
        }

        if (obstacle_mask[i]) {
            next_potentials[i] = potentials[i] * 0.1f;
            continue;
        }

        int n_u = (i >= 9) ? i - 9 : i;
        int n_d = (i < 72) ? i + 9 : i;
        int n_l = (i % 9 > 0) ? i - 1 : i;
        int n_r = (i % 9 < 8) ? i + 1 : i;

#if defined(HAS_NEON)
        float32_t neigh_vals[4] = { potentials[n_u], potentials[n_d], potentials[n_l], potentials[n_r] };
        float32x4_t v_neigh = vld1q_f32(neigh_vals);
        float sum = vgetq_lane_f32(v_neigh, 0) + 
                    vgetq_lane_f32(v_neigh, 1) + 
                    vgetq_lane_f32(v_neigh, 2) + 
                    vgetq_lane_f32(v_neigh, 3);
#else
        float sum = potentials[n_u] + potentials[n_d] + potentials[n_l] + potentials[n_r];
#endif

        next_potentials[i] = (sum * 0.25f) / (1.0f + mass_term_m2);
    }

    for (int i = 0; i < 81; ++i) {
        potentials[i] = next_potentials[i];
    }
}

}
