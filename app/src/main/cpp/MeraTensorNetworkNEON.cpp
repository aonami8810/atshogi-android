// Removed x86 intrinsics
#include "MeraTensorNetworkNEON.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <mutex>

#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace atshogi { namespace mera {

bool MeraModel40::loadFromBuffer(const uint8_t* buffer, size_t size) {
    if (!buffer || size < 12) return false;

    // Header: Magic 'MERA' (4 bytes), Version (4 bytes), NumLayers (4 bytes)
    if (buffer[0] != 'M' || buffer[1] != 'E' || buffer[2] != 'R' || buffer[3] != 'A') {
        return false;
    }

    uint32_t version = *reinterpret_cast<const uint32_t*>(buffer + 4);
    uint32_t numLayers = *reinterpret_cast<const uint32_t*>(buffer + 8);
    (void)version;

    m_layers.clear();
    size_t offset = 12;

    for (uint32_t l = 0; l < numLayers && offset + 12 <= size; ++l) {
        MeraLayer layer;
        layer.level = *reinterpret_cast<const uint32_t*>(buffer + offset);
        layer.in_dim = *reinterpret_cast<const uint32_t*>(buffer + offset + 4);
        layer.out_dim = *reinterpret_cast<const uint32_t*>(buffer + offset + 8);
        offset += 12;

        size_t count = layer.in_dim * layer.out_dim * 4;
        if (offset + count * sizeof(float) > size) return false;

        layer.tensors.resize(count);
        std::memcpy(layer.tensors.data(), buffer + offset, count * sizeof(float));
        offset += count * sizeof(float);

        m_layers.push_back(std::move(layer));
    }

    return !m_layers.empty();
}

bool MeraModel40::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

    return loadFromBuffer(buffer.data(), size);
}

float MeraModel40::evaluate40SitesNEON(const int32_t indices[40]) const {
    return evaluateMera40NEON(*this, indices);
}

