#include <stdint.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_NEON 1
#endif

extern "C" {

void evaluate_charged_omni_tensor_neon(
    const float* potentials,
    const float* topological_charges,
    float* out_gradients
) {
    for (int i = 0; i < 80; i += 4) {
#if defined(HAS_NEON)
        float32x4_t v_charges = vld1q_f32(&topological_charges[i]);
        float32_t pots_u[4], pots_d[4], pots_l[4], pots_r[4];

        for (int l = 0; l < 4; ++l) {
            int idx = i + l;
            int u = (idx >= 9) ? idx - 9 : idx;
            int d = (idx < 72) ? idx + 9 : idx;
            int l_idx = (idx % 9 > 0) ? idx - 1 : idx;
            int r = (idx % 9 < 8) ? idx + 1 : idx;

            pots_u[l] = potentials[u];
            pots_d[l] = potentials[d];
            pots_l[l] = potentials[l_idx];
            pots_r[l] = potentials[r];
        }

        float32x4_t v_u = vld1q_f32(pots_u);
        float32x4_t v_d = vld1q_f32(pots_d);
        float32x4_t v_l = vld1q_f32(pots_l);
        float32x4_t v_r = vld1q_f32(pots_r);
        float32x4_t v_curr = vld1q_f32(&potentials[i]);

        float32x4_t v_grad = vsubq_f32(v_u, v_curr);
        v_grad = vaddq_f32(v_grad, vsubq_f32(v_d, v_curr));
        v_grad = vaddq_f32(v_grad, vsubq_f32(v_l, v_curr));
        v_grad = vaddq_f32(v_grad, vsubq_f32(v_r, v_curr));

        float32x4_t v_force = vmulq_f32(v_grad, v_charges);
        vst1q_f32(&out_gradients[i], v_force);
#else
        for (int l = 0; l < 4; ++l) {
            int idx = i + l;
            int u = (idx >= 9) ? idx - 9 : idx;
            int d = (idx < 72) ? idx + 9 : idx;
            int l_idx = (idx % 9 > 0) ? idx - 1 : idx;
            int r = (idx % 9 < 8) ? idx + 1 : idx;

            float grad = (potentials[u] - potentials[idx]) +
                         (potentials[d] - potentials[idx]) +
                         (potentials[l_idx] - potentials[idx]) +
                         (potentials[r] - potentials[idx]);
            out_gradients[idx] = topological_charges[idx] * grad;
        }
#endif
    }

    int idx = 80;
    int u = (idx >= 9) ? idx - 9 : idx;
    int d = (idx < 72) ? idx + 9 : idx;
    int l_idx = (idx % 9 > 0) ? idx - 1 : idx;
    int r = (idx % 9 < 8) ? idx + 1 : idx;

    float grad = (potentials[u] - potentials[80]) +
                 (potentials[d] - potentials[80]) +
                 (potentials[l_idx] - potentials[80]) +
                 (potentials[r] - potentials[80]);

    out_gradients[80] = topological_charges[80] * grad;
}

}
