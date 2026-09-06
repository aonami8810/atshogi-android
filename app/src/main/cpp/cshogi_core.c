#include "cshogi_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <x86intrin.h>
#endif

// Bitboard helpers


static inline Bitboard bb_or(Bitboard a, Bitboard b) {
    Bitboard res = { a.lo | b.lo, a.hi | b.hi };
    return res;
}

static inline Bitboard bb_and(Bitboard a, Bitboard b) {
    Bitboard res = { a.lo & b.lo, a.hi & b.hi };
    return res;
}

static inline Bitboard bb_not(Bitboard a) {
    Bitboard res = { ~a.lo, ~a.hi & 0x1FFFFULL };
    return res;
}

static inline bool bb_is_zero(Bitboard a) {
    return (a.lo == 0) && ((a.hi & 0x1FFFFULL) == 0);
}

// 駒E利きE列テーブル
static Bitboard step_attacks[2][15][81];
static uint64_t zobrist_table243[243];
static bool tables_initialized = false;



static inline bool in_bounds(int f, int r) {
    return (f >= 1 && f <= 9 && r >= 1 && r <= 9);
}

void cshogi_init_tables(void) {
    if (tables_initialized) return;
    memset(step_attacks, 0, sizeof(step_attacks));

    for (int f = 1; f <= 9; ++f) {
        for (int r = 1; r <= 9; ++r) {
            int sq = make_sq(f, r);

            // 歩
            if (r > 1) bb_set(&step_attacks[BLACK_TURN][PAWN][sq], make_sq(f, r - 1));
            if (r < 9) bb_set(&step_attacks[WHITE_TURN][PAWN][sq], make_sq(f, r + 1));

            // 桁E
            if (f > 1 && r > 2) bb_set(&step_attacks[BLACK_TURN][KNIGHT][sq], make_sq(f - 1, r - 2));
            if (f < 9 && r > 2) bb_set(&step_attacks[BLACK_TURN][KNIGHT][sq], make_sq(f + 1, r - 2));
            if (f > 1 && r < 8) bb_set(&step_attacks[WHITE_TURN][KNIGHT][sq], make_sq(f - 1, r + 2));
            if (f < 9 && r < 8) bb_set(&step_attacks[WHITE_TURN][KNIGHT][sq], make_sq(f + 1, r + 2));

            // 銀
            int b_silver_deltas[5][2] = { {0, -1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1} };
            for (int i = 0; i < 5; ++i) {
                int nf = f + b_silver_deltas[i][0];
                int nr = r + b_silver_deltas[i][1];
                if (in_bounds(nf, nr)) bb_set(&step_attacks[BLACK_TURN][SILVER][sq], make_sq(nf, nr));
            }
            int w_silver_deltas[5][2] = { {0, 1}, {-1, 1}, {1, 1}, {-1, -1}, {1, -1} };
            for (int i = 0; i < 5; ++i) {
                int nf = f + w_silver_deltas[i][0];
                int nr = r + w_silver_deltas[i][1];
                if (in_bounds(nf, nr)) bb_set(&step_attacks[WHITE_TURN][SILVER][sq], make_sq(nf, nr));
            }

            // 釁E/ 戁E
            int b_gold_deltas[6][2] = { {0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1} };
            for (int i = 0; i < 6; ++i) {
                int nf = f + b_gold_deltas[i][0];
                int nr = r + b_gold_deltas[i][1];
                if (in_bounds(nf, nr)) {
                    int nsq = make_sq(nf, nr);
                    bb_set(&step_attacks[BLACK_TURN][GOLD][sq], nsq);
                    bb_set(&step_attacks[BLACK_TURN][PRO_PAWN][sq], nsq);
                    bb_set(&step_attacks[BLACK_TURN][PRO_LANCE][sq], nsq);
                    bb_set(&step_attacks[BLACK_TURN][PRO_KNIGHT][sq], nsq);
                    bb_set(&step_attacks[BLACK_TURN][PRO_SILVER][sq], nsq);
                }
            }
            int w_gold_deltas[6][2] = { {0, 1}, {-1, 0}, {1, 0}, {0, -1}, {-1, 1}, {1, 1} };
            for (int i = 0; i < 6; ++i) {
                int nf = f + w_gold_deltas[i][0];
                int nr = r + w_gold_deltas[i][1];
                if (in_bounds(nf, nr)) {
                    int nsq = make_sq(nf, nr);
                    bb_set(&step_attacks[WHITE_TURN][GOLD][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][PRO_PAWN][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][PRO_LANCE][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][PRO_KNIGHT][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][PRO_SILVER][sq], nsq);
                }
            }

            // 玁E
            int king_deltas[8][2] = { {0,-1},{0,1},{-1,0},{1,0},{-1,-1},{1,-1},{-1,1},{1,1} };
            for (int i = 0; i < 8; ++i) {
                int nf = f + king_deltas[i][0];
                int nr = r + king_deltas[i][1];
                if (in_bounds(nf, nr)) {
                    int nsq = make_sq(nf, nr);
                    bb_set(&step_attacks[BLACK_TURN][KING][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][KING][sq], nsq);
                }
            }

            // 馬の十孁E
            int cross_deltas[4][2] = { {0,-1},{0,1},{-1,0},{1,0} };
            for (int i = 0; i < 4; ++i) {
                int nf = f + cross_deltas[i][0];
                int nr = r + cross_deltas[i][1];
                if (in_bounds(nf, nr)) {
                    int nsq = make_sq(nf, nr);
                    bb_set(&step_attacks[BLACK_TURN][HORSE][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][HORSE][sq], nsq);
                }
            }

            // 竜E斜め
            int diag_deltas[4][2] = { {-1,-1},{1,-1},{-1,1},{1,1} };
            for (int i = 0; i < 4; ++i) {
                int nf = f + diag_deltas[i][0];
                int nr = r + diag_deltas[i][1];
                if (in_bounds(nf, nr)) {
                    int nsq = make_sq(nf, nr);
                    bb_set(&step_attacks[BLACK_TURN][DRAGON][sq], nsq);
                    bb_set(&step_attacks[WHITE_TURN][DRAGON][sq], nsq);
                }
            }
        }
    }

    // 243頂点用 Zobrist ハッシュチEEブル初期匁E(ゴールチEレシオ + PRNG)
    for (int i = 0; i < 243; ++i) {
        zobrist_table243[i] = 0x9e3779b97f4a7c15ULL ^ ((uint64_t)i * 0xc6a4a7935bd1e995ULL);
    }

    tables_initialized = true;
}