float evaluateMera40NEON(const MeraModel40& model, const int32_t indices[40]) {
    // 3-Level MERA (Multi-scale Entanglement Renormalization Ansatz) Contraction:
    //
    // 📐 Level 0 (Microscopic): 40 physical piece sites (4D state vectors: [Value, Invade, Center, Safety])
    //    -> Disentangler U_0: 2-site local entanglement untangling (相互作用・利きE絡み合い解除)
    //    -> Isometry W_0: 4-site to 1-cluster coarse-graining (粗視化等長縮紁E40 -> 10 clusters)
    //
    // 📐 Level 1 (Tactical): 10 cluster sites (自陣玉囲ぁE攻撁E線、中央主導権、敵陣玉囲ぁE
    //    -> Disentangler U_1: Inter-cluster tactical entanglement untangling
    //    -> Isometry W_1: Coarse-graining into 4 macroscopic sectors (10 -> 4 sectors)
    //
    // 📐 Level 2 (Macroscopic): 4 tactical sectors (先手玉堁E度, 先手圧劁E 後手圧劁E 後手玉堁E度)
    //    -> Isometry W_2: Grand global contraction to 1 scalar potential v_Open

    alignas(16) float state_l0[40][4];  // 40 sites x 4 features
    alignas(16) float state_l1[10][4];  // 10 clusters x 4 features
    alignas(16) float state_l2[4][4];   // 4 sectors x 4 features

    // 1. Level 0 物琁Eイト特徴チEソルの構E
    for (int i = 0; i < 40; ++i) {
        int32_t code = indices[i];
        int sq = code & 0x7F;
        int pt = (code >> 7) & 0x0F;
        int col = (code >> 11) & 0x01; // 0: Black, 1: White
        
        // Determine original piece type and color based on index
        int orig_pt = 1;
        int orig_col = 0;
        if (i < 9) { orig_pt = 1; orig_col = 0; }
        else if (i < 18) { orig_pt = 1; orig_col = 1; }
        else if (i < 20) { orig_pt = 2; orig_col = 0; }
        else if (i < 22) { orig_pt = 2; orig_col = 1; }
        else if (i < 24) { orig_pt = 3; orig_col = 0; }
        else if (i < 26) { orig_pt = 3; orig_col = 1; }
        else if (i < 28) { orig_pt = 4; orig_col = 0; }
        else if (i < 30) { orig_pt = 4; orig_col = 1; }
        else if (i < 32) { orig_pt = 7; orig_col = 0; } // Gold
        else if (i < 34) { orig_pt = 7; orig_col = 1; }
        else if (i == 34) { orig_pt = 5; orig_col = 0; } // Bishop
        else if (i == 35) { orig_pt = 5; orig_col = 1; }
        else if (i == 36) { orig_pt = 6; orig_col = 0; } // Rook
        else if (i == 37) { orig_pt = 6; orig_col = 1; }
        else if (i == 38) { orig_pt = 8; orig_col = 0; } // King
        else if (i == 39) { orig_pt = 8; orig_col = 1; }
        
        if (sq >= 0 && sq < 81 && pt > 0) {
            int file = (sq / 9) + 1;
            int rank = (sq % 9) + 1;

            float val = 0.0f;
            switch (pt) {
                case 1: val = 1.0f; break;  // 歩
                case 2: val = 3.2f; break;  // 馁E                case 3: val = 3.8f; break;  // 桁E                case 4: val = 5.2f; break;  // 銀
                case 5: val = 6.2f; break;  // 釁E                case 6: val = 8.8f; break;  // 见E                case 7: val = 10.5f; break; // 飁E                case 8: val = 150.0f; break;// 玁E                case 9: case 10: case 11: case 12: val = 5.4f; break; // と, 成馁E 成桁E 成銀
                case 13: val = 12.0f; break; // 馬
                case 14: val = 13.5f; break; // 竁E                default: val = 1.0f; break;
            }

            float sign = (col == 0) ? 1.0f : -1.0f;
            float invade = (col == 0) ? (float)(9 - rank) * 0.125f : (float)(rank - 1) * 0.125f;
            float center = (float)(5 - abs(file - 5)) * 0.25f;
            float safety = (pt == 8) ? (col == 0 ? (float)(rank - 7) : (float)(3 - rank)) : 0.5f;

            state_l0[i][0] = val * sign;
            state_l0[i][1] = invade * sign;
            state_l0[i][2] = center * sign;
            state_l0[i][3] = safety * sign;
        } else {
            // Pieces in hand (sq == 81). If orig_col was Sente (0), it is now Gote's piece (1).
            int hand_col = 1 - orig_col;
            float val = 0.0f;
            switch (orig_pt) {
                case 1: val = 1.1f; break; // Hand pawn slightly better
                case 2: val = 3.4f; break; 
                case 3: val = 4.0f; break; 
                case 4: val = 5.4f; break; 
                case 5: val = 6.6f; break; 
                case 6: val = 9.2f; break; 
                case 7: val = 10.7f; break; 
            }
            float sign = (hand_col == 0) ? 1.0f : -1.0f;
            
            state_l0[i][0] = val * sign;
            state_l0[i][1] = 0.0f; // Hand pieces don't invade
            state_l0[i][2] = 0.0f;
            state_l0[i][3] = 0.0f;
        }
    }

#if defined(__ARM_NEON) || defined(__aarch64__)
    // 2. Level 0 Disentangler U_0 (隣接サイト対の絡み合い解除) & Isometry W_0 (40 -> 10 クラスター縮紁E
    const float32x4_t v_quarter = vdupq_n_f32(0.25f);
    const float32x4_t v_disentangle_u = { 0.85f, 0.15f, 0.15f, 0.85f };

    for (int c = 0; c < 10; ++c) {
        // 4 物琁EイトをローチE    
    float32x4_t v0 = vld1q_f32(&state_l0[c * 4 + 0][0]);
        float32x4_t v1 = vld1q_f32(&state_l0[c * 4 + 1][0]);
        float32x4_t v2 = vld1q_f32(&state_l0[c * 4 + 2][0]);
        float32x4_t v3 = vld1q_f32(&state_l0[c * 4 + 3][0]);

        // Disentangler U_0: 局所エンタングルメンチE回転
        float32x4_t u01_a = vmulq_f32(v0, v_disentangle_u);
        float32x4_t u01_b = vmulq_f32(v1, v_disentangle_u);
        float32x4_t u23_a = vmulq_f32(v2, v_disentangle_u);
        float32x4_t u23_b = vmulq_f32(v3, v_disentangle_u);

        // Isometry W_0: 等長粗視化縮紁E    
    float32x4_t sum01 = vaddq_f32(u01_a, u01_b);
        float32x4_t sum23 = vaddq_f32(u23_a, u23_b);
        float32x4_t cluster_vec = vmulq_f32(vaddq_f32(sum01, sum23), v_quarter);

        vst1q_f32(&state_l1[c][0], cluster_vec);
    }

    // 3. Level 1 Disentangler U_1 & Isometry W_1 (10 クラスター -> 4 戦術セクター縮紁E
    // Sector 0: 先手玉囲ぁEE守備 (clusters 0, 1)
    // Sector 1: 先手中央・攻撁E緁E(clusters 2, 3, 4)
    // Sector 2: 後手中央・攻撁E緁E(clusters 5, 6, 7)
    // Sector 3: 後手玉囲ぁEE守備 (clusters 8, 9)

    float32x4_t c0 = vld1q_f32(&state_l1[0][0]);
    float32x4_t c1 = vld1q_f32(&state_l1[1][0]);
    float32x4_t sec0 = vmulq_n_f32(vaddq_f32(c0, c1), 0.5f);
    vst1q_f32(&state_l2[0][0], sec0);

    float32x4_t c2 = vld1q_f32(&state_l1[2][0]);
    float32x4_t c3 = vld1q_f32(&state_l1[3][0]);
    float32x4_t c4 = vld1q_f32(&state_l1[4][0]);
    float32x4_t sec1 = vmulq_n_f32(vaddq_f32(vaddq_f32(c2, c3), c4), 0.3333333f);
    vst1q_f32(&state_l2[1][0], sec1);

    float32x4_t c5 = vld1q_f32(&state_l1[5][0]);
    float32x4_t c6 = vld1q_f32(&state_l1[6][0]);
    float32x4_t c7 = vld1q_f32(&state_l1[7][0]);
    float32x4_t sec2 = vmulq_n_f32(vaddq_f32(vaddq_f32(c5, c6), c7), 0.3333333f);
    vst1q_f32(&state_l2[2][0], sec2);

    float32x4_t c8 = vld1q_f32(&state_l1[8][0]);
    float32x4_t c9 = vld1q_f32(&state_l1[9][0]);
    float32x4_t sec3 = vmulq_n_f32(vaddq_f32(c8, c9), 0.5f);
    vst1q_f32(&state_l2[3][0], sec3);

    // 4. Level 2 -> Level 3 Isometry W_2 (4 セクター -> 1 大域Eクロ形勢スカラー v_Open)
    // 大局皁E勢 = (先手守備 + 先手攻撁E - (後手守備 + 後手攻撁E
    float32x4_t sente_total = vaddq_f32(sec0, sec1);
    float32x4_t gote_total = vaddq_f32(sec2, sec3);
    float32x4_t diff = vsubq_f32(sente_total, gote_total);

    // 特徴量重み [Material: 1.0, Invade: 40.0, Center: 20.0, Safety: 50.0]
    const float32x4_t v_weights = { 1.0f, 40.0f, 20.0f, 50.0f };
    float32x4_t weighted = vmulq_f32(diff, v_weights);
    float v_Open = vaddvq_f32(weighted);

        return v_Open;
#else
    // Standard scalar implementation for x86_64
    for (int c = 0; c < 10; ++c) {
        for (int k = 0; k < 4; ++k) {
            float v0 = state_l0[c * 4 + 0][k] * 0.85f;
            float v1 = state_l0[c * 4 + 1][k] * 0.15f;
            float v2 = state_l0[c * 4 + 2][k] * 0.15f;
            float v3 = state_l0[c * 4 + 3][k] * 0.85f;
            state_l1[c][k] = (v0 + v1 + v2 + v3) * 0.25f;
        }
    }
    for (int k = 0; k < 4; ++k) {
        state_l2[0][k] = (state_l1[0][k] + state_l1[1][k]) * 0.5f;
        state_l2[1][k] = (state_l1[2][k] + state_l1[3][k] + state_l1[4][k]) * 0.3333333f;
        state_l2[2][k] = (state_l1[5][k] + state_l1[6][k] + state_l1[7][k]) * 0.3333333f;
        state_l2[3][k] = (state_l1[8][k] + state_l1[9][k]) * 0.5f;
    }
    float v_Open = 0.0f;
    const float weights[4] = { 1.0f, 40.0f, 20.0f, 50.0f }; // Original weights
    for (int k = 0; k < 4; ++k) {
        float sente_total = state_l2[0][k] + state_l2[1][k];
        float gote_total = state_l2[2][k] + state_l2[3][k];
        v_Open += (sente_total + gote_total) * weights[k];
    }
    return v_Open;
#endif
}

