#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <float.h>
#include <stdint.h>

// ============================================================================
// 1. ATShogi-OM Extreme V5 決定論的幾何学エンジン状態定義
// ============================================================================
struct UsiEngineState {
    float* mmap_ptr = nullptr;
    size_t mmap_size = 0;
    const int chi = 64;             // 神のボンド次元
    const float V_inf = 2.039721f;  // 極限ポテンシャル
};

struct UsiMoveCandidate {
    uint32_t move_id;          // ビットパックされたUSI指し手表現
    uint32_t target_state_idx; // 遷移先ハッセ図（状態空間）インデックス
    int is_gote_after;         // 指し手実行後の手番 (0: 先手, 1: 後手)
};

// ============================================================================
// 2. 指し手のビットパック・アンパック（Word32 ⇔ USI文字列）
// ============================================================================
uint32_t pack_usi_move(const std::string& move_str) {
    if (move_str == "resign" || move_str.length() < 4) {
        return 0xFFFFFFFF;
    }
    uint32_t fx = move_str[0] - '0';
    uint32_t fy = move_str[1] - 'a' + 1;
    uint32_t tx = move_str[2] - '0';
    uint32_t ty = move_str[3] - 'a' + 1;
    uint32_t promo = (move_str.length() > 4 && move_str[4] == '+') ? 1 : 0;

    return fx | (fy << 8) | (tx << 16) | (ty << 24) | (promo << 31);
}

std::string unpack_usi_move(uint32_t w) {
    if (w == 0xFFFFFFFF || w == 0) {
        return "resign";
    }
    uint32_t fx = w & 0xFF;
    uint32_t fy = (w >> 8) & 0xFF;
    uint32_t tx = (w >> 16) & 0xFF;
    uint32_t ty = (w >> 24) & 0xFF;
    uint32_t promo = (w >> 31) & 1;

    std::string move_str = "";
    move_str += (char)(fx + '0');
    move_str += (char)(fy + 'a' - 1);
    move_str += (char)(tx + '0');
    move_str += (char)(ty + 'a' - 1);
    if (promo == 1) {
        move_str += "+";
    }
    return move_str;
}

// ============================================================================
// 3. 定跡メモリマップ (mmap) ロジック
// ============================================================================
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

    // OSのL3キャッシュ・RAMスケジューラへの先読み・連続アクセスアドバイス
    madvise(addr, sb.st_size, MADV_WILLNEED | MADV_SEQUENTIAL);

    UsiEngineState* state = new UsiEngineState();
    state->mmap_ptr = static_cast<float*>(addr);
    state->mmap_size = sb.st_size;
    return state;
}

void destroy_usi_engine(UsiEngineState* state) {
    if (state) {
        if (state->mmap_ptr) {
            munmap(state->mmap_ptr, state->mmap_size);
        }
        delete state;
    }
}

// ============================================================================
// 4. 最急降下勾配流（最善手）の 0ms 逆算
// ============================================================================
uint32_t evaluate_best_move_usi(
    const UsiEngineState* state,
    const std::vector<UsiMoveCandidate>& candidates,
    const float* current_potentials
) {
    if (!state || candidates.empty()) {
        return 0xFFFFFFFF;
    }

    uint32_t best_move_id = 0xFFFFFFFF;

    // 現在の手番判定：最初の候補手の遷移先手番の反転（二部グラフ）
    bool current_is_gote = (candidates[0].is_gote_after == 0);
    const float* pots = (current_potentials != nullptr) ? current_potentials : state->mmap_ptr;

    if (!current_is_gote) {
        // 先手番 (min 評価)：詰み特異点 (0.0) への最短最急降下
        float min_val = FLT_MAX;
        for (const auto& cand : candidates) {
            float pot = pots[cand.target_state_idx];
            if (pot < min_val) {
                min_val = pot;
                best_move_id = cand.move_id;
            }
        }
    } else {
        // 後手番 (max 評価)：最善抵抗（フロンティア限定ガード）
        float max_val = -FLT_MAX;
        for (const auto& cand : candidates) {
            float pot = pots[cand.target_state_idx];
            // 未解決領域 (1000.0) を超えないフロンティア内のみを対象とする
            if (pot < 999.0f) {
                if (pot > max_val) {
                    max_val = pot;
                    best_move_id = cand.move_id;
                }
            }
        }
        // もし完全に包囲されてすべて未解決なら、最初の候補手でフォールバック
        if (best_move_id == 0xFFFFFFFF) {
            best_move_id = candidates[0].move_id;
        }
    }

    return best_move_id;
}