static inline Bitboard get_ray_attacks(int sq, int df, int dr, Bitboard occ) {
    Bitboard attacks = {0, 0};
    int f = sq_file(sq) + df;
    int r = sq_rank(sq) + dr;
    while (in_bounds(f, r)) {
        int nsq = make_sq(f, r);
        bb_set(&attacks, nsq);
        if (bb_test(&occ, nsq)) break;
        f += df;
        r += dr;
    }
    return attacks;
}

static inline Bitboard get_sliding_attacks(uint8_t pt, int sq, uint8_t color, Bitboard occ) {
    Bitboard attacks = {0, 0};
    switch (pt) {
        case LANCE:
            if (color == BLACK_TURN) {
                attacks = get_ray_attacks(sq, 0, -1, occ);
            } else {
                attacks = get_ray_attacks(sq, 0, 1, occ);
            }
            break;
        case BISHOP:
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, 1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, 1, occ));
            break;
        case HORSE:
            attacks = step_attacks[color][HORSE][sq];
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, 1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, 1, occ));
            break;
        case ROOK:
            attacks = bb_or(attacks, get_ray_attacks(sq, 0, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 0, 1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, 0, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, 0, occ));
            break;
        case DRAGON:
            attacks = step_attacks[color][DRAGON][sq];
            attacks = bb_or(attacks, get_ray_attacks(sq, 0, -1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 0, 1, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, -1, 0, occ));
            attacks = bb_or(attacks, get_ray_attacks(sq, 1, 0, occ));
            break;
        default:
            break;
    }
    return attacks;
}

Bitboard get_piece_attacks(uint8_t pt, int sq, uint8_t color, Bitboard occ) {
    if (pt == LANCE || pt == BISHOP || pt == ROOK || pt == HORSE || pt == DRAGON) {
        return get_sliding_attacks(pt, sq, color, occ);
    }
    return step_attacks[color][pt][sq];
}

// 側ごとの全利ぁEBitboard 生E
static inline Bitboard get_side_all_attacks(const ShogiBoard* board, uint8_t color) {
    Bitboard all_att = {0, 0};
    Bitboard occ = board->occupied;
    Bitboard my_pieces = board->color_bb[color];

    for (int sq = 0; sq < 81; ++sq) {
        if (!bb_test(&my_pieces, sq)) continue;
        uint8_t pt = board->board[sq] & 0x0F;
        Bitboard att = get_piece_attacks(pt, sq, color, occ);
        all_att = bb_or(all_att, att);
    }
    return all_att;
}

// 243頂点単体！E層利き空間）E抽出
void cshogi_get_simplex243(const ShogiBoard* board, Simplex243* out_simplex) {
    cshogi_init_tables();
    Bitboard b_att = get_side_all_attacks(board, BLACK_TURN);
    Bitboard w_att = get_side_all_attacks(board, WHITE_TURN);
    Bitboard c_att = bb_and(b_att, w_att);

    out_simplex->layer_black = b_att;
    out_simplex->layer_white = w_att;
    out_simplex->layer_contested = c_att;
}

// 243頂点 Zobrist オービフォールドハチEュの高速計箁E(cx243=...)
uint64_t cshogi_compute_orbit_hash243(const ShogiBoard* board) {
    Simplex243 s;
    cshogi_get_simplex243(board, &s);

    uint64_t h = 0;
    // Layer 0: 0..80
    for (int i = 0; i < 81; ++i) {
        if (bb_test(&s.layer_black, i)) h ^= zobrist_table243[i];
    }
    // Layer 1: 81..161
    for (int i = 0; i < 81; ++i) {
        if (bb_test(&s.layer_white, i)) h ^= zobrist_table243[81 + i];
    }
    // Layer 2: 162..242
    for (int i = 0; i < 81; ++i) {
        if (bb_test(&s.layer_contested, i)) h ^= zobrist_table243[162 + i];
    }
    return h;
}

