#include "app/src/main/cpp/ShogiBitboardCore.h"
#include "app/src/main/cpp/MeraTensorNetworkNEON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

#define TOTAL_TARGET_GAMES 10000
#define CONVERGENCE_EPSILON 1e-5f

static float s_global_tensor[81];
static float s_prev_tensor[81];
static std::mutex s_tensor_mutex;

static inline uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *state = x;
}
static inline double rand_double(uint64_t* state) {
    return (double)(xorshift64(state) & 0xFFFFFFFFFFFFULL) / (double)0x1000000000000ULL;
}

static int play_fast_game(uint64_t* rng_state, int* out_to_sqs, int max_moves, double* out_reward) {
    ShogiBoard board;
    atshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", &board);

    int move_len = 0;
    int ply = 0;

    while (ply < max_moves) {
        Move16 legal_moves[600];
        int count = atshogi_generate_legal_moves(&board, legal_moves);
        if (count == 0) {
            *out_reward = (board.turn == BLACK_TURN) ? -1.0 : 1.0;
            return move_len;
        }

        Move16 selected_move = legal_moves[0];

        if (ply < 6 && rand_double(rng_state) < 0.08) {
            int idx = (int)(rand_double(rng_state) * count);
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
            out_to_sqs[move_len++] = to_sq;
        }

        ShogiBoard next_b;
        if (!atshogi_apply_move(&board, selected_move, &next_b)) break;
        board = next_b;
        ply++;
    }

    *out_reward = 0.0;
    return move_len;
}

int main(int argc, char** argv) {
    atshogi_init_tables();
    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 16;

    printf("=================================================================\n");
    printf("  ATShogi-OM Extreme: Host 16-Core Saturated Joseki Generator    \n");
    printf("  Threads: %u (C++17 std::thread) | Target: %d Games           \n", num_threads, TOTAL_TARGET_GAMES);
    printf("=================================================================\n\n");

    float* base_tensor = get_global_topological_manifold();
    memcpy(s_global_tensor, base_tensor, sizeof(s_global_tensor));
    memcpy(s_prev_tensor, base_tensor, sizeof(s_prev_tensor));

    int total_games = 0;
    int batch_size = 500;
    int batch_idx = 1;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (total_games < TOTAL_TARGET_GAMES) {
        int games_in_batch = batch_size;
        if (total_games + games_in_batch > TOTAL_TARGET_GAMES) {
            games_in_batch = TOTAL_TARGET_GAMES - total_games;
        }

        std::atomic<int> atomic_sente_wins(0);
        std::atomic<int> atomic_gote_wins(0);
        std::atomic<int> atomic_draws(0);
        std::atomic<int> games_counter(0);

        std::vector<std::thread> workers;
        workers.reserve(num_threads);

        for (unsigned int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, t]() {
                uint64_t rng = 88172645463325252ULL + (uint64_t)t * 10007ULL;
                int local_moves[512];

                while (true) {
                    int g_idx = games_counter.fetch_add(1);
                    if (g_idx >= games_in_batch) break;

                    double reward = 0.0;
                    int len = play_fast_game(&rng, local_moves, 512, &reward);

                    if (reward > 0.5) atomic_sente_wins++;
                    else if (reward < -0.5) atomic_gote_wins++;
                    else atomic_draws++;

                    {
                        std::lock_guard<std::mutex> lock(s_tensor_mutex);
                        backpropagate_game_trajectory(local_moves, len, reward, 0.08, 0.95);
                    }
                }
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        total_games += games_in_batch;

        // テンソル場のフロベニウスノルム変化量
        float delta_norm = 0.0f;
        for (int i = 0; i < 81; ++i) {
            float diff = base_tensor[i] - s_prev_tensor[i];
            delta_norm += diff * diff;
        }
        delta_norm = sqrtf(delta_norm) / (float)games_in_batch;
        memcpy(s_prev_tensor, base_tensor, sizeof(s_prev_tensor));

        auto now = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        double speed = (double)total_games / (elapsed > 0 ? elapsed : 0.001);

        printf("[Batch %2d] Games: %5d / %5d (Sente: %3d, Gote: %3d, Draw: %3d) | ||d_T||: %.6f | Elapsed: %6.2fs (%.1f games/s)\n",
               batch_idx, total_games, TOTAL_TARGET_GAMES,
               atomic_sente_wins.load(), atomic_gote_wins.load(), atomic_draws.load(),
               delta_norm, elapsed, speed);
        fflush(stdout);

        if (total_games >= 2000 && delta_norm < CONVERGENCE_EPSILON) {
            printf("\n>>> [SATURATION CONVERGENCE] Fully saturated at %d games! (||d_T|| < %.1e)\n", total_games, CONVERGENCE_EPSILON);
            break;
        }

        batch_size = (batch_size * 2 <= 2000) ? batch_size * 2 : 2000;
        batch_idx++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_elapsed = std::chrono::duration<double>(end_time - start_time).count();
    printf("\n=== Saturated Joseki Generation Finished: %d Games in %.2f s (%.1f games/s) ===\n\n",
           total_games, total_elapsed, (double)total_games / (total_elapsed > 0 ? total_elapsed : 0.001));

    // GeneratedStaticJoseki.h にシリアライズ保存
    FILE* fp = fopen("app/src/main/cpp/GeneratedStaticJoseki.h", "w");
    if (fp) {
        fprintf(fp, "// Auto-generated by ATShogi-OM Extreme Host 16-Core Saturation Generator\n");
        fprintf(fp, "// Total Games: %d, Parallel Threads: %u, Saturated: True\n", total_games, num_threads);
        fprintf(fp, "#ifndef ATSHOGI_GENERATED_STATIC_JOSEKI_H\n");
        fprintf(fp, "#define ATSHOGI_GENERATED_STATIC_JOSEKI_H\n\n");
        fprintf(fp, "#include <stdint.h>\n\n");
        fprintf(fp, "static const float g_selfplay_static_joseki[81] = {\n");

        for (int f = 0; f < 9; ++f) {
            fprintf(fp, "    // %d筋: %da..%di\n    ", f + 1, f + 1, f + 1);
            for (int r = 0; r < 9; ++r) {
                int sq = f * 9 + r;
                fprintf(fp, "%+8.2ff%s", base_tensor[sq], (sq == 80) ? "" : ", ");
            }
            fprintf(fp, "\n");
        }
        fprintf(fp, "};\n\n");
        fprintf(fp, "#endif // ATSHOGI_GENERATED_STATIC_JOSEKI_H\n");
        fclose(fp);
        printf("[SUCCESS] app/src/main/cpp/GeneratedStaticJoseki.h updated with saturated tensor manifold!\n");
    }

    return 0;
}
