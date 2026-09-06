#ifndef OMNI_JOSEKI_TENSOR_HPP
#define OMNI_JOSEKI_TENSOR_HPP

#include <cstdint>
#include <vector>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// 531441 = 81 * 81 * 81
#define TENSOR_DIM 531441
#define TT_MAX_RANK 512

// TT-Core structure for MPS (Tensor Train / Matrix Product State)
struct TTCore {
    int rank_prev;
    int dim;
    int rank_next;
    float* data;
};

// Main generator structure
struct OmniJosekiGenerator {
    float* potential_manifold; // Full 81x81x81
    TTCore cores[3];
};

OmniJosekiGenerator* create_generator();
void destroy_generator(OmniJosekiGenerator* gen);

// 1. Distill Oracle (Suisho5/NNUE) evaluation into the 16-dimensional manifold 3rd-order tensor
void distill_oracle_evaluation(OmniJosekiGenerator* gen, const char* nnue_path);

// 2. Deterministic co-chain sweep (Witten complex backpropagation) in tropical semiring
void witten_complex_backpropagation(OmniJosekiGenerator* gen);

// 3. Orbifold (G_351 quotient space) geometric projection
void apply_orbifold_projection(OmniJosekiGenerator* gen);

// 4. TT-SVD to compress to ~207 KB static_joseki.bin
void compress_and_export_ttsvd(OmniJosekiGenerator* gen, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // OMNI_JOSEKI_TENSOR_HPP