static inline bool is_square_attacked_by(const ShogiBoard* board, int sq, uint8_t attacker_color) {
    Bitboard occ = board->occupied;
    for (int pt = 1; pt <= 14; ++pt) {
        Bitboard attackers = board->piece_bb[attacker_color][pt];
        if (bb_is_zero(attackers)) continue;

        Bitboard target_mask;
        if (pt == LANCE) {
            uint8_t defender_color = 1 - attacker_color;
            target_mask = get_piece_attacks(LANCE, sq, defender_color, occ);
        } else if (pt == PAWN || pt == KNIGHT || pt == SILVER || pt == GOLD || pt == PRO_PAWN || pt == PRO_LANCE || pt == PRO_KNIGHT || pt == PRO_SILVER) {
            uint8_t defender_color = 1 - attacker_color;
            target_mask = step_attacks[defender_color][pt][sq];
        } else {
            target_mask = get_piece_attacks(pt, sq, attacker_color, occ);
        }

        if (!bb_is_zero(bb_and(attackers, target_mask))) {
            return true;
        }
    }
    return false;
}

bool cshogi_is_in_check(const ShogiBoard* board, uint8_t color) {
    int ksq = board->king_sq[color];
    if (ksq < 0 || ksq >= 81) return false;
    return is_square_attacked_by(board, ksq, 1 - color);
}

static uint8_t char_to_pt(char c) {
    switch (c) {
        case 'p': case 'P': return PAWN;
        case 'l': case 'L': return LANCE;
        case 'n': case 'N': return KNIGHT;
        case 's': case 'S': return SILVER;
        case 'g': case 'G': return GOLD;
        case 'b': case 'B': return BISHOP;
        case 'r': case 'R': return ROOK;
        case 'k': case 'K': return KING;
        default: return EMPTY;
    }
}


void cshogi_to_sfen(const ShogiBoard* board, char* out_sfen) {
    if (!board || !out_sfen) return;
    int idx = 0;
    for (int r = 0; r < 9; ++r) {
        int empty = 0;
        for (int f = 8; f >= 0; --f) {
            int pt = board->board[r * 9 + f];
            if (pt == 0) {
                empty++;
            } else {
                if (empty > 0) {
                    out_sfen[idx++] = '0' + empty;
                    empty = 0;
                }
                int type = pt & 15;
                int is_white = pt & 16;
                int is_promoted = pt & 32;
                if (is_promoted) out_sfen[idx++] = '+';
                char c;
                switch (type) {
                    case 1: c = 'P'; break;
                    case 2: c = 'L'; break;
                    case 3: c = 'N'; break;
                    case 4: c = 'S'; break;
                    case 5: c = 'B'; break;
                    case 6: c = 'R'; break;
                    case 7: c = 'G'; break;
                    case 8: c = 'K'; break;
                    default: c = '?'; break;
                }
                if (is_white) c = c - 'A' + 'a';
                out_sfen[idx++] = c;
            }
        }
        if (empty > 0) {
            out_sfen[idx++] = '0' + empty;
        }
        if (r < 8) out_sfen[idx++] = '/';
    }
    
    out_sfen[idx++] = ' ';
    out_sfen[idx++] = board->turn == 0 ? 'b' : 'w';
    out_sfen[idx++] = ' ';
    
    int has_hand = 0;
    const char* p_names = "PLNSBRG";
    for(int c = 0; c < 2; ++c) {
        for(int t = 6; t >= 0; --t) { // R, B, G, S, N, L, P
            uint8_t* ptr = (uint8_t*)&board->hands[c]; int count = ptr[t];; // type is 1..7
            if (count > 0) {
                has_hand = 1;
                if (count > 1) {
                    if (count >= 10) out_sfen[idx++] = '0' + (count / 10);
                    out_sfen[idx++] = '0' + (count % 10);
                }
                char piece_char = p_names[t];
                if (c == 1) piece_char = piece_char - 'A' + 'a';
                out_sfen[idx++] = piece_char;
            }
        }
    }
    if (!has_hand) out_sfen[idx++] = '-';
    
    out_sfen[idx++] = ' ';
    out_sfen[idx++] = '1';
    out_sfen[idx++] = '\0';
}


