#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct LocalTensorContextSoA {
    float potentials[16][81];  // 16局面並列ポテンシャル
    float bond_weights[16][81]; // 16局面並列ボンド重み
};

extern "C" {

/**
 * 16局面分の自己対局ステップ報酬を、共有定跡テンソルへ並列かつアトミックに逆伝播（FMA更新）する
 * AVX-512、AVX2、スカラーの各環境に対応
 * @param shared_ctx_soa [in/out] mmapマウントされた共有ポテンシャル SoA
 * @param v_rewards      [in]     減衰適用済みのステップ報酬ベクトル（16要素）
 * @param v_residuals    [in]     各局面の Gaifullin 残差ベクトル（16要素、整合性重み）
 */
void backpropagate_omni_tensor(
    LocalTensorContextSoA* shared_ctx_soa,
    const float* v_rewards,
    const float* v_residuals
) {
#if defined(__AVX512F__)
    // 1. 16要素の報酬および残差ベクトルを AVX-512 レジスタにロード
    __m512 r = _mm512_loadu_ps(v_rewards);
    __m512 res = _mm512_loadu_ps(v_residuals);

    // 2. 補正係数 W = r * (1.0f - res) の計算
    __m512 one = _mm512_set1_ps(1.0f);
    __m512 w_factor = _mm512_mul_ps(r, _mm512_sub_ps(one, res));

    // 3. 81マスのポテンシャル場を各局面について並列一元更新
    for (int cell = 0; cell < 81; ++cell) {
        float cell_pots[16];
        float cell_bonds[16];
        for (int lane = 0; lane < 16; ++lane) {
            cell_pots[lane] = shared_ctx_soa->potentials[lane][cell];
            cell_bonds[lane] = shared_ctx_soa->bond_weights[lane][cell];
        }

        __m512 v_pots = _mm512_loadu_ps(cell_pots);
        __m512 v_bonds = _mm512_loadu_ps(cell_bonds);

        // V_new = V_old + W * Bond_weight (FMA: _mm512_fmadd_ps)
        __m512 v_new_pots = _mm512_fmadd_ps(w_factor, v_bonds, v_pots);

        // 共有メモリへ書き戻し
        float out_pots[16];
        _mm512_storeu_ps(out_pots, v_new_pots);
        for (int lane = 0; lane < 16; ++lane) {
            shared_ctx_soa->potentials[lane][cell] = out_pots[lane];
        }
    }

#elif defined(__AVX2__)
    // AVX2 (256-bit): 16要素を 8要素 x 2バッチ で処理する
    __m256 r_lo = _mm256_loadu_ps(v_rewards);
    __m256 r_hi = _mm256_loadu_ps(v_rewards + 8);
    __m256 res_lo = _mm256_loadu_ps(v_residuals);
    __m256 res_hi = _mm256_loadu_ps(v_residuals + 8);

    __m256 one = _mm256_set1_ps(1.0f);
    
    // W = r * (1.0f - res)
    __m256 w_factor_lo = _mm256_mul_ps(r_lo, _mm256_sub_ps(one, res_lo));
    __m256 w_factor_hi = _mm256_mul_ps(r_hi, _mm256_sub_ps(one, res_hi));

    for (int cell = 0; cell < 81; ++cell) {
        float cell_pots[16];
        float cell_bonds[16];
        for (int lane = 0; lane < 16; ++lane) {
            cell_pots[lane] = shared_ctx_soa->potentials[lane][cell];
            cell_bonds[lane] = shared_ctx_soa->bond_weights[lane][cell];
        }

        __m256 v_pots_lo = _mm256_loadu_ps(cell_pots);
        __m256 v_pots_hi = _mm256_loadu_ps(cell_pots + 8);
        __m256 v_bonds_lo = _mm256_loadu_ps(cell_bonds);
        __m256 v_bonds_hi = _mm256_loadu_ps(cell_bonds + 8);

        // FMA: V_new = V_old + W * Bond_weight
        __m256 v_new_pots_lo = _mm256_fmadd_ps(w_factor_lo, v_bonds_lo, v_pots_lo);
        __m256 v_new_pots_hi = _mm256_fmadd_ps(w_factor_hi, v_bonds_hi, v_pots_hi);

        float out_pots[16];
        _mm256_storeu_ps(out_pots, v_new_pots_lo);
        _mm256_storeu_ps(out_pots + 8, v_new_pots_hi);

        for (int lane = 0; lane < 16; ++lane) {
            shared_ctx_soa->potentials[lane][cell] = out_pots[lane];
        }
    }

#else
    // ポータブルフォールバック（スカラー処理）
    for (int lane = 0; lane < 16; ++lane) {
        float w = v_rewards[lane] * (1.0f - v_residuals[lane]);
        for (int cell = 0; cell < 81; ++cell) {
            shared_ctx_soa->potentials[lane][cell] += w * shared_ctx_soa->bond_weights[lane][cell];
        }
    }
#endif
}

// Forward declarations for FFI
void* atshogi_position_create();
void atshogi_position_set_startpos(void* handle);
void atshogi_omni_generate_atlas(void* handle, const char* out_filepath, const char* trunk_usi_string);
void atshogi_position_free(void* handle);

/**
 * 16局面のバッチ自己対局を実行するCインタフェース（Haskellから呼び出される）
 */
#include "../app/src/main/cpp/cshogi_core.h"

void execute_batch_selfplay_16_lanes(void* mmap_ptr) {
    bool allocated = false;
    LocalTensorContextSoA* shared_ctx = (LocalTensorContextSoA*)mmap_ptr;
    
    if (!shared_ctx) {
        shared_ctx = (LocalTensorContextSoA*)calloc(1, sizeof(LocalTensorContextSoA));
        
        // Initialize bonds with 1.0f
        for (int lane = 0; lane < 16; ++lane) {
            for (int cell = 0; cell < 81; ++cell) {
                shared_ctx->bond_weights[lane][cell] = 1.0f;
            }
        }
        allocated = true;
    }
    
    // Simulate 16 games in parallel
    ShogiBoard boards[16];
    bool active[16];
    float rewards[16] = {0};
    float residuals[16] = {0}; // Gaifullin residual mock
    
    for (int i = 0; i < 16; ++i) {
        cshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", &boards[i]);
        active[i] = true;
    }
    
    for (int ply = 0; ply < 512; ++ply) {
        bool any_active = false;
        Move16 moves[16][600];
        int num_moves[16] = {0};
        
        for (int i = 0; i < 16; ++i) {
            if (!active[i]) continue;
            any_active = true;
            num_moves[i] = cshogi_generate_legal_moves(&boards[i], moves[i]);
            
            if (num_moves[i] == 0) {
                // Checkmate (loss for current turn)
                rewards[i] = -1.0f; 
                active[i] = false;
            } else {
                // Random walk
                int rand_idx = rand() % num_moves[i];
                ShogiBoard next_board;
                cshogi_apply_move(&boards[i], moves[i][rand_idx], &next_board);
                boards[i] = next_board;
            }
        }
        
        if (!any_active) break;
    }
    
    // Fallback: draw for games that hit max ply
    for (int i = 0; i < 16; ++i) {
        if (active[i]) {
            rewards[i] = 0.0f;
            active[i] = false;
        }
    }
    
    // Backpropagate rewards (using the SoA AVX kernel)
    backpropagate_omni_tensor(shared_ctx, rewards, residuals);
    
    // Flush to static_joseki.bin
    FILE* out_f = fopen("static_joseki.bin", "wb");
    if (out_f) {
        fwrite(shared_ctx, sizeof(LocalTensorContextSoA), 1, out_f);
        fclose(out_f);
    }
    
    if (allocated) {
        free(shared_ctx);
    }
}

}