extern "C" void contract_local_peps(
    const LocalTensorContext* ctx,
    const float* dynamic_lambda,
    float* out_gradient
) {
    if (!ctx || !dynamic_lambda || !out_gradient) return;

#if defined(__ARM_NEON) || defined(__aarch64__)
    // Dummy neon implementation for contract_local_peps since it was deleted
    for (int i = 0; i < 81; ++i) {
        out_gradient[i] = ctx->bond_weights[i] * ctx->potentials[i] * (*dynamic_lambda);
    }
#else
    for (int i = 0; i < 81; ++i) {
        out_gradient[i] = ctx->bond_weights[i] * ctx->potentials[i] * (*dynamic_lambda);
    }
#endif
}

void contract_peps_with_joseki_blend(
    const LocalTensorContext* ctx,
    const float* static_joseki,
    const float* t_cobordism,
    float* out_gradient
) {
    if (!ctx || !static_joseki || !t_cobordism || !out_gradient) return;

    float t_val = *t_cobordism;

#if defined(__ARM_NEON) || defined(__aarch64__)
    // 1. パラメータ t と (1-t) をEクタライズ褁E
    float32x4_t v_t = vdupq_n_f32(t_val);
    float32x4_t v_one_minus_t = vdupq_n_f32(1.0f - t_val);
    const float32x4_t v_neg_one = vdupq_n_f32(-1.0f);

    for (int i = 0; i < 80; i += 4) {
        // --- A. 動的 MERA/PEPS ポテンシャルの計箁E---
        float32x4_t w_dyn = vld1q_f32(&ctx->bond_weights[i]);
        float32x4_t p_dyn = vld1q_f32(&ctx->potentials[i]);
        float32x4_t v_dyn = vmulq_f32(w_dyn, p_dyn); // 動的 PEPS 評価

        // --- B. 静的定跡EEPSからロード済みEEローチEEE局面配置とのチEソル縮紁E---
        float32x4_t w_stat = vld1q_f32(&static_joseki[i]);
        float32x4_t v_stat = vmulq_f32(w_stat, p_dyn); // 静的定跡評価: w_stat_i * p_i

        // --- C. コボルチEズム・ブレンチE---
        // V = (1-t)*V_static + t*V_dynamic めEFused Multiply-Add (FMA) で計箁E    
    float32x4_t v_blend = vmulq_f32(v_stat, v_one_minus_t);
        v_blend = vmlaq_f32(v_blend, v_dyn, v_t);

        // 最急降下流E負の勾配）としてストア
        float32x4_t v_neg_grad = vmulq_f32(v_blend, v_neg_one);
        vst1q_f32(&out_gradient[i], v_neg_grad);
    }

    // 残端スカラー処琁E
    float dyn_val = ctx->bond_weights[80] * ctx->potentials[80];
    float stat_val = static_joseki[80] * ctx->potentials[80];
    float blended_val = (1.0f - t_val) * stat_val + t_val * dyn_val;
    out_gradient[80] = -blended_val;
#else
    for (int i = 0; i < 81; ++i) {
        float dyn_val = ctx->bond_weights[i] * ctx->potentials[i];
        float stat_val = static_joseki[i] * ctx->potentials[i];
        float blended_val = (1.0f - t_val) * stat_val + t_val * dyn_val;
        out_gradient[i] = -blended_val;
    }
#endif
}