bool cshogi_from_sfen(const char* sfen, ShogiBoard* board) {
    cshogi_init_tables();
    memset(board, 0, sizeof(ShogiBoard));

    if (strcmp(sfen, "startpos") == 0) {
        sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    }

    const char* p = sfen;
    int f = 9, r = 1;

    while (*p && *p != ' ') {
        if (*p == '/') {
            r++;
            f = 9;
            p++;
        } else if (*p >= '1' && *p <= '9') {
            f -= (*p - '0');
            p++;
        } else {
            bool is_prom = false;
            if (*p == '+') {
                is_prom = true;
                p++;
            }
            char c = *p++;
            uint8_t color = (c >= 'A' && c <= 'Z') ? BLACK_TURN : WHITE_TURN;
            uint8_t pt = char_to_pt(c);
            if (is_prom) {
                if (pt == PAWN) pt = PRO_PAWN;
                else if (pt == LANCE) pt = PRO_LANCE;
                else if (pt == KNIGHT) pt = PRO_KNIGHT;
                else if (pt == SILVER) pt = PRO_SILVER;
                else if (pt == BISHOP) pt = HORSE;
                else if (pt == ROOK) pt = DRAGON;
            }

            int sq = make_sq(f, r);
            board->board[sq] = (color << 4) | pt;
            bb_set(&board->piece_bb[color][pt], sq);
            bb_set(&board->color_bb[color], sq);
            bb_set(&board->occupied, sq);

            if (pt == KING) {
                board->king_sq[color] = sq;
            }
            f--;
        }
    }

    while (*p == ' ') p++;
    if (*p == 'b' || *p == 'B') board->turn = BLACK_TURN;
    else if (*p == 'w' || *p == 'W') board->turn = WHITE_TURN;
    if (*p) p++;

    while (*p == ' ') p++;
    if (*p == '-') {
        p++;
    } else {
        int count = 1;
        while (*p && *p != ' ') {
            if (*p >= '0' && *p <= '9') {
                count = strtol(p, (char**)&p, 10);
            } else {
                char c = *p++;
                uint8_t color = (c >= 'A' && c <= 'Z') ? BLACK_TURN : WHITE_TURN;
                uint8_t pt = char_to_pt(c);
                HandPieces* h = &board->hands[color];
                if (pt == PAWN) h->pawn += count;
                else if (pt == LANCE) h->lance += count;
                else if (pt == KNIGHT) h->knight += count;
                else if (pt == SILVER) h->silver += count;
                else if (pt == GOLD) h->gold += count;
                else if (pt == BISHOP) h->bishop += count;
                else if (pt == ROOK) h->rook += count;
                count = 1;
            }
        }
    }
    return true;
}

static inline void apply_move_internal(const ShogiBoard* src, Move16 move, ShogiBoard* dst) {
    memcpy(dst, src, sizeof(ShogiBoard));
    uint8_t color = src->turn;
    uint8_t opp = 1 - color;
    dst->turn = opp;

    if (move & MOVE_DROP_FLAG) {
        uint8_t pt = move & 0x7F;
        int to_sq = (move >> 7) & 0x7F;

        dst->board[to_sq] = (color << 4) | pt;
        bb_set(&dst->piece_bb[color][pt], to_sq);
        bb_set(&dst->color_bb[color], to_sq);
        bb_set(&dst->occupied, to_sq);

        HandPieces* h = &dst->hands[color];
        if (pt == PAWN) h->pawn--;
        else if (pt == LANCE) h->lance--;
        else if (pt == KNIGHT) h->knight--;
        else if (pt == SILVER) h->silver--;
        else if (pt == GOLD) h->gold--;
        else if (pt == BISHOP) h->bishop--;
        else if (pt == ROOK) h->rook--;
    } else {
        int from_sq = move & 0x7F;
        int to_sq = (move >> 7) & 0x7F;
        bool is_prom = (move & MOVE_PROMOTE_FLAG) != 0;

        uint8_t piece_val = src->board[from_sq];
        uint8_t pt = piece_val & 0x0F;

        dst->board[from_sq] = EMPTY;
        bb_clear(&dst->piece_bb[color][pt], from_sq);
        bb_clear(&dst->color_bb[color], from_sq);
        bb_clear(&dst->occupied, from_sq);

        uint8_t cap_val = src->board[to_sq];
        if (cap_val != EMPTY) {
            uint8_t cap_pt = cap_val & 0x0F;
            bb_clear(&dst->piece_bb[opp][cap_pt], to_sq);
            bb_clear(&dst->color_bb[opp], to_sq);

            uint8_t unprom_pt = cap_pt;
            if (cap_pt == PRO_PAWN) unprom_pt = PAWN;
            else if (cap_pt == PRO_LANCE) unprom_pt = LANCE;
            else if (cap_pt == PRO_KNIGHT) unprom_pt = KNIGHT;
            else if (cap_pt == PRO_SILVER) unprom_pt = SILVER;
            else if (cap_pt == HORSE) unprom_pt = BISHOP;
            else if (cap_pt == DRAGON) unprom_pt = ROOK;

            HandPieces* h = &dst->hands[color];
            if (unprom_pt == PAWN) h->pawn++;
            else if (unprom_pt == LANCE) h->lance++;
            else if (unprom_pt == KNIGHT) h->knight++;
            else if (unprom_pt == SILVER) h->silver++;
            else if (unprom_pt == GOLD) h->gold++;
            else if (unprom_pt == BISHOP) h->bishop++;
            else if (unprom_pt == ROOK) h->rook++;
        }

        uint8_t dst_pt = pt;
        if (is_prom) {
            if (pt == PAWN) dst_pt = PRO_PAWN;
            else if (pt == LANCE) dst_pt = PRO_LANCE;
            else if (pt == KNIGHT) dst_pt = PRO_KNIGHT;
            else if (pt == SILVER) dst_pt = PRO_SILVER;
            else if (pt == BISHOP) dst_pt = HORSE;
            else if (pt == ROOK) dst_pt = DRAGON;
        }

        dst->board[to_sq] = (color << 4) | dst_pt;
        bb_set(&dst->piece_bb[color][dst_pt], to_sq);
        bb_set(&dst->color_bb[color], to_sq);
        bb_set(&dst->occupied, to_sq);

        if (pt == KING) {
            dst->king_sq[color] = to_sq;
        }
    }
}

