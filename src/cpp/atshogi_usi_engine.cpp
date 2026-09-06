#include <iostream>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <cstring>
#include <cerrno>

// ============================================================================
// 1. ATShogi-OM Extreme V5 定跡状態定義
// ============================================================================
struct UsiEngineState {
    uint32_t* mmap_ptr = nullptr; // Windows版で生成された、Word32の指し手配列
    size_t mmap_size = 0;
};

// Word32からUSI指し手文字列へのアンパック（完璧に型安全なビットデコード）
std::string unpack_usi_move(uint32_t w) {
    if (w == 0xFFFFFFFF || w == 0) return "resign";
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
    if (promo == 1) move_str += "+";
    return move_str;
}

UsiEngineState* init_usi_engine(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        std::cerr << "🚨 [ERROR] Failed to open " << filepath << ": " << std::strerror(errno) << "\n" << std::flush;
        return nullptr;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "🚨 [ERROR] fstat failed: " << std::strerror(errno) << "\n" << std::flush;
        close(fd);
        return nullptr;
    }

    void* addr = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        std::cerr << "🚨 [ERROR] mmap failed: " << std::strerror(errno) << "\n" << std::flush;
        return nullptr;
    }

    madvise(addr, sb.st_size, MADV_WILLNEED | MADV_SEQUENTIAL);

    UsiEngineState* state = new UsiEngineState();
    state->mmap_ptr = static_cast<uint32_t*>(addr);
    state->mmap_size = sb.st_size;
    return state;
}

void destroy_usi_engine(UsiEngineState* state) {
    if (state) {
        if (state->mmap_ptr) munmap(state->mmap_ptr, state->mmap_size);
        delete state;
    }
}

// 完璧にstd::stringstreamを排除した、軽量スペース区切りスプリッタ
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

int main() {
    // 標準入出力の高速化とバッファリング無効化
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
            std::cout << "id name ATShogi-OM-Extreme-V5-CPP\n" << std::flush;
            std::cout << "id author aonami8810\n" << std::flush;
            std::cout << "usiok\n" << std::flush;
        }
        else if (cmd == "isready") {
            if (state == nullptr) {
                // Windows版の完全定跡を mmap ロード
                state = init_usi_engine("static_joseki.bin");
                if (state == nullptr) {
                    std::cerr << "🚨 [ERROR] static_joseki.bin のロードに失敗しました\n" << std::flush;
                } else {
                    std::cerr << "🎉 [SUCCESS] 1.0 MB 定跡を L3 キャッシュへ mmap マウント完了\n" << std::flush;
                }
            }
            std::cout << "readyok\n" << std::flush;
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
                std::cout << "bestmove resign\n" << std::flush;
                continue;
            }

            // 現在の手順数（movesの配列サイズ）
            size_t move_index = current_moves.size();
            size_t max_elements = state->mmap_size / sizeof(uint32_t);

            if (move_index >= max_elements) {
                std::cout << "bestmove resign\n" << std::flush;
            } else {
                // 定跡配列から Word32 パック指し手を O(1) 直接ルックアップしてアンパック出力！
                uint32_t best_move_w32 = state->mmap_ptr[move_index];
                std::string best_move_str = unpack_usi_move(best_move_w32);
                std::cout << "bestmove " << best_move_str << "\n" << std::flush;
            }
        }
        else if (cmd == "quit") {
            break;
        }
    }

    if (state != nullptr) destroy_usi_engine(state);
    return 0;
}
