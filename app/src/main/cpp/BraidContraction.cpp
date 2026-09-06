
#include "cshogi_core.h"
static inline void extract_tensor_indices_braid(const ShogiBoard* board, int32_t indices[40]) {
    int idx = 0;
    // 1. Board pieces
    for (int sq = 0; sq < 81; ++sq) {
        uint8_t p = board->board[sq];
        if (p != 0) {
            int pt = p & 0x0F;
            int col = p >> 4;
            indices[idx++] = sq | (pt << 7) | (col << 11);
        }
    }
    // 2. Hand pieces
    for (int col = 0; col < 2; ++col) {
        for (int i = 0; i < board->hands[col].pawn; ++i) indices[idx++] = 127 | (1 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].lance; ++i) indices[idx++] = 127 | (2 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].knight; ++i) indices[idx++] = 127 | (3 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].silver; ++i) indices[idx++] = 127 | (4 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].gold; ++i) indices[idx++] = 127 | (5 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].bishop; ++i) indices[idx++] = 127 | (6 << 7) | (col << 11);
        for (int i = 0; i < board->hands[col].rook; ++i) indices[idx++] = 127 | (7 << 7) | (col << 11);
    }
    while (idx < 40) indices[idx++] = 127;
}

#include "cshogi_core.h"
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

extern "C" {
    void* load_topological_tensor_mmap();
    void encode_sfen_to_tensor_state(void* mmap_ptr, const char* sfen);
    const int* get_piece_indices(void* mmap_ptr);
    float atshogi_mera_evaluate_default_k40(const int* indices);
    void atshogi_init_mps();
}

// 1. Topological Independence (Mazurkiewicz Trace Commutativity)
bool is_independent(Move16 m1, Move16 m2) {
    if (m1 == 0 || m2 == 0) return false;
    
    int src1 = (m1 & MOVE_DROP_FLAG) ? -1 : (m1 >> 7) & 0x7F;
    int dst1 = m1 & 0x7F;
    
    int src2 = (m2 & MOVE_DROP_FLAG) ? -1 : (m2 >> 7) & 0x7F;
    int dst2 = m2 & 0x7F;
    
    if (src1 == src2 || dst1 == dst2 || src1 == dst2 || src2 == dst1) return false;
    
    auto dist = [](int s1, int s2) {
        if (s1 < 0 || s2 < 0) return 99;
        int f1 = s1 / 9, r1 = s1 % 9;
        int f2 = s2 / 9, r2 = s2 % 9;
        return std::max(std::abs(f1 - f2), std::abs(r1 - r2));
    };
    
    if (dist(dst1, dst2) <= 1) return false;
    if (src1 >= 0 && dist(src1, dst2) <= 1) return false;
    if (src2 >= 0 && dist(src2, dst1) <= 1) return false;
    
    return true;
}

// 3. 2-Category Adjunction (Alpha-Beta on Quotient Space)
#include <atomic>
constexpr size_t TT_SIZE = 16777216; // 16M entries = 128 MB
std::atomic<uint64_t>* g_shared_tt = nullptr;

static inline void init_shared_tt() {
    if (!g_shared_tt) {
        g_shared_tt = new std::atomic<uint64_t>[TT_SIZE];
        for (size_t i = 0; i < TT_SIZE; ++i) g_shared_tt[i].store(0, std::memory_order_relaxed);
    }
}
float braid_adjunction_search(ShogiBoard* board, int depth, float alpha, float beta, Move16 prev_own_move, Move16 prev_opp_move, void* mmap_ptr) {
     if (depth <= 0) {
        int32_t indices[40];
        extract_tensor_indices_braid(board, indices);
        float eval = atshogi_mera_evaluate_default_k40(indices);
        return (board->turn == 0) ? eval : -eval;
    }
    
    Move16 legal_moves[600];
    int count = cshogi_generate_legal_moves(board, legal_moves);
    if (count == 0) return -1e8f; // Adjunction lower bound (loss)
    
    float max_val = -1e9f;
    
    for (int i = 0; i < count; ++i) {
        Move16 m = legal_moves[i];
        
        // 2. Braid Contraction (Canonical Ordering)
        if (prev_own_move != 0 && is_independent(m, prev_own_move)) {
            if (m < prev_own_move) {
                // Prune! This branch belongs to the homotopy class represented by the canonical order.
                continue; 
            }
        }
        
        ShogiBoard next_b;
        if (!cshogi_apply_move(board, m, &next_b)) continue;
        
        // Recursive functor application with adjunction bounds
        float val = -braid_adjunction_search(&next_b, depth - 1, -beta, -alpha, prev_opp_move, m, mmap_ptr);
        
        if (val > max_val) {
            max_val = val;
        }
        if (max_val > alpha) {
            alpha = max_val;
        }
        if (alpha >= beta) {
            break; // 2-Morphism Galois Connection Cutoff
        }
    }
    
    return max_val;
}