bool cshogi_apply_move(const ShogiBoard* board, Move16 move, ShogiBoard* next_board) {
    apply_move_internal(board, move, next_board);
    if (cshogi_is_in_check(next_board, board->turn)) {
        return false;
    }
    return true;
}

int cshogi_generate_legal_moves(const ShogiBoard* board, Move16* out_moves) {
    cshogi_init_tables();
    int count = 0;
    uint8_t color = board->turn;
    Bitboard occ = board->occupied;
    Bitboard my_pieces = board->color_bb[color];
    Bitboard empty_sqs = bb_not(occ);

    for (int from_sq = 0; from_sq < 81; ++from_sq) {
        if (!bb_test(&my_pieces, from_sq)) continue;

        uint8_t pt = board->board[from_sq] & 0x0F;
        Bitboard attacks = get_piece_attacks(pt, from_sq, color, occ);
        Bitboard valid_dest = bb_and(attacks, bb_not(my_pieces));

        int from_r = sq_rank(from_sq);

        for (int to_sq = 0; to_sq < 81; ++to_sq) {
            if (!bb_test(&valid_dest, to_sq)) continue;

            int to_r = sq_rank(to_sq);
            bool can_prom = false;
            bool must_prom = false;

            if (color == BLACK_TURN) {
                can_prom = (from_r <= 3 || to_r <= 3);
                if (pt == PAWN || pt == LANCE) must_prom = (to_r == 1);
                else if (pt == KNIGHT) must_prom = (to_r <= 2);
            } else {
                can_prom = (from_r >= 7 || to_r >= 7);
                if (pt == PAWN || pt == LANCE) must_prom = (to_r == 9);
                else if (pt == KNIGHT) must_prom = (to_r >= 8);
            }

            bool is_promotable_piece = (pt == PAWN || pt == LANCE || pt == KNIGHT || pt == SILVER || pt == BISHOP || pt == ROOK);

            if (is_promotable_piece && can_prom) {
                Move16 mv_prom = MOVE_PROMOTE_FLAG | (to_sq << 7) | from_sq;
                ShogiBoard test_board;
                if (cshogi_apply_move(board, mv_prom, &test_board)) {
                    out_moves[count++] = mv_prom;
                }
            }

            if (!must_prom) {
                Move16 mv_norm = (to_sq << 7) | from_sq;
                ShogiBoard test_board;
                if (cshogi_apply_move(board, mv_norm, &test_board)) {
                    out_moves[count++] = mv_norm;
                }
            }
        }
    }

    const HandPieces* h = &board->hands[color];
    uint8_t drop_pts[7] = { PAWN, LANCE, KNIGHT, SILVER, GOLD, BISHOP, ROOK };
    uint8_t drop_counts[7] = { h->pawn, h->lance, h->knight, h->silver, h->gold, h->bishop, h->rook };

    for (int i = 0; i < 7; ++i) {
        if (drop_counts[i] == 0) continue;
        uint8_t pt = drop_pts[i];

        for (int to_sq = 0; to_sq < 81; ++to_sq) {
            if (!bb_test(&empty_sqs, to_sq)) continue;

            int to_f = sq_file(to_sq);
            int to_r = sq_rank(to_sq);

            if (color == BLACK_TURN) {
                if ((pt == PAWN || pt == LANCE) && to_r == 1) continue;
                if (pt == KNIGHT && to_r <= 2) continue;
            } else {
                if ((pt == PAWN || pt == LANCE) && to_r == 9) continue;
                if (pt == KNIGHT && to_r >= 8) continue;
            }

            if (pt == PAWN) {
                bool has_pawn = false;
                for (int r = 1; r <= 9; ++r) {
                    int check_sq = make_sq(to_f, r);
                    if (bb_test(&board->piece_bb[color][PAWN], check_sq)) {
                        has_pawn = true;
                        break;
                    }
                }
                if (has_pawn) continue;
            }

            Move16 mv_drop = MOVE_DROP_FLAG | (to_sq << 7) | pt;
            ShogiBoard test_board;
            if (cshogi_apply_move(board, mv_drop, &test_board)) {
                if (pt == PAWN) {
                    uint8_t opp = 1 - color;
                    if (cshogi_is_in_check(&test_board, opp)) {
                        Move16 opp_moves[600];
                        int opp_move_count = cshogi_generate_legal_moves(&test_board, opp_moves);
                        if (opp_move_count == 0) {
                            continue;
                        }
                    }
                }
                out_moves[count++] = mv_drop;
            }
        }
    }

    return count;
}