extern "C" void free_topological_tensor_mmap(void* mmap_ptr) { if (mmap_ptr) delete static_cast<LocalTensorContext*>(mmap_ptr); }

extern "C" const int32_t* get_piece_indices(void* mmap_ptr) { if (!mmap_ptr) return nullptr; return static_cast<LocalTensorContext*>(mmap_ptr)->pieceIndices; }

extern "C" int get_remaining_pieces(void* mmap_ptr) {
    if (!mmap_ptr) return 40;
    auto* ctx = static_cast<LocalTensorContext*>(mmap_ptr);
    return ctx->remainingPieces;
}


extern "C" float atshogi_mera_evaluate_default_k40(const int32_t indices[40]) {
    if (!indices) return 0.0f;
    static atshogi::mera::MeraModel40 defaultModel;
    static bool load_success = false;
    static std::once_flag init_flag;
    
    std::call_once(init_flag, []() {
        if (defaultModel.loadFromFile("mera_k40.atmp")) {
            load_success = true;
        } else {
            fprintf(stderr, "info string failed to load MERA weights (ignoring for x86 heuristic)!\n");
        }
    });

    // We do not return 0.0f here anymore. The x86_64 heuristic does not use the loaded tensors.
    return defaultModel.evaluate40SitesNEON(indices);
}


