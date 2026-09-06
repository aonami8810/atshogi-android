#include "app/src/main/cpp/ShogiBitboardCore.h"
#include "app/src/main/cpp/MeraTensorNetworkNEON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_TOTAL_GAMES 10000
#define CONVERGENCE_EPSILON 1e-4f

static float s_prev_tensor_field[81];

// 簡易乱数 (XORShift64)
static uint64_t s_rng_state = 88172645463325252ULL;
static inline uint64_t xorshift64(void) {
    uint64_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return s_rng_state = x;
}
static inline double rand_double(void) {
    return (double)(xorshift64() & 0xFFFFFFFFFFFFULL) / (double)0x1000000000000ULL;
}

// 1局の自己対局を実行し、指し手履歴と勝敗結果を返す
static int play_single_selfplay_game(int* out_move_to_sqs, int max_moves, double* out_reward) {
    ShogiBoard board;
    atshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", &board);

    int move_len = 0;
    int ply = 0;

    while (ply < max_moves) {
        Move16 legal_moves[600];
        int count = atshogi_generate_legal_moves(&board, legal_moves);
        if (count == 0) {
            // 現在手番の負け（詰みアトラクター）
            *out_reward = (board.turn == BLACK_TURN) ? -1.0 : 1.0;
            return move_len;
        }

        Move16 selected_move = legal_moves[0];

        // 序盤の多様性確保（ホモトピー揺らぎ: 10%でランダム、90%でMERA/PEPSエンジン最善手）
        if (ply < 8 && rand_double() < 0.12) {
            int idx = (int)(rand_double() * count);
            if (idx >= count) idx = count - 1;
            selected_move = legal_moves[idx];
        } else {
            char best_buf[64] = {0};
            atshogi_select_best_move_k40(&board, best_buf, sizeof(best_buf));
            if (strcmp(best_buf, "resign") == 0) {
                *out_reward = (board.turn == BLACK_TURN) ? -1.0 : 1.0;
                return move_len;
            }
            selected_move = atshogi_parse_usi_move(best_buf, &board);
            if (selected_move == 0) selected_move = legal_moves[0];
        }

        int to_sq = (selected_move >> 6) & 0x7F;
        if (move_len < max_moves) {
            out_move_to_sqs[move_len++] = to_sq;
        }

        ShogiBoard next_b;
        if (!atshogi_apply_move(&board, selected_move, &next_b)) break;
        board = next_b;
        ply++;
    }

    // 最大手数到達（千日手）
    *out_reward = 0.0;
    return move_len;
}

int main(void) {
    atshogi_init_tables();
    printf("=================================================================\n");
    printf("  ATShogi-OM Extreme: Topological Self-Play Saturation Engine    \n");
    printf("  Target: Max 10,000 Games with Exponential Backoff Saturation   \n");
    printf("=================================================================\n\n");

    float* global_tensor = get_global_topological_manifold();
    memcpy(s_prev_tensor_field, global_tensor, sizeof(s_prev_tensor_field));

    int total_games_played = 0;
    int batch_size = 100;
    int batch_idx = 1;
    double start_time = (double)clock() / CLOCKS_PER_SEC;

    int move_history[512];

    while (total_games_played < MAX_TOTAL_GAMES) {
        int games_in_this_batch = batch_size;
        if (total_games_played + games_in_this_batch > MAX_TOTAL_GAMES) {
            games_in_this_batch = MAX_TOTAL_GAMES - total_games_played;
        }

        int sente_wins = 0, gote_wins = 0, draws = 0;

        for (int g = 0; g < games_in_this_batch; ++g) {
            double reward = 0.0;
            int moves = play_single_selfplay_game(move_history, 512, &reward);
            
            if (reward > 0.5) sente_wins++;
            else if (reward < -0.5) gote_wins++;
            else draws++;

            // Topological Backpropagation (時空二重減衰 逆伝播)
            backpropagate_game_trajectory(move_history, moves, reward, 0.1, 0.95);
        }

        total_games_played += games_in_this_batch;

        // テンソル場のフロベニウスノルム変化量（収束判定）
        float delta_norm = 0.0f;
        for (int i = 0; i < 81; ++i) {
            float diff = global_tensor[i] - s_prev_tensor_field[i];
            delta_norm += diff * diff;
        }
        delta_norm = sqrtf(delta_norm) / (float)games_in_this_batch;
        memcpy(s_prev_tensor_field, global_tensor, sizeof(s_prev_tensor_field));

        double elapsed = ((double)clock() / CLOCKS_PER_SEC) - start_time;
        printf("[Batch %2d] Games: %5d / %5d (Sente: %d, Gote: %d, Draw: %d) | ||d_Tensor||: %.6f | Elapsed: %.2fs\n",
               batch_idx, total_games_played, MAX_TOTAL_GAMES, sente_wins, gote_wins, draws, delta_norm, elapsed);
        fflush(stdout);

        // 収束判定（特異値・ポテンシャル飽和）
        if (total_games_played >= 1000 && delta_norm < CONVERGENCE_EPSILON) {
            printf("\n>>> [CONVERGENCE ACHIEVED] Tensor manifold has reached theoretical saturation (||d_Tensor|| < %.1e) at %d games!\n",
                   CONVERGENCE_EPSILON, total_games_played);
            break;
        }

        // 指数バックオフ（バッチサイズを 2 倍に拡大）
        batch_size = (batch_size * 2 <= 2000) ? batch_size * 2 : 2000;
        batch_idx++;
    }

    double total_elapsed = ((double)clock() / CLOCKS_PER_SEC) - start_time;
    printf("\n=== Self-Play Session Completed: %d Games in %.2f seconds (%.1f games/sec) ===\n\n",
           total_games_played, total_elapsed, (double)total_games_played / (total_elapsed > 0 ? total_elapsed : 0.001));

    // 結果を標準出力にフォーマット出力
    printf("--- SATURATED STATIC JOSEKI C ARRAY ---\n");
    for (int f = 0; f < 9; ++f) {
        printf("    // %d筋: %da..%di\n    ", f + 1, f + 1, f + 1);
        for (int r = 0; r < 9; ++r) {
            int sq = f * 9 + r;
            printf("%+8.2ff%s", global_tensor[sq], (sq == 80) ? "" : ", ");
        }
        printf("\n");
    }
    printf("-----------------------------------------\n");

    return 0;
}