void cshogi_move_to_usi(Move16 move, char* out_usi) {
    int to_sq = (move >> 7) & 0x7F;
    int to_f = sq_file(to_sq);
    int to_r = sq_rank(to_sq);

    if (move & MOVE_DROP_FLAG) {
        uint8_t pt = move & 0x7F;
        char pt_char = '?';
        switch (pt) {
            case PAWN: pt_char = 'P'; break;
            case LANCE: pt_char = 'L'; break;
            case KNIGHT: pt_char = 'N'; break;
            case SILVER: pt_char = 'S'; break;
            case GOLD: pt_char = 'G'; break;
            case BISHOP: pt_char = 'B'; break;
            case ROOK: pt_char = 'R'; break;
        }
        sprintf(out_usi, "%c*%d%c", pt_char, to_f, 'a' + to_r - 1);
    } else {
        int from_sq = move & 0x7F;
        int from_f = sq_file(from_sq);
        int from_r = sq_rank(from_sq);
        bool is_prom = (move & MOVE_PROMOTE_FLAG) != 0;

        if (is_prom) {
            sprintf(out_usi, "%d%c%d%c+", from_f, 'a' + from_r - 1, to_f, 'a' + to_r - 1);
        } else {
            sprintf(out_usi, "%d%c%d%c", from_f, 'a' + from_r - 1, to_f, 'a' + to_r - 1);
        }
    }
}

Move16 cshogi_parse_usi_move(const char* usi_str, const ShogiBoard* board) {
    if (!usi_str || strlen(usi_str) < 4) return 0;

    if (usi_str[1] == '*') {
        char pt_char = usi_str[0];
        uint8_t pt = char_to_pt(pt_char);
        int to_f = usi_str[2] - '0';
        int to_r = usi_str[3] - 'a' + 1;
        int to_sq = make_sq(to_f, to_r);
        return (Move16)(MOVE_DROP_FLAG | (to_sq << 7) | pt);
    } else {
        int from_f = usi_str[0] - '0';
        int from_r = usi_str[1] - 'a' + 1;
        int to_f = usi_str[2] - '0';
        int to_r = usi_str[3] - 'a' + 1;
        int from_sq = make_sq(from_f, from_r);
        int to_sq = make_sq(to_f, to_r);
        bool is_prom = (usi_str[4] == '+');

        Move16 mv = (to_sq << 7) | from_sq;
        if (is_prom) mv |= MOVE_PROMOTE_FLAG;
        return mv;
    }
}

double cshogi_bench_legal_moves(const ShogiBoard* board, int iterations, uint64_t* out_total_moves) {
    Move16 move_buf[600];
    uint64_t total = 0;
    
    clock_t start = clock();
    for (int i = 0; i < iterations; ++i) {
        int n = cshogi_generate_legal_moves(board, move_buf);
        total += n;
    }
    clock_t end = clock();
    
    if (out_total_moves) *out_total_moves = total;
    return (double)(end - start) / CLOCKS_PER_SEC;
}


int gpuVramMateLookup(void* x) { return 0; }
void runMirrorBatchOpenCL(void* x, void* y, void* z, void* w, void* q) { }