extern "C" void c_atshogi_braid_delta_t(void* handle, int max_depth) {
    if (!handle) return;
    ShogiBoard* start_board = (ShogiBoard*)handle;
    
    fprintf(stderr, "[BraidGen] Starting Topological Braid Contraction + Adjunction (Depth=%d)\n", max_depth);
    
    fprintf(stderr, "Loading mmap...\n"); void* mmap_ptr = load_topological_tensor_mmap(); fprintf(stderr, "Loaded\n");
    
    Move16 legal_moves[600];
    int count = cshogi_generate_legal_moves(start_board, legal_moves);
    
    float best_val = -1e9f;
    Move16 best_move = 0;
    
    for (int i = 0; i < count; ++i) {
        ShogiBoard next_b;
        if (!cshogi_apply_move(start_board, legal_moves[i], &next_b)) continue;
        
        float val = -braid_adjunction_search(&next_b, max_depth - 1, -1e9f, -best_val, 0, legal_moves[i], mmap_ptr);
        
        if (val > best_val) {
            best_val = val;
            best_move = legal_moves[i];
        }
    }
    
    if (best_move != 0) {
        char usi[16];
        cshogi_move_to_usi(best_move, usi);
        fprintf(stderr, "[BraidGen] Convergence Complete. Best Trunk Move: %s (Eval: %.2f)\n", usi, best_val);
    }
}


static inline void cshogi_compute_hash128_braid(const ShogiBoard* board, Simplex81* out_hash) {
    uint64_t hash_lo = 0xcbf29ce484222325ULL;
    uint64_t hash_hi = 0x100000001b3ULL;
    for (size_t i = 0; i < 81; i++) {
        hash_lo ^= board->board[i];
        hash_lo *= 0x100000001b3ULL;
        hash_hi ^= hash_lo;
        hash_hi *= 0x100000001b3ULL;
    }
    for (size_t i = 0; i < sizeof(HandPieces)*2; i++) {
        uint8_t b = ((uint8_t*)board->hands)[i];
        hash_lo ^= b;
        hash_lo *= 0x100000001b3ULL;
        hash_hi ^= hash_lo;
        hash_hi *= 0x100000001b3ULL;
    }
    hash_lo ^= board->turn;
    hash_lo *= 0x100000001b3ULL;
    hash_hi ^= hash_lo;
    hash_hi *= 0x100000001b3ULL;

    out_hash->mask_hi = hash_hi;
    out_hash->mask_lo = hash_lo;
}

extern "C" uint16_t c_atshogi_find_best_move(void* handle, int max_depth) {
    if (!handle) return 0;
    ShogiBoard* start_board = (ShogiBoard*)handle;
    
    // 1. Check Offline Omni-Atlas (Constant time response)
    FILE* f = fopen("omni_atlas.bin", "rb");
    if (f) {
        AtlasNode node;
        Simplex81 curr_hash;
        fprintf(stderr, "Compute hash...\n"); cshogi_compute_hash128_braid(start_board, &curr_hash); fprintf(stderr, "Hashed\n");
        while (fread(&node, sizeof(AtlasNode), 1, f)) {
            if (node.state_simplex.mask_hi == curr_hash.mask_hi && node.state_simplex.mask_lo == curr_hash.mask_lo) {
                fclose(f);
                fprintf(stderr, "[ATShogi::Core] Found move in Omni-Atlas! Instant response.\n");
                return (Move16)node.best_move.mask_lo;
            }
        }
        fclose(f);
    }
    
    // 2. Dynamic Fallback
    fprintf(stderr, "Loading mmap...\n"); void* mmap_ptr = load_topological_tensor_mmap(); fprintf(stderr, "Loaded\n");
    Move16 legal_moves[600];
    int count = cshogi_generate_legal_moves(start_board, legal_moves);
    if(count == 0) return 0;
    float best_val = -1e9f;
    Move16 best_move = legal_moves[0];
    for (int i = 0; i < count; ++i) {
        ShogiBoard next_b;
        if (!cshogi_apply_move(start_board, legal_moves[i], &next_b)) continue;
        float val = -braid_adjunction_search(&next_b, max_depth - 1, -1e9f, -best_val, 0, legal_moves[i], mmap_ptr);
        if (val > best_val) {
            best_val = val;
            best_move = legal_moves[i];
        }
    }
    return best_move;
}