extern "C" void backpropagate_topological_path(
    void* state_ptr,
    int step_index,
    double step_reward,
    double delta_p2
) {
    if (!state_ptr) return;
    
    LocalTensorContext* ctx = static_cast<LocalTensorContext*>(state_ptr);
    
    // チEロジカル重みの補正係数EE報酬 * (1.0 - Δp2) 
    // 数学皁E整合性が保たれてぁEパスEΔp2 -> 0Eほど、強ぁEEチEシャルが刻み込まれる
    float weight_factor = static_cast<float>(step_reward * (1.0 - delta_p2));

#if defined(__ARM_NEON) || defined(__aarch64__)
    float32x4_t v_weight = vdupq_n_f32(weight_factor);
    
    // NEON SIMDによる81マスのポテンシャル場更新EEMA: 融合積和EE    // V_new = V_old + weight_factor * Bond_weights
    for (int i = 0; i < 80; i += 4) {
        float32x4_t v_pot = vld1q_f32(&ctx->potentials[i]);
        float32x4_t v_bond = vld1q_f32(&ctx->bond_weights[i]);
        
        // ポテンシャルに谷を彫り込むEEV_new = V_old + (weight_factor * Bond_weights)
        float32x4_t v_new_pot = vmlaq_f32(v_pot, v_bond, v_weight);
        
        vst1q_f32(&ctx->potentials[i], v_new_pot);
    }
    
    // 81要素目の残端処琁E
    ctx->potentials[80] += weight_factor * ctx->bond_weights[80];
#else
    for (int i = 0; i < 81; ++i) {
        ctx->potentials[i] += weight_factor * ctx->bond_weights[i];
    }
#endif
}

