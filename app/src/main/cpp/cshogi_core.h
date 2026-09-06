#ifndef CSHOGI_CORE_H
#define CSHOGI_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 81マス (0..80): sq = (file - 1) * 9 + (rank - 1), file in 1..9, rank in 1..9
// 128bit Bitboard: lo (0..63), hi (64..80)
typedef struct {
    uint64_t lo; // sq 0..63
    uint64_t hi; // sq 64..80
} Bitboard;

// 243頂点モデル (81マス x 3層):
// Layer 0 (0..80): 先手支配・利き空間 (LayerBlack)
// Layer 1 (81..161): 後手支配・利き空間 (LayerWhite)
// Layer 2 (162..242): 争点・利きの衝突空間 (LayerContested = LayerBlack & LayerWhite)
typedef struct {
    Bitboard layer_black;     // Layer 0: 0..80
    Bitboard layer_white;     // Layer 1: 81..161
    Bitboard layer_contested; // Layer 2: 162..242
} Simplex243;

typedef struct {
    uint64_t mask_hi;
    uint64_t mask_lo;
} Simplex81;

typedef struct __attribute__((aligned(64))) {
    Simplex81 state_simplex;
    Simplex81 best_move;
    double    morse_value;
    uint8_t   isotropy_mask;
    uint8_t   padding[7];
} AtlasNode;


enum PieceType {
    EMPTY = 0,
    PAWN = 1,
    LANCE = 2,
    KNIGHT = 3,
    SILVER = 4,
    GOLD = 5,
    BISHOP = 6,
    ROOK = 7,
    KING = 8,
    PRO_PAWN = 9,
    PRO_LANCE = 10,
    PRO_KNIGHT = 11,
    PRO_SILVER = 12,
    HORSE = 13,
    DRAGON = 14
};

enum Color {
    BLACK_TURN = 0,
    WHITE_TURN = 1
};

typedef struct {
    uint8_t pawn;
    uint8_t lance;
    uint8_t knight;
    uint8_t silver;
    uint8_t gold;
    uint8_t bishop;
    uint8_t rook;
    uint8_t padding;
} HandPieces;

typedef struct {
    uint8_t board[81];       // 各マスの駒 (color << 4 | piece_type), 0 is empty
    Bitboard piece_bb[2][15]; // [color][piece_type]
    Bitboard color_bb[2];    // [color] occupied
    Bitboard occupied;       // all occupied
    HandPieces hands[2];     // [color]
    uint8_t turn;            // 0: Black, 1: White
    uint8_t king_sq[2];      // 各玉の位置 (0..80)
} ShogiBoard;

typedef uint16_t Move16;

#define MOVE_DROP_FLAG    0x8000
#define MOVE_PROMOTE_FLAG 0x4000

static inline int make_sq(int file, int rank) {
    return (file - 1) * 9 + (rank - 1);
}

static inline int sq_file(int sq) {
    return (sq / 9) + 1;
}

static inline int sq_rank(int sq) {
    return (sq % 9) + 1;
}

static inline bool bb_test(const Bitboard* bb, int sq) {
    if (sq < 64) {
        return (bb->lo & (1ULL << sq)) != 0;
    } else {
        return (bb->hi & (1ULL << (sq - 64))) != 0;
    }
}

static inline void bb_set(Bitboard* bb, int sq) {
    if (sq < 64) {
        bb->lo |= (1ULL << sq);
    } else {
        bb->hi |= (1ULL << (sq - 64));
    }
}

static inline void bb_clear(Bitboard* bb, int sq) {
    if (sq < 64) {
        bb->lo &= ~(1ULL << sq);
    } else {
        bb->hi &= ~(1ULL << (sq - 64));
    }
}

void cshogi_init_tables(void);
bool cshogi_from_sfen(const char* sfen, ShogiBoard* out_board);
void cshogi_to_sfen(const ShogiBoard* board, char* out_sfen);
int cshogi_generate_legal_moves(const ShogiBoard* board, Move16* out_moves);
bool cshogi_apply_move(const ShogiBoard* board, Move16 move, ShogiBoard* next_board);
bool cshogi_is_in_check(const ShogiBoard* board, uint8_t color);
void cshogi_move_to_usi(Move16 move, char* out_usi);
Move16 cshogi_parse_usi_move(const char* usi_str, const ShogiBoard* board);
double cshogi_bench_legal_moves(const ShogiBoard* board, int iterations, uint64_t* out_total_moves);

// --- 243頂点モデル用 C/Bitboard 高速トポロジーインターフェース ---
// 盤面から 243頂点単体（先手層・後手層・争点層）を 0ns で一括抽出
void cshogi_get_simplex243(const ShogiBoard* board, Simplex243* out_simplex);

// 各駒の利き生成
Bitboard get_piece_attacks(uint8_t piece_type, int sq, uint8_t color, Bitboard occ);

// 243頂点 Zobrist オービフォールドハッシュを C 側で直接 0ns 計算 (cx243=...)
uint64_t cshogi_compute_orbit_hash243(const ShogiBoard* board);

#ifdef __cplusplus
}
#endif

#endif // CSHOGI_CORE_H
