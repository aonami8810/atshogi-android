#include "tensor_generator.hpp"
#include <iostream>

int main() {
    std::cout << "Initializing Omni-Joseki Engine (C++ Entry Point)" << std::endl;
    OmniJosekiGenerator* gen = create_generator();
    
    distill_oracle_evaluation(gen, "suisho5.nnue");
    witten_complex_backpropagation(gen);
    apply_orbifold_projection(gen);
    compress_and_export_ttsvd(gen, "static_joseki.bin");
    
    destroy_generator(gen);
    return 0;
}
