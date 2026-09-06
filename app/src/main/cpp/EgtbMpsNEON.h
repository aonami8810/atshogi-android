#ifndef ATSHOGI_EGTB_MPS_NEON_H
#define ATSHOGI_EGTB_MPS_NEON_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace atshogi::egtb {

/**
 * @brief Represents a single Matrix Product State (MPS) core tensor.
 * Stored in [d, r_in, r_out] layout for contiguous O(1) slice lookup.
 */
struct MpsCore {
    uint32_t r_in{1};   // Input bond dimension (rank)
    uint32_t d{1};      // Physical index dimension (e.g. 81 for board squares)
    uint32_t r_out{1};  // Output bond dimension (rank)
    std::vector<float> data; // Flat array: size = d * r_in * r_out

    /**
     * @brief Gets pointer to the slice matrix M of shape (r_in, r_out) corresponding to physical index idx.
     */
    inline const float* getSlice(uint32_t idx) const {
        return &data[idx * (r_in * r_out)];
    }
};

/**
 * @brief Complete MPS tensor network container representing compressed EGTB / Atlas.
 */
class MpsModel {
public:
    MpsModel() = default;

    /**
     * @brief Loads model from raw byte buffer (.atmp binary format).
     */
    bool loadFromBuffer(const uint8_t* buffer, size_t size);

    /**
     * @brief Loads model from binary file.
     */
    bool loadFromFile(const std::string& filepath);

    /**
     * @brief Returns whether the model is loaded and ready.
     */
    bool isValid() const { return !m_cores.empty(); }

    /**
     * @brief Number of physical sites (dimension N).
     */
    size_t numSites() const { return m_cores.size(); }

    /**
     * @brief Access cores directly.
     */
    const std::vector<MpsCore>& cores() const { return m_cores; }

    /**
     * @brief Evaluates potential/value for a given multi-index state using ARM NEON acceleration.
     * Complexity: O(N * chi^2)
     */
    float evaluate(const int32_t* indices, size_t numIndices) const;

private:
    std::vector<MpsCore> m_cores;
};

/**
 * @brief Evaluates MPS chain using 128-bit ARM NEON SIMD vector-matrix multiplications.
 * Fallback to optimized scalar loops on non-ARM architectures.
 *
 * @param model Loaded MPS model.
 * @param indices Array of physical indices of length N.
 * @param numIndices Length of indices array.
 * @return float Evaluated scalar Morse potential or DTM value.
 */
float evaluateMpsNEON(const MpsModel& model, const int32_t* indices, size_t numIndices);

/**
 * @brief Ultra-fast specialized O(k=7) 7-site unrolled evaluation for EGTBL 7.0 (7-piece tablebase).
 * Fully utilizes NEON registers with zero heap allocation.
 */
float evaluateMps7SiteNEON(const MpsModel& model, const int32_t indices[7]);

} // namespace atshogi::egtb

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief C/FFI interface for Haskell / Java JNI bindings.
 */
void* atshogi_mps_create_from_file(const char* filepath);
void* atshogi_mps_create_from_buffer(const uint8_t* buffer, size_t size);
void atshogi_mps_destroy(void* modelPtr);
float atshogi_mps_evaluate(const void* modelPtr, const int32_t* indices, int32_t numIndices);
float atshogi_mps_evaluate_7site(const void* modelPtr, const int32_t indices[7]);

#ifdef __cplusplus
}
#endif

#endif // ATSHOGI_EGTB_MPS_NEON_H
