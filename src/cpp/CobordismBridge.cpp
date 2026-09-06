#include <cmath>

extern "C" {
    float compute_cobordism_weight(int k) {
        // w(k) = 1 / (1 + exp(1.5 * (k - 7)))
        float alpha = 1.5f;
        float k_star = 7.0f;
        return 1.0f / (1.0f + std::exp(alpha * (k - k_star)));
    }
}