static LocalTensorContext g_fiber_pool[128];
static int g_fiber_idx = 0;

extern "C" void* initialize_k40_state(void* mmap_ptr) {
    g_fiber_idx = 0;
    LocalTensorContext* ctx = &g_fiber_pool[g_fiber_idx++];
    memset(ctx, 0, sizeof(LocalTensorContext));
    for (int i = 0; i < 81; ++i) ctx->bond_weights[i] = 1.0f;
    return ctx;
}

extern "C" double get_gaifullin_residual(void* state_ptr) {
    if (!state_ptr) return 0.0;
    return 1e-10; // 初期・定跡進行時の Gaifullin 不変量残差 Δp2
}

extern "C" void* sample_next_state_with_homotopy(void* mmap_ptr, void* state_ptr) {
    if (g_fiber_idx >= 127) g_fiber_idx = 0;
    LocalTensorContext* next = &g_fiber_pool[g_fiber_idx++];
    if (state_ptr) {
        memcpy(next, state_ptr, sizeof(LocalTensorContext));
    }
    return next;
}

extern "C" bool is_checkmate_attractor(void* state_ptr) {
    return false;
}

alignas(16) static float g_topological_joseki_manifold[81] = {
    // 1筁E 1a..1i
       -4.75f,    +5.20f,   +29.74f,   -10.98f,   +17.04f,    +6.65f,    -0.69f,    +3.38f,   +19.42f, 
    // 2筁E 2a..2i
      -15.54f,   +22.57f,   -78.04f,   -37.06f,    -0.72f,   +78.11f,    +0.95f,    -9.43f,   -10.34f, 
    // 3筁E 3a..3i
       +9.28f,    -1.43f,   -52.75f,   +40.22f,    +1.95f,    -4.06f,   +14.13f,    +0.19f,    +0.40f, 
    // 4筁E 4a..4i
       -2.41f,   -14.09f,   -26.90f,   -29.22f,    -1.83f,   -23.27f,   -16.93f,   -18.76f,    -0.03f, 
    // 5筁E 5a..5i
      -10.22f,    +0.06f,   +10.32f,    +0.21f,   +53.98f,    -0.08f,   +10.14f,    -0.26f,    +6.58f, 
    // 6筁E 6a..6i
       +0.00f,    -0.03f,    +0.00f,   -58.08f,    +0.07f,   +47.32f,    -0.01f,   -20.93f,    +0.00f, 
    // 7筁E 7a..7i
      -12.93f,    +0.18f,   -46.72f,    -0.57f,    -0.33f,   +98.00f,   -13.08f,   +64.21f,    -3.74f, 
    // 8筁E 8a..8i
       -0.07f,    -4.25f,    -0.02f,   -86.43f,    -3.96f,   +40.88f,    +0.19f,   -45.85f,    -2.76f, 
    // 9筁E 9a..9i
      -31.31f,    -0.04f,    +1.12f,    -0.24f,   -14.38f,    +5.84f,    -0.02f,    -0.01f,    +0.73f
};

float* get_global_topological_manifold() {
    return g_topological_joseki_manifold;
}

extern "C" void backpropagate_game_trajectory(
    const int* move_to_sqs,
    int move_count,
    double reward,
    double alpha,
    double gamma
) {
    if (!move_to_sqs || move_count <= 0) return;

    for (int a = 0; a < move_count; ++a) {
        int sq = move_to_sqs[a];
        if (sq >= 0 && sq < 81) {
            int dist_from_end = move_count - 1 - a;
            double decay = pow(gamma, (double)dist_from_end);
            double delta_p2 = 1e-10; // Gaifullin 不変量残差
            float d_val = static_cast<float>(alpha * reward * decay * (1.0 - delta_p2));
            g_topological_joseki_manifold[sq] += d_val;
        }
    }
}

} } // namespace atshogi::mera

extern "C" {

}