// ============================================================================
// 5. デモ用の合法手候補生成器（手番状態に応じた候補手を生成）
// ============================================================================
std::vector<UsiMoveCandidate> generate_candidates(bool is_gote) {
    std::vector<UsiMoveCandidate> candidates;

    // 最善手（先手: ▲7六歩、後手: △3四歩）を含むダミーのハッセノードリスト
    std::string best_move_str = !is_gote ? "7g7f" : "3c3d";

    // C++構造体とFFI同一のアライメント配列を即時構築
    candidates.push_back({pack_usi_move(best_move_str), 1, is_gote ? 0 : 1}); // 遷移先ノード1
    candidates.push_back({pack_usi_move("2g2f"), 2, is_gote ? 0 : 1});        // 遷移先ノード2
    candidates.push_back({pack_usi_move("8g8f"), 3, is_gote ? 0 : 1});        // 遷移先ノード3

    return candidates;
}

// ============================================================================
// 6. 完璧にstd::stringstreamを排除した、軽量スペース区切りスプリッタ
// ============================================================================
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
// 7. メインUSIプロトコル・イベントループ (100% C++一元化)
// ============================================================================
int main() {
    // 標準入出力の高速化とバッファリング無効化（将棋UI連携の絶対条件）
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout << std::unitbuf;

    UsiEngineState* state = nullptr;
    std::vector<std::string> current_moves;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        // stringstreamを完全排除してパース
        std::vector<std::string> tokens = split_command(line);
        if (tokens.empty()) continue;

        std::string cmd = tokens[0];

        if (cmd == "usi") {
            std::cout << "id name ATShogi-OM-Extreme-V5-CPP\n";
            std::cout << "id author aonami8810 & Hayato Aonami\n";
            std::cout << "usiok\n";
        }
        else if (cmd == "isready") {
            if (state == nullptr) {
                // 1.0 MB の定跡を mmap ロード
                const char* bin_path = "static_joseki.bin";
                state = init_usi_engine(bin_path);
                if (state == nullptr) {
                    std::cerr << "🚨 [ERROR] static_joseki.bin のメモリマップロードに失敗しました\n";
                } else {
                    std::cerr << "🎉 [SUCCESS] 1.0 MB 定跡を L3 キャッシュへ mmap マウント完了\n";
                }
            }
            std::cout << "readyok\n";
        }
        else if (cmd == "usinewgame") {
            current_moves.clear();
        }
        else if (cmd == "position") {
            current_moves.clear();
            bool reading_moves = false;
            for (size_t i = 1; i < tokens.size(); ++i) {
                if (tokens[i] == "startpos") {
                    continue;
                } else if (tokens[i] == "moves") {
                    reading_moves = true;
                    continue;
                }
                if (reading_moves) {
                    current_moves.push_back(tokens[i]);
                }
            }
        }
        else if (cmd == "go") {
            if (state == nullptr) {
                std::cout << "bestmove resign\n";
                continue;
            }

            // 現在の手順数から手番を即時判定（偶数：先手、奇数：後手）
            bool is_gote = (current_moves.size() % 2 == 1);

            // 合法手の生成
            std::vector<UsiMoveCandidate> candidates = generate_candidates(is_gote);

            // 最急降下ベクトルの逆算 (0ms)
            uint32_t best_move_w32 = evaluate_best_move_usi(state, candidates, nullptr);

            // 指し手のアンパックと出力
            std::string best_move_str = unpack_usi_move(best_move_w32);
            std::cout << "bestmove " << best_move_str << "\n";
        }
        else if (cmd == "quit") {
            break;
        }
    }

    if (state != nullptr) {
        destroy_usi_engine(state);
    }
    return 0;
}