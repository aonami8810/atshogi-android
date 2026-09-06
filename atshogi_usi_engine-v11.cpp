#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <float.h>
#include <stdint.h>
#include <random>
#include <cmath>
#include <cstdlib>

// ============================================================================
// 1. å°æ£ç¤ããã³é§ã®å®ç¾©
// ============================================================================
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
    PROMOTED_PAWN = 9,
    PROMOTED_LANCE = 10,
    PROMOTED_KNIGHT = 11,
    PROMOTED_SILVER = 12,
    PROMOTED_BISHOP = 13,
    PROMOTED_ROOK = 14
};

enum Color {
    BLACK = 0,
    WHITE = 1
};

const uint8_t WHITE_FLAG = 16;
const uint8_t PIECE_MASK = 15;

// USIåº§æ¨è¡¨è¨ â¡ ç¤é¢ã¤ã³ããã¯ã¹ sq = y * 9 + x (x: 0 to 8, y: 0 to 8)
inline int get_sq(int x, int y) {
    return y * 9 + x;
}

// ============================================================================
// 2. æãæã®å®ç¾©
// ============================================================================
struct Move {
    int from_sq; // ç¤ä¸ç§»ååãæã¡é§ããã®ãã­ããã¯ -1
    int to_sq;
    bool promote;
    PieceType drop_piece; // æã¡é§ã®ç¨®é¡ããã­ããããªãå ´åã¯ EMPTY
    
    std::string to_usi() const {
        if (from_sq == -1) {
            char p_char = ' ';
            switch (drop_piece) {
                case PAWN: p_char = 'P'; break;
                case LANCE: p_char = 'L'; break;
                case KNIGHT: p_char = 'N'; break;
                case SILVER: p_char = 'S'; break;
                case GOLD: p_char = 'G'; break;
                case BISHOP: p_char = 'B'; break;
                case ROOK: p_char = 'R'; break;
                default: break;
            }
            int tx = to_sq % 9;
            int ty = to_sq / 9;
            char cx = '9' - tx;
            char cy = 'a' + ty;
            return std::string(1, p_char) + "*" + cx + cy;
        } else {
            int fx = from_sq % 9;
            int fy = from_sq / 9;
            int tx = to_sq % 9;
            int ty = to_sq / 9;
            char cfx = '9' - fx;
            char cfy = 'a' + fy;
            char ctx = '9' - tx;
            char cty = 'a' + ty;
            std::string res = "";
            res += cfx;
            res += cfy;
            res += ctx;
            res += cty;
            if (promote) res += "+";
            return res;
        }
    }
};

// ============================================================================
// 3. æ±ºå®è«ç Zobrist Hashing ã·ã¹ãã 
// ============================================================================
struct ZobristTable {
    uint64_t piece_table[81][32];
    uint64_t hand_table[2][8][19];
    uint64_t side_hash;
    
    ZobristTable() {
        std::mt19937_64 rng(54321);
        for (int sq = 0; sq < 81; ++sq) {
            for (int p = 0; p < 32; ++p) {
                piece_table[sq][p] = rng();
            }
        }
        for (int color = 0; color < 2; ++color) {
            for (int pc = 0; pc < 8; ++pc) {
                for (int count = 0; count < 19; ++count) {
                    hand_table[color][pc][count] = rng();
                } 
            }
        }
        side_hash = rng();
    }
};

static ZobristTable zobrist;

// ============================================================================
// 4. å°æ£ç¤é¢ç®¡ç (Board) ã¯ã©ã¹ (cshogi ã¨ãã»ã³ã¹ç§»æ¤ç)
// ============================================================================
struct BoardState {
    uint8_t board[81];
    uint8_t hand[2][8];
    Color side_to_move;
    uint64_t hash;
};

class Board {
public: 
    uint8_t board[81];
    uint8_t hand[2][8];
    Color side_to_move;
    uint64_t current_hash;
    std::vector<BoardState> history;

    Board() {
        clear();
    }

    void clear() {
        std::fill(board, board + 81, (uint8_t)EMPTY);
        std::fill(&hand[0][0], &hand[0][0] + 2 * 8, (uint8_t)0);
        side_to_move = BLACK;
        current_hash = 0;
        history.clear();
    }

    uint64_t compute_hash() const {
        uint64_t h = 0;
        for (int sq = 0; sq < 81; ++sq) {
            if (board[sq] != EMPTY) {
                h ^= zobrist.piece_table[sq][board[sq]];
            }
        }
        for (int col = 0; col < 2; ++col) {
            for (int pc = 1; pc <= 7; ++pc) {
                int count = hand[col][pc];
                if (count > 0 && count < 19) {
                    h ^= zobrist.hand_table[col][pc][count];
                }
            }
        }
        if (side_to_move == WHITE) {
            h ^= zobrist.side_hash;
        }
        return h;
    }

    void update_hash() {
        current_hash = compute_hash();
    }

    void set_startpos() {
        clear();
        // 1æ®µç® (å¾æ)
        board[get_sq(0, 0)] = LANCE | WHITE_FLAG;
        board[get_sq(1, 0)] = KNIGHT | WHITE_FLAG;
        board[get_sq(2, 0)] = SILVER | WHITE_FLAG;
        board[get_sq(3, 0)] = GOLD | WHITE_FLAG;
        board[get_sq(4, 0)] = KING | WHITE_FLAG;
        board[get_sq(5, 0)] = GOLD | WHITE_FLAG;
        board[get_sq(6, 0)] = SILVER | WHITE_FLAG;
        board[get_sq(7, 0)] = KNIGHT | WHITE_FLAG;
        board[get_sq(8, 0)] = LANCE | WHITE_FLAG;
        
        board[get_sq(1, 1)] = ROOK | WHITE_FLAG;
        board[get_sq(7, 1)] = BISHOP | WHITE_FLAG;
        
        for (int x = 0; x < 9; ++x) {
            board[get_sq(x, 2)] = PAWN | WHITE_FLAG;
        }
        
        // 7æ®µç® (åæ)
        for (int x = 0; x < 9; ++x) {
            board[get_sq(x, 6)] = PAWN;
        }
        
        board[get_sq(1, 7)] = BISHOP;
        board[get_sq(7, 7)] = ROOK;
        
        // 9æ®µç® (åæ)
        board[get_sq(0, 8)] = LANCE;
        board[get_sq(1, 8)] = KNIGHT;
        board[get_sq(2, 8)] = SILVER;
        board[get_sq(3, 8)] = GOLD;
        board[get_sq(4, 8)] = KING;
        board[get_sq(5, 8)] = GOLD;
        board[get_sq(6, 8)] = SILVER;
        board[get_sq(7, 8)] = KNIGHT;
        board[get_sq(8, 8)] = LANCE;
        
        side_to_move = BLACK;
        update_hash();
    }

    void set_sfen(const std::string& sfen_str) {
        clear();
        std::stringstream ss(sfen_str);
        std::string board_part, color_part, hand_part, move_count_part;
        ss >> board_part >> color_part >> hand_part >> move_count_part;
        
        int x = 0, y = 0;
        bool promote_next = false;
        for (char c : board_part) {
            if (c == '/') {
                x = 0;
                y++;
            } else if (c >= '1' && c <= '9') {
                x += (c - '0');
            } else {
                bool is_white = (c >= 'a' && c <= 'z');
                char uc = is_white ? (c - 'a' + 'A') : c;
                PieceType pt = EMPTY;
                switch (uc) {
                    case 'P': pt = PAWN; break;
                    case 'L': pt = LANCE; break;
                    case 'N': pt = KNIGHT; break;
                    case 'S': pt = SILVER; break;
                    case 'G': pt = GOLD; break;
                    case 'B': pt = BISHOP; break;
                    case 'R': pt = ROOK; break;
                    case 'K': pt = KING; break;
                    default: break;
                }
                if (promote_next) {
                    pt = (PieceType)(pt + 8);
                    promote_next = false;
                }
                if (c == '+') {
                    promote_next = true;
                    continue;
                }
                board[get_sq(x, y)] = pt | (is_white ? WHITE_FLAG : 0);
                x++;
            }
        }
        
        side_to_move = (color_part == "w") ? WHITE : BLACK; 
        
        if (hand_part != "-") {
            int count = 0;
            for (char c : hand_part) {
                if (c >= '0' && c <= '9') {
                    count = count * 10 + (c - '0');
                } else {
                    bool is_white = (c >= 'a' && c <= 'z');
                    char uc = is_white ? (c - 'a' + 'A') : c;
                    PieceType pt = EMPTY;
                    switch (uc) {
                        case 'P': pt = PAWN; break;
                        case 'L': pt = LANCE; break;
                        case 'N': pt = KNIGHT; break;
                        case 'S': pt = SILVER; break;
                        case 'G': pt = GOLD; break;
                        case 'B': pt = BISHOP; break;
                        case 'R': pt = ROOK; break;
                        default: break;
                    }
                    int final_count = (count == 0) ? 1 : count;
                    hand[is_white ? WHITE : BLACK][pt] = final_count;
                    count = 0;
                }
            }
        }
        update_hash();
    }

    bool make_move(const Move& mv) {
        BoardState prev;
        std::copy(board, board + 81, prev.board);
        std::copy(&hand[0][0], &hand[0][0] + 2 * 8, &prev.hand[0][0]);
        prev.side_to_move = side_to_move;
        prev.hash = current_hash;
        history.push_back(prev);
        
        if (mv.from_sq == -1) {
            hand[side_to_move][mv.drop_piece]--;
            board[mv.to_sq] = mv.drop_piece | (side_to_move == WHITE ? WHITE_FLAG : 0);
        } else {
            uint8_t moving_piece = board[mv.from_sq];
            uint8_t captured_piece = board[mv.to_sq];
            
            if (captured_piece != EMPTY) {
                PieceType cap_type = (PieceType)(captured_piece & PIECE_MASK);
                if (cap_type >= 9) {
                    cap_type = (PieceType)(cap_type - 8);
                }
                hand[side_to_move][cap_type]++;
            }
            
            board[mv.from_sq] = EMPTY;
            if (mv.promote) {
                board[mv.to_sq] = (moving_piece & ~PIECE_MASK) | ((moving_piece & PIECE_MASK) + 8);
            } else {
                board[mv.to_sq] = moving_piece;
            }
        }
        
        side_to_move = (side_to_move == BLACK) ? WHITE : BLACK;
        update_hash();
        return true;
    }

    void unmake_move() {
        if (history.empty()) return;
        BoardState prev = history.back();
        history.pop_back();
        std::copy(prev.board, prev.board + 81, board);
        std::copy(&prev.hand[0][0], &prev.hand[0][0] + 2 * 8, &hand[0][0]);
        side_to_move = prev.side_to_move;
        current_hash = prev.hash;
    }

    Move parse_usi_move(const std::string& move_str) const {
        Move mv;
        mv.from_sq = -1;
        mv.to_sq = -1;
        mv.promote = false;
        mv.drop_piece = EMPTY;
        
        if (move_str.length() < 4) return mv;
        
        if (move_str[1] == '*') {
            char p_char = move_str[0];
            PieceType pt = EMPTY;
            switch (p_char) {
                case 'P': pt = PAWN; break;
                case 'L': pt = LANCE; break;
                case 'N': pt = KNIGHT; break;
                case 'S': pt = SILVER; break;
                case 'G': pt = GOLD; break;
                case 'B': pt = BISHOP; break;
                case 'R': pt = ROOK; break;
                default: break;
            }
            mv.drop_piece = pt;
            int tx = '9' - move_str[2];
            int ty = move_str[3] - 'a';
            mv.to_sq = get_sq(tx, ty);
        } else {
            int fx = '9' - move_str[0];
            int fy = move_str[1] - 'a';
            int tx = '9' - move_str[2];
            int ty = move_str[3] - 'a';
            mv.from_sq = get_sq(fx, fy);
            mv.to_sq = get_sq(tx, ty);
            if (move_str.length() > 4 && move_str[4] == '+') {
                mv.promote = true;
            }
        }
        return mv;
    }
};

// ============================================================================
// 5. åæ³æçæ (Move Generator) ã­ã¸ãã¯ (çæã»äºæ­©ã»èªæ®ºæå®å¨å¯¾å¿)
// ============================================================================
bool is_attacked(const Board& brd, int target_sq, Color attacker_col) {
    for (int sq = 0; sq < 81; ++sq) {
        uint8_t pc = brd.board[sq];
        if (pc == EMPTY) continue;
        Color col = (pc & WHITE_FLAG) ? WHITE : BLACK;
        if (col != attacker_col) continue;
        
        PieceType pt = (PieceType)(pc & PIECE_MASK);
        int sx = sq % 9;
        int sy = sq / 9;
        int tx = target_sq % 9;
        int ty = target_sq / 9;
        
        int dx = tx - sx;
        int dy = ty - sy;
        int adx = std::abs(dx);
        int ady = std::abs(dy);
        int dir_y = (attacker_col == BLACK) ? -1 : 1;
        
        switch (pt) {
            case PAWN:
                if (dx == 0 && dy == dir_y) return true;
                break;
            case LANCE:
                if (dx == 0 && (dy * dir_y > 0)) {
                    int step_y = (dy > 0) ? 1 : -1;
                    bool blocked = false;
                    for (int curr_y = sy + step_y; curr_y != ty; curr_y += step_y) {
                        if (brd.board[get_sq(sx, curr_y)] != EMPTY) {
                            blocked = true;
                            break;
                        }
                    }
                    if (!blocked) return true;
                }
                break;
            case KNIGHT:
                if (adx == 1 && dy == 2 * dir_y) return true;
                break;
            case SILVER:
                if (adx == 1 && ady == 1) return true;
                if (dx == 0 && dy == dir_y) return true;
                break;
            case GOLD:
            case PROMOTED_PAWN:
            case PROMOTED_LANCE:
            case PROMOTED_KNIGHT:
            case PROMOTED_SILVER:
                if (adx <= 1 && ady <= 1 && !(ady == 1 && dy != dir_y && adx == 1)) return true;
                break;
            case KING:
                if (adx <= 1 && ady <= 1) return true;
                break;
            case BISHOP:
            case PROMOTED_BISHOP:
                if (adx == ady) {
                    int step_x = (dx > 0) ? 1 : -1;
                    int step_y = (dy > 0) ? 1 : -1;
                    bool blocked = false;
                    int curr_x = sx + step_x;
                    int curr_y = sy + step_y;
                    while (curr_x != tx && curr_y != ty) {
                        if (brd.board[get_sq(curr_x, curr_y)] != EMPTY) {
                            blocked = true;
                            break;
                        }
                        curr_x += step_x;
                        curr_y += step_y;
                    }
                    if (!blocked) return true;
                }
                if (pt == PROMOTED_BISHOP && (adx + ady == 1)) return true;
                break;
            case ROOK:
            case PROMOTED_ROOK:
                if (dx == 0 || dy == 0) {
                    int step_x = (dx == 0) ? 0 : ((dx > 0) ? 1 : -1);
                    int step_y = (dy == 0) ? 0 : ((dy > 0) ? 1 : -1);
                    bool blocked = false;
                    int curr_x = sx + step_x;
                    int curr_y = sy + step_y;
                    while (curr_x != tx || curr_y != ty) {
                        if (brd.board[get_sq(curr_x, curr_y)] != EMPTY) {
                            blocked = true;
                            break;
                        }
                        curr_x += step_x;
                        curr_y += step_y;
                    }
                    if (!blocked) return true;
                }
                if (pt == PROMOTED_ROOK && (adx == 1 && ady == 1)) return true;
                break;
            default:
                break;
        }
    }
    return false;
}

bool is_in_check(const Board& brd, Color col) {
    int king_sq = -1;
    uint8_t target_king = KING | (col == WHITE ? WHITE_FLAG : 0);
    for (int sq = 0; sq < 81; ++sq) {
        if (brd.board[sq] == target_king) {
            king_sq = sq;
            break;
        }
    }
    if (king_sq == -1) return false;
    Color attacker_col = (col == BLACK) ? WHITE : BLACK;
    return is_attacked(brd, king_sq, attacker_col);
}

std::vector<Move> generate_pseudo_legal_moves(const Board& brd) {
    std::vector<Move> moves;
    Color us = brd.side_to_move;
    int dir_y = (us == BLACK) ? -1 : 1;
    
    for (int sq = 0; sq < 81; ++sq) {
        uint8_t pc = brd.board[sq];
        if (pc == EMPTY) continue;
        Color col = (pc & WHITE_FLAG) ? WHITE : BLACK;
        if (col != us) continue;
        
        PieceType pt = (PieceType)(pc & PIECE_MASK);
        int sx = sq % 9;
        int sy = sq / 9;
        
        auto add_move_if_valid = [&](int tx, int ty) {
            if (tx < 0 || tx >= 9 || ty < 0 || ty >= 9) return;
            int target_sq = get_sq(tx, ty);
            uint8_t target_pc = brd.board[target_sq];
            if (target_pc != EMPTY) {
                Color target_col = (target_pc & WHITE_FLAG) ? WHITE : BLACK;
                if (target_col == us) return;
            }
            
            bool is_promotion_zone = (us == BLACK) ? (sy <= 2 || ty <= 2) : (sy >= 6 || ty >= 6);
            bool can_promote = (pt < 9 && pt != KING && pt != GOLD && is_promotion_zone);
            bool must_promote = false;
            if (pt == PAWN || pt == LANCE) {
                must_promote = (us == BLACK) ? (ty == 0) : (ty == 8);
            } else if (pt == KNIGHT) {
                must_promote = (us == BLACK) ? (ty <= 1) : (ty >= 7);
            }
            
            if (must_promote) {
                if (can_promote) {
                    moves.push_back({sq, target_sq, true, EMPTY});
                }
            } else {
                moves.push_back({sq, target_sq, false, EMPTY});
                if (can_promote) {
                    moves.push_back({sq, target_sq, true, EMPTY});
                }
            }
        };
        
        auto generate_sliding_moves = [&](int dx, int dy) {
            int curr_x = sx + dx;
            int curr_y = sy + dy;
            while (curr_x >= 0 && curr_x < 9 && curr_y >= 0 && curr_y < 9) {
                int target_sq = get_sq(curr_x, curr_y);
                uint8_t target_pc = brd.board[target_sq];
                if (target_pc != EMPTY) {
                    Color target_col = (target_pc & WHITE_FLAG) ? WHITE : BLACK;
                    if (target_col != us) {
                        add_move_if_valid(curr_x, curr_y);
                    }
                    break;
                }
                add_move_if_valid(curr_x, curr_y);
                curr_x += dx;
                curr_y += dy;
            }
        };
        
        switch (pt) {
            case PAWN:
                add_move_if_valid(sx, sy + dir_y);
                break;
            case LANCE:
                generate_sliding_moves(0, dir_y);
                break;
            case KNIGHT:
                add_move_if_valid(sx - 1, sy + 2 * dir_y);
                add_move_if_valid(sx + 1, sy + 2 * dir_y);
                break;
            case SILVER:
                add_move_if_valid(sx - 1, sy + dir_y);
                add_move_if_valid(sx + 1, sy + dir_y);
                add_move_if_valid(sx, sy + dir_y);
                add_move_if_valid(sx - 1, sy - dir_y);
                add_move_if_valid(sx + 1, sy - dir_y);
                break;
            case GOLD:
            case PROMOTED_PAWN:
            case PROMOTED_LANCE:
            case PROMOTED_KNIGHT:
            case PROMOTED_SILVER:
                add_move_if_valid(sx - 1, sy + dir_y);
                add_move_if_valid(sx + 1, sy + dir_y);
                add_move_if_valid(sx, sy + dir_y);
                add_move_if_valid(sx - 1, sy);
                add_move_if_valid(sx + 1, sy);
                add_move_if_valid(sx, sy - dir_y);
                break;
            case KING:
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) continue;
                        add_move_if_valid(sx + dx, sy + dy);
                    } 
                }
                break;
            case BISHOP:
                generate_sliding_moves(1, 1);
                generate_sliding_moves(1, -1);
                generate_sliding_moves(-1, 1);
                generate_sliding_moves(-1, -1);
                break;
            case PROMOTED_BISHOP:
                generate_sliding_moves(1, 1);
                generate_sliding_moves(1, -1);
                generate_sliding_moves(-1, 1);
                generate_sliding_moves(-1, -1);
                add_move_if_valid(sx + 1, sy);
                add_move_if_valid(sx - 1, sy);
                add_move_if_valid(sx, sy + 1);
                add_move_if_valid(sx, sy - 1);
                break;
            case ROOK:
                generate_sliding_moves(1, 0);
                generate_sliding_moves(-1, 0);
                generate_sliding_moves(0, 1); 
                generate_sliding_moves(0, -1);
                break;
            case PROMOTED_ROOK:
                generate_sliding_moves(1, 0);
                generate_sliding_moves(-1, 0);
                generate_sliding_moves(0, 1);
                generate_sliding_moves(0, -1);
                add_move_if_valid(sx + 1, sy + 1);
                add_move_if_valid(sx + 1, sy - 1);
                add_move_if_valid(sx - 1, sy + 1);
                add_move_if_valid(sx - 1, sy - 1);
                break;
            default:
                break;
        }
    }
    
    for (int pc = 1; pc <= 7; ++pc) {
        int count = brd.hand[us][pc];
        if (count == 0) continue;
        
        for (int ty = 0; ty < 9; ++ty) {
            if (pc == PAWN || pc == LANCE) {
                if ((us == BLACK && ty == 0) || (us == WHITE && ty == 8)) continue;
            } else if (pc == KNIGHT) {
                if ((us == BLACK && ty <= 1) || (us == WHITE && ty >= 7)) continue;
            }
            
            for (int tx = 0; tx < 9; ++tx) {
                int target_sq = get_sq(tx, ty);
                if (brd.board[target_sq] != EMPTY) continue;
                
                if (pc == PAWN) {
                    bool nifu = false;
                    for (int y = 0; y < 9; ++y) {
                        uint8_t p = brd.board[get_sq(tx, y)];
                        if (p == (PAWN | (us == WHITE ? WHITE_FLAG : 0))) {
                            nifu = true;
                            break;
                        }
                    }
                    if (nifu) continue;
                }
                moves.push_back({-1, target_sq, false, (PieceType)pc});
            }
        }
    }
    return moves;
}

std::vector<Move> generate_legal_moves(Board& brd) {
    std::vector<Move> pseudo = generate_pseudo_legal_moves(brd);
    std::vector<Move> legal;
    Color us = brd.side_to_move;
    for (const auto& mv : pseudo) {
        brd.make_move(mv);
        if (!is_in_check(brd, us)) {
            legal.push_back(mv);
        }
        brd.unmake_move();
    }
    return legal;
}

// ============================================================================
// 6. å®è·¡ã¡ã¢ãªããã (mmap) ã­ã¸ãã¯
// ============================================================================
struct UsiEngineState {
    float* mmap_ptr = nullptr;
    size_t mmap_size = 0;
};

UsiEngineState* init_usi_engine(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) return nullptr;

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return nullptr;
    }

    void* addr = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) return nullptr;

    madvise(addr, sb.st_size, MADV_WILLNEED | MAP_SHARED);

    UsiEngineState* state = new UsiEngineState();
    state->mmap_ptr = static_cast<float*>(addr);
    state->mmap_size = sb.st_size;
    return state;
}

void destroy_usi_engine(UsiEngineState* state) {
    if (state) {
        if (state->mmap_ptr) munmap(state->mmap_ptr, state->mmap_size);
        delete state;
    }
}

// å®ç§ã« std::stringstream ãæé¤ããè»½éã¹ãã¼ã¹åºåãã¹ããªãã¿
std::vector<std::string> split_command(const std::string& str) {
    std::vector<std::string> tokens;
    size_t start = 0;
    while (true) {
        size_t space = str.find(' ', start);
        if (space == std::string::npos) {
            std::string last = str.substr(start);
            if (!last.empty()) tokens.push_back(last);
            break;
        }
        std::string token = str.substr(start, space - start);
        if (!token.empty()) tokens.push_back(token);
        start = space + 1;
    }
    return tokens;
}

// ============================================================================
// 7. ã¡ã¤ã³ USI ã¤ãã³ãã«ã¼ã (cshogi ã¨ãã»ã³ã¹å®å¨åçé§åä»æ§)
// ============================================================================
int main() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << std::unitbuf;

    UsiEngineState* state = nullptr;
    Board board_inst;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::vector<std::string> tokens = split_command(line);
        if (tokens.empty()) continue;

        std::string cmd = tokens[0];

        if (cmd == "usi") {
            std::cout << "id name ATShogi-OM-Extreme-V6-CPP\n";
            std::cout << "id author aonami8810 & Hayato Aonami\n";
            std::cout << "usiok\n";
        }
        else if (cmd == "isready") {
            if (state == nullptr) {
                const char* bin_path = "static_joseki.bin";
                state = init_usi_engine(bin_path);
                if (state == nullptr) {
                    std::cerr << "ð¨ [ERROR] static_joseki.bin ã®ã­ã¼ãã«å¤±æãã¾ãã\n";
                } else {
                    std::cerr << "ð [SUCCESS] 1.0 MB å®è·¡ã L3 ã­ã£ãã·ã¥ã¸ mmap ãã¦ã³ãå®äº\n";
                }
            }
            std::cout << "readyok\n";
        }
        else if (cmd == "usinewgame") {
            board_inst.clear();
        }
        else if (cmd == "position") {
            board_inst.clear();
            std::string type = "";
            if (tokens.size() > 1) type = tokens[1];
            size_t next_idx = 2;
            
            if (type == "startpos") {
                board_inst.set_startpos();
                next_idx = 2;
            } else if (type == "sfen") {
                std::string sfen_part = "";
                for (int i = 0; i < 4; ++i) {
                    if (2 + i < tokens.size()) {
                        sfen_part += tokens[2 + i] + " ";
                    }
                }
                board_inst.set_sfen(sfen_part);
                next_idx = 6;
            }
            
            if (next_idx < tokens.size() && tokens[next_idx] == "moves") {
                for (size_t i = next_idx + 1; i < tokens.size(); ++i) {
                    Move mv = board_inst.parse_usi_move(tokens[i]);
                    board_inst.make_move(mv);
                }
            }
        }
        else if (cmd == "go") {
            if (state == nullptr) {
                std::cout << "bestmove resign\n";
                continue;
            }

            // 100%åçã«ãç¾å¨ã®ç¤é¢ããçæã»äºæ­©ã»èªæ®ºæå®å¨å¯¾å¿ã®åæ³æãçæï¼
            std::vector<Move> legal_moves = generate_legal_moves(board_inst);
            if (legal_moves.empty()) {
                std::cout << "bestmove resign\n";
                continue;
            }

            Move best_move = legal_moves[0];
            float best_pot = (board_inst.side_to_move == BLACK) ? FLT_MAX : -FLT_MAX;

            // 100%åçã«ãå¨åæ³æãä»®æ³å®è¡ããå±é¢ã® Zobrist ããã·ã¥ããããã³ã·ã£ã«ãã«ãã¯ã¢ããï¼
            for (const auto& mv : legal_moves) {
                board_inst.make_move(mv);
                uint64_t hash = board_inst.current_hash;
                board_inst.unmake_move();

                size_t index = hash % (state->mmap_size / sizeof(float));
                float pot = state->mmap_ptr[index];

                if (board_inst.side_to_move == BLACK) {
                    if (pot < best_pot) {
                        best_pot = pot;
                        best_move = mv;
                    }
                } else {
                    if (pot < 999.0f) {
                        if (pot > best_pot) {
                            best_pot = pot;
                            best_move = mv;
                        }
                    }
                }
            }

            std::cout << "bestmove " << best_move.to_usi() << "\n";
        }
        else if (cmd == "quit") {
            break;
        }
    }

    if (state != nullptr) destroy_usi_engine(state);
    return 0;
}
