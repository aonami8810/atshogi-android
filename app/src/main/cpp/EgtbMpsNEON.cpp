#include "EgtbMpsNEON.h"

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <fstream>
#include <cstring>
#include <algorithm>
#include <array>

namespace atshogi::egtb {

bool MpsModel::loadFromBuffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 12) return false;

    // Header validation
    if (std::memcmp(buffer, "ATMP", 4) != 0) {
        return false;
    }

    uint32_t version = 0;
    uint32_t numCores = 0;
    std::memcpy(&version, buffer + 4, 4);
    std::memcpy(&numCores, buffer + 8, 4);

    if (version != 1 || numCores == 0) return false;

    size_t offset = 12;
    std::vector<MpsCore> newCores;
    newCores.reserve(numCores);

    for (uint32_t k = 0; k < numCores; ++k) {
        if (offset + 12 > size) return false;

        MpsCore core;
        std::memcpy(&core.r_in, buffer + offset, 4);
        std::memcpy(&core.d, buffer + offset + 4, 4);
        std::memcpy(&core.r_out, buffer + offset + 8, 4);
        offset += 12;

        size_t floatCount = static_cast<size_t>(core.d) * core.r_in * core.r_out;
        size_t byteCount = floatCount * sizeof(float);

        if (offset + byteCount > size) return false;

        core.data.resize(floatCount);
        std::memcpy(core.data.data(), buffer + offset, byteCount);
        offset += byteCount;

        newCores.push_back(std::move(core));
    }

    m_cores = std::move(newCores);
    return true;
}

bool MpsModel::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        return false;
    }

    return loadFromBuffer(buffer.data(), buffer.size());
}

float MpsModel::evaluate(const int32_t* indices, size_t numIndices) const {
    return evaluateMpsNEON(*this, indices, numIndices);
}

// ----------------------------------------------------------------------------
// ARM NEON & SIMD Vector-Matrix multiplication for MPS chain evaluation
// ----------------------------------------------------------------------------

static inline void multiplyVecMatNEON(
    const float* v_in,
    uint32_t r_in,
    const float* M_slice,
    uint32_t r_out,
    float* v_out
) {
    // Zero initialize output vector
    std::fill_n(v_out, r_out, 0.0f);

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    // Fast path: r_out is multiple of 4
    if (r_out == 4) {
        float32x4_t acc = vdupq_n_f32(0.0f);
        for (uint32_t i = 0; i < r_in; ++i) {
            float32x4_t vi = vdupq_n_f32(v_in[i]);
            float32x4_t row = vld1q_f32(&M_slice[i * 4]);
            acc = vfmaq_f32(acc, vi, row);
        }
        vst1q_f32(v_out, acc);
        return;
    } else if (r_out == 8) {
        float32x4_t acc0 = vdupq_n_f32(0.0f);
        float32x4_t acc1 = vdupq_n_f32(0.0f);
        for (uint32_t i = 0; i < r_in; ++i) {
            float32x4_t vi = vdupq_n_f32(v_in[i]);
            float32x4_t row0 = vld1q_f32(&M_slice[i * 8]);
            float32x4_t row1 = vld1q_f32(&M_slice[i * 8 + 4]);
            acc0 = vfmaq_f32(acc0, vi, row0);
            acc1 = vfmaq_f32(acc1, vi, row1);
        }
        vst1q_f32(v_out, acc0);
        vst1q_f32(v_out + 4, acc1);
        return;
    } else {
        // General NEON SIMD loop
        for (uint32_t i = 0; i < r_in; ++i) {
            float vi = v_in[i];
            float32x4_t v_vi = vdupq_n_f32(vi);
            const float* row = &M_slice[i * r_out];

            uint32_t j = 0;
            for (; j + 4 <= r_out; j += 4) {
                float32x4_t curr_out = vld1q_f32(&v_out[j]);
                float32x4_t row_vec = vld1q_f32(&row[j]);
                curr_out = vfmaq_f32(curr_out, v_vi, row_vec);
                vst1q_f32(&v_out[j], curr_out);
            }
            for (; j < r_out; ++j) {
                v_out[j] += vi * row[j];
            }
        }
        return;
    }
#else
    // Highly optimized scalar fallback for non-ARM hosts
    for (uint32_t i = 0; i < r_in; ++i) {
        float vi = v_in[i];
        const float* row = &M_slice[i * r_out];
        for (uint32_t j = 0; j < r_out; ++j) {
            v_out[j] += vi * row[j];
        }
    }
#endif
}

float evaluateMpsNEON(const MpsModel& model, const int32_t* indices, size_t numIndices) {
    const auto& cores = model.cores();
    if (cores.empty() || numIndices != cores.size()) {
        return 0.0f;
    }

    // Stack buffers for intermediate bond states (max bond dim chi <= 64 is ample for EGTB)
    constexpr size_t MAX_BOND_DIM = 64;
    alignas(16) float v_curr[MAX_BOND_DIM];
    alignas(16) float v_next[MAX_BOND_DIM];

    // Initial state: rank 0 (scalar 1.0)
    v_curr[0] = 1.0f;
    uint32_t curr_rank = 1;

    for (size_t k = 0; k < cores.size(); ++k) {
        const auto& core = cores[k];
        int32_t idx = indices[k];

        // Bounds check
        if (idx < 0 || static_cast<uint32_t>(idx) >= core.d) {
            return 0.0f;
        }

        uint32_t r_in = core.r_in;
        uint32_t r_out = core.r_out;
        const float* M_slice = core.getSlice(static_cast<uint32_t>(idx));

        if (r_out > MAX_BOND_DIM) {
            return 0.0f; // Exceeds stack buffer safety limit
        }

        multiplyVecMatNEON(v_curr, r_in, M_slice, r_out, v_next);

        // Copy forward
        std::copy_n(v_next, r_out, v_curr);
        curr_rank = r_out;
    }

    return v_curr[0];
}

float evaluateMps7SiteNEON(const MpsModel& model, const int32_t indices[7]) {
    const auto& cores = model.cores();
    if (cores.size() != 7) {
        return 0.0f;
    }

    constexpr size_t MAX_BOND_DIM = 16;
    alignas(16) float v0[MAX_BOND_DIM];
    alignas(16) float v1[MAX_BOND_DIM];

    v0[0] = 1.0f;

    // Site 1 (Sente King)
    const auto& c1 = cores[0];
    if (static_cast<uint32_t>(indices[0]) >= c1.d) return 0.0f;
    multiplyVecMatNEON(v0, c1.r_in, c1.getSlice(indices[0]), c1.r_out, v1);

    // Site 2 (Gote King)
    const auto& c2 = cores[1];
    if (static_cast<uint32_t>(indices[1]) >= c2.d) return 0.0f;
    multiplyVecMatNEON(v1, c2.r_in, c2.getSlice(indices[1]), c2.r_out, v0);

    // Site 3 (Attacker 1)
    const auto& c3 = cores[2];
    if (static_cast<uint32_t>(indices[2]) >= c3.d) return 0.0f;
    multiplyVecMatNEON(v0, c3.r_in, c3.getSlice(indices[2]), c3.r_out, v1);

    // Site 4 (Attacker 2)
    const auto& c4 = cores[3];
    if (static_cast<uint32_t>(indices[3]) >= c4.d) return 0.0f;
    multiplyVecMatNEON(v1, c4.r_in, c4.getSlice(indices[3]), c4.r_out, v0);

    // Site 5 (Defender 1)
    const auto& c5 = cores[4];
    if (static_cast<uint32_t>(indices[4]) >= c5.d) return 0.0f;
    multiplyVecMatNEON(v0, c5.r_in, c5.getSlice(indices[4]), c5.r_out, v1);

    // Site 6 (Defender 2)
    const auto& c6 = cores[5];
    if (static_cast<uint32_t>(indices[5]) >= c6.d) return 0.0f;
    multiplyVecMatNEON(v1, c6.r_in, c6.getSlice(indices[5]), c6.r_out, v0);

    // Site 7 (Hand / Turn)
    const auto& c7 = cores[6];
    if (static_cast<uint32_t>(indices[6]) >= c7.d) return 0.0f;
    multiplyVecMatNEON(v0, c7.r_in, c7.getSlice(indices[6]), c7.r_out, v1);

    return v1[0];
}

} // namespace atshogi::egtb

// ----------------------------------------------------------------------------
// C FFI Interface
// ----------------------------------------------------------------------------

extern "C" {

void* atshogi_mps_create_from_file(const char* filepath) {
    if (!filepath) return nullptr;
    auto* model = new atshogi::egtb::MpsModel();
    if (!model->loadFromFile(filepath)) {
        delete model;
        return nullptr;
    }
    return model;
}

void* atshogi_mps_create_from_buffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size == 0) return nullptr;
    auto* model = new atshogi::egtb::MpsModel();
    if (!model->loadFromBuffer(buffer, size)) {
        delete model;
        return nullptr;
    }
    return model;
}

void atshogi_mps_destroy(void* modelPtr) {
    if (modelPtr) {
        delete static_cast<atshogi::egtb::MpsModel*>(modelPtr);
    }
}

float atshogi_mps_evaluate(const void* modelPtr, const int32_t* indices, int32_t numIndices) {
    if (!modelPtr || !indices || numIndices <= 0) return 0.0f;
    const auto* model = static_cast<const atshogi::egtb::MpsModel*>(modelPtr);
    return model->evaluate(indices, static_cast<size_t>(numIndices));
}

float atshogi_mps_evaluate_7site(const void* modelPtr, const int32_t indices[7]) {
    if (!modelPtr || !indices) return 0.0f;
    const auto* model = static_cast<const atshogi::egtb::MpsModel*>(modelPtr);
    return atshogi::egtb::evaluateMps7SiteNEON(*model, indices);
}

} // extern "C"