#ifdef __cplusplus
extern "C" {
#endif

void* atshogi_position_create() { ShogiBoard* b = (ShogiBoard*)calloc(1, sizeof(ShogiBoard)); return b; }
void atshogi_position_free(void* handle) { free(handle); }
void atshogi_position_set_startpos(void* handle) { if(handle) cshogi_from_sfen("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", (ShogiBoard*)handle); }
void atshogi_position_set_sfen(void* handle, const char* sfen) { if(handle) cshogi_from_sfen(sfen, (ShogiBoard*)handle); }
void atshogi_position_apply_move(void* handle, const char* move_usi) { if(handle) { Move16 m = cshogi_parse_usi_move(move_usi, (ShogiBoard*)handle); ShogiBoard next_b; if(cshogi_apply_move((ShogiBoard*)handle, m, &next_b)) *(ShogiBoard*)handle = next_b; } }
int atshogi_position_get_turn(void* handle) { return handle ? ((ShogiBoard*)handle)->turn : 0; }

extern void* load_topological_tensor_mmap();
extern void encode_sfen_to_tensor_state(void* mmap_ptr, const char* sfen);
extern const int* get_piece_indices(void* mmap_ptr);
extern float atshogi_mera_evaluate_default_k40(const int* indices);
extern void free_topological_tensor_mmap(void* mmap_ptr);


static inline void cshogi_compute_hash128(const ShogiBoard* board, Simplex81* out_hash) {
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
static void cshogi_get_piece_indices_from_board(const ShogiBoard* board, int32_t indices[40]) {
    for (int i = 0; i < 40; i++) indices[i] = 81; // 81 means captured
    int p_b = 0, p_w = 9, l_b = 18, l_w = 20, n_b = 22, n_w = 24;
    int s_b = 26, s_w = 28, g_b = 30, g_w = 32, b_b = 34, b_w = 35;
    int r_b = 36, r_w = 37, k_b = 38, k_w = 39;

    for (int sq = 0; sq < 81; sq++) {
        int pt = board->board[sq];
        if (pt == 0) continue;
        int type = pt & 15;
        int is_white = pt & 16;
        int idx = -1;
        switch(type) {
            case 1: idx = is_white ? p_w++ : p_b++; break; // P
            case 2: idx = is_white ? l_w++ : l_b++; break; // L
            case 3: idx = is_white ? n_w++ : n_b++; break; // N
            case 4: idx = is_white ? s_w++ : s_b++; break; // S
            case 5: idx = is_white ? b_w++ : b_b++; break; // B
            case 6: idx = is_white ? r_w++ : r_b++; break; // R
            case 7: idx = is_white ? g_w++ : g_b++; break; // G
            case 8: idx = is_white ? k_w++ : k_b++; break; // K
        }
        if (idx >= 0 && idx < 40) {
            indices[idx] = sq | (type << 7) | (is_white ? (1 << 11) : 0);
        }
    }
}

struct LocalTensorContextSoA {
    float potentials[16][81];
    float bond_weights[16][81];
};

extern void solve_laplace_iteration_step_dynamic(
    float* potentials,
    const uint8_t* obstacle_mask,
    int sente_king_idx,
    int gote_king_idx,
    float mass_term_m2
);

#include <math.h>

float evaluate_omni_tensor(const ShogiBoard* board, const struct LocalTensorContextSoA* tensor) {
    float score = 0.0f;
    int sente_king_sq = 44;
    int gote_king_sq = 36;

    // 1. 玉の位置を特定
    for (int sq = 0; sq < 81; sq++) {
        uint8_t p = board->board[sq];
        if (p != 0) {
            int pt = p & 0xF;
            int col = (p >> 4) & 1;
            if (pt == 8) { // cshogi KING
                if (col == 0) sente_king_sq = sq;
                else gote_king_sq = sq;
            }
        }
    }

    int sx = sente_king_sq / 9, sy = sente_king_sq % 9;
    int gx = gote_king_sq / 9, gy = gote_king_sq % 9;

    // 2. 多粒子多重電位場（Multi-Particle Multi-Potential Field）の適用
    for (int sq = 0; sq < 81; sq++) {
        uint8_t p = board->board[sq];
        if (p != 0) {
            int pt = p & 0xF;
            int col = (p >> 4) & 1;
            
            // 基礎電荷 q_p は先手(+1), 後手(-1)で完全に固定
            float q_p = (col == 0) ? 1.0f : -1.0f;
            
            // 動的な役割比率 theta_p (起点 u の駒種のみに依存するため移動中不変 -> 自己推進バグ完全消滅)
            float theta = 0.5f;
            switch(pt) {
                case 8: theta = 0.0f; break; // 王 (完全防衛)
                case 7: theta = 0.2f; break; // 金 (防衛寄り)
                case 4: theta = 0.2f; break; // 銀 (防衛寄り)
                case 12: theta = 0.2f; break; // 成銀 (防衛寄り)
                case 6: theta = 1.0f; break; // 飛 (完全攻撃)
                case 14: theta = 1.0f; break; // 竜 (完全攻撃)
                case 5: theta = 1.0f; break; // 角 (完全攻撃)
                case 13: theta = 1.0f; break; // 馬 (完全攻撃)
                case 3: theta = 0.9f; break; // 桂 (攻撃)
                case 11: theta = 0.9f; break; // 成桂 (攻撃)
                case 2: theta = 0.9f; break; // 香 (攻撃)
                case 10: theta = 0.9f; break; // 成香 (攻撃)
                case 1: theta = 0.8f; break; // 歩 (攻撃寄り)
                case 9: theta = 0.8f; break; // と金 (攻撃寄り)
            }

            // 空間のハッセ距離近似（チェビシェフ距離）
            int x = sq / 9, y = sq % 9;
            int dx_s = abs(x - sx), dy_s = abs(y - sy);
            int d_S = (dx_s > dy_s) ? dx_s : dy_s;
            
            int dx_g = abs(x - gx), dy_g = abs(y - gy);
            int d_G = (dx_g > dy_g) ? dx_g : dy_g;

            // 自玉基準の防衛電位場 T_Defense と 敵玉基準の攻撃電位場 T_Attack
            // 距離が近いほど電位が高い（引力）
            float t_defense = - (float)((col == 0) ? d_S : d_G) * 10.0f;
            float t_attack  = - (float)((col == 0) ? d_G : d_S) * 10.0f;

            // 完備テンソル T_omni(v)
            float t_omni = tensor->potentials[pt][sq];
            
            // 独立スカラー場の線形結合による総合ポテンシャル
            float field = (1.0f - theta) * t_defense + theta * t_attack + t_omni;
            
            // 離散外微分 ΔΦ = q_p * ( field(v) - field(u) ) 
            // 評価関数としては直接状態 field をスコアに加算・減算すればよい
            if (col == 1) score -= field;
            else score += field;
        }
    }

    return score;
}

void atshogi_select_best_move_k40_mmap(void* handle, char* out_move_usi, int max_len, const void* mmap_ptr, size_t mmap_size) {
    if(!handle || !out_move_usi) return; 
    ShogiBoard* board = (ShogiBoard*)handle;
    
    const struct LocalTensorContextSoA* tensor = (const struct LocalTensorContextSoA*)mmap_ptr;
    
    Move16 legal_moves[600]; 
    int count = cshogi_generate_legal_moves(board, legal_moves); 
    if(count == 0) { 
        strncpy(out_move_usi, "resign", max_len); 
        return; 
    }
    
    int current_turn = board->turn; // 0 for Black, 1 for White
    float best_eval = -1e9f;
    int best_move_idx = 0;
    
    for (int i = 0; i < count; ++i) {
        ShogiBoard next_b;
        if (!cshogi_apply_move(board, legal_moves[i], &next_b)) continue;
        
        float val = 0.0f;
        if (tensor && mmap_size >= sizeof(struct LocalTensorContextSoA)) {
            val = evaluate_omni_tensor(&next_b, tensor);
            if (current_turn == 1) val = -val;
        } else {
            int32_t indices[40];
            cshogi_get_piece_indices_from_board(&next_b, indices);
            val = atshogi_mera_evaluate_default_k40(indices);
            if (current_turn == 1) val = -val;
            fprintf(stderr, "FALLBACK! mmap_size=%zu, sizeof=%zu, tensor=%p\n", mmap_size, sizeof(struct LocalTensorContextSoA), (void*)tensor);
        }

        char move_str[16];
        cshogi_move_to_usi(legal_moves[i], move_str);
        fprintf(stderr, "Move %d (%s): val=%f\n", i, move_str, val);

        if (val > best_eval) {
            best_eval = val;
            best_move_idx = i;
        }
    }
    char usi_str[16];
    cshogi_move_to_usi(legal_moves[best_move_idx], usi_str);
    snprintf(out_move_usi, max_len, "%s %f", usi_str, best_eval);
}

size_t get_topological_tensor_mmap_size(); // Forward declaration

void atshogi_select_best_move_k40(void* handle, char* out_move_usi, int max_len, const AtlasNode* atlas, int atlas_count) {
    if (atlas && atlas_count == 0) {
        atshogi_select_best_move_k40_mmap(handle, out_move_usi, max_len, atlas, get_topological_tensor_mmap_size());
    } else {
        atshogi_select_best_move_k40_mmap(handle, out_move_usi, max_len, atlas, atlas_count * sizeof(AtlasNode));
    }
}


#ifdef __cplusplus
}
#endif




extern void* atshogi_mps_create_from_file(const char* filepath);
extern float atshogi_mps_evaluate_7site(const void* modelPtr, const int32_t indices[7]);

static void* global_mps_model = NULL;

void atshogi_init_mps() {
    if (!global_mps_model) {
        global_mps_model = atshogi_mps_create_from_file("egtb.atmp");
    }
}

static void extract_7site_indices(const ShogiBoard* board, int32_t indices[7]) {
    for(int i=0; i<7; ++i) indices[i] = 0;
    
    int s_king = -1, g_king = -1;
    for(int i=0; i<81; ++i) {
        uint8_t p = board->board[i];
        if (p == 0) continue;
        uint8_t pt = p & 0x0F;
        uint8_t col = p >> 4;
        if(pt == KING) {
            if(col == BLACK_TURN) s_king = i;
            if(col == WHITE_TURN) g_king = i;
        }
    }
    
    // code: sq(7bits) | pt(4bits) | col(1bit)
    if(s_king >= 0) indices[0] = s_king | (KING << 7) | (BLACK_TURN << 11);
    if(g_king >= 0) indices[1] = g_king | (KING << 7) | (WHITE_TURN << 11);
    
    int idx = 2;
    for(int i=0; i<81 && idx < 7; ++i) {
        uint8_t p = board->board[i];
        if(p == 0) continue;
        uint8_t pt = p & 0x0F;
        uint8_t col = p >> 4;
        if(pt == ROOK || pt == BISHOP || pt == GOLD || pt == DRAGON || pt == HORSE) {
            indices[idx++] = i | (pt << 7) | (col << 11);
        }
    }
    while(idx < 7) {
        indices[idx++] = 0;
    }
}

void atshogi_generate_dynamic_refutation_k40(void* handle, AtlasNode* out_node) {
    if(out_node) memset(out_node, 0, sizeof(AtlasNode));
    if(!handle || !out_node) return;
    fprintf(stderr, "INIT MPS`n"); fprintf(stderr, "atshogi_init_mps CALL`n"); atshogi_init_mps(); fprintf(stderr, "atshogi_init_mps DONE`n"); fprintf(stderr, "MPS DONE`n");
    if(!global_mps_model) {
        return; 
    }
    
    ShogiBoard* board = (ShogiBoard*)handle;
    Move16 legal_moves[600];
    int num_moves = cshogi_generate_legal_moves(board, legal_moves);
    if(num_moves == 0) {
        return;
    }
    
    float best_score = (board->turn == BLACK_TURN) ? -1000000.0f : 1000000.0f;
    Move16 best_move = 0;
    
    for(int i=0; i<num_moves; ++i) {
        ShogiBoard next_board;
        cshogi_apply_move(board, legal_moves[i], &next_board);
        
        int32_t indices[7];
        extract_7site_indices(&next_board, indices); fprintf(stderr, "EVAL`n");
        
        float score = atshogi_mps_evaluate_7site(global_mps_model, indices);
        
        if (board->turn == BLACK_TURN) {
            if (score > best_score) {
                best_score = score;
                best_move = legal_moves[i];
            }
        } else {
            if (score < best_score) {
                best_score = score;
                best_move = legal_moves[i];
            }
        }
    }
    
    if(best_move != 0) {
        cshogi_compute_hash128(board, &out_node->state_simplex);
        out_node->best_move.mask_lo = (uint64_t)best_move;
        out_node->best_move.mask_hi = 0;
        out_node->morse_value = best_score;
        out_node->isotropy_mask = 1;
    }
}




