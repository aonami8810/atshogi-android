#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <cmath> // ★追加：NaN検知用
#include <float.h>
#include <vector>

// V5 決定論的定跡エンジンのメタデータ
struct UsiEngineState {
    float* mmap_ptr;
    size_t mmap_size;
    const int chi = 64; // 神のボンド次元
    const float V_inf = 2.039721f;
};

// 1手ごとの指し手候補構造体 (Haskell-C++ FFIマッピング用)
struct UsiMoveCandidate {
    uint32_t move_id;         // USI指し手表現（パックされたWord32）
    uint32_t target_state_idx; // 遷移先ハッセ図ノードインデックス
    int is_gote_after;        // 指し手実行後の手番 (0: 先手, 1: 後手)
};

extern "C" {

/**
 * 1. POSIX mmap による 1.0 MB 定跡 static_joseki.bin のロード
 */
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

    // 常駐アドバイス
    madvise(addr, sb.st_size, MADV_WILLNEED | MADV_SEQUENTIAL);

    UsiEngineState* state = new UsiEngineState();
    state->mmap_ptr = static_cast<float*>(addr);
    state->mmap_size = sb.st_size;
    return state;
}

/**
 * 2. 安全な定跡のアンロード
 */
void destroy_usi_engine(UsiEngineState* state) {
    if (state) {
        if (state->mmap_ptr) {
            munmap(state->mmap_ptr, state->mmap_size);
        }
        delete state;
    }
}

/**
 * 3. USI 'go' 受信時:
 * 合法手候補を走査し、mmap定跡ポテンシャル場から「最急降下ベクトル（最善手）」を 0ms で決定する。
 */
uint32_t evaluate_best_move_usi(
    const UsiEngineState* state,
    const UsiMoveCandidate* candidates,
    int num_candidates,
    const float* current_potentials
) {
    if (!state || !candidates || num_candidates <= 0) {
        return 0xFFFFFFFF;
    }

    uint32_t best_move_id = 0xFFFFFFFF;

    // 呼び出し元の現在手番を、最初の候補手の遷移後反転から動的に取得（二部グラフ）
    bool current_is_gote = (candidates[0].is_gote_after == 0);

    // ポテンシャルが未解決（1000.0）の場合を考慮したポインタ参照先フォールバック
    const float* pots = (current_potentials != nullptr) ? current_potentials : state->mmap_ptr;

    if (!current_is_gote) {
        // 先手番 (min 評価)：詰み特異点 (0.0) への最短最急降下
        float min_val = FLT_MAX;
        for (int i = 0; i < num_candidates; ++i) {
            uint32_t tgt_node = candidates[i].target_state_idx;
            float pot = pots[tgt_node];
            // NaN（非数）は評価から除外
            if (std::isnan(pot)) continue;

            if (pot < min_val) {
                min_val = pot;
                best_move_id = candidates[i].move_id;
            }
        }
    } else {
        // 後手番 (max 評価)：最善抵抗
        float max_val = -FLT_MAX;
        for (int i = 0; i < num_candidates; ++i) {
            uint32_t tgt_node = candidates[i].target_state_idx;
            float pot = pots[tgt_node];
            // NaN（非数）は評価から除外
            if (std::isnan(pot)) continue;

            if (pot < 999.0f) {
                if (pot > max_val) {
                    max_val = pot;
                    best_move_id = candidates[i].move_id;
                }
            }
        }
    }

    // ★【ここが最重要セーフガード！】
    // 先手・後手問わず、評価値の不整合やNaNにより最善手が決まらなかった場合、
    // 即投了せず、必ず第一推奨候補手を安全フォールバックとして返す。
    if (best_move_id == 0xFFFFFFFF || best_move_id == 0) {
        best_move_id = candidates[0].move_id;
    }

    return best_move_id;
}

// std::vector<UsiMoveCandidate> C++ラッパー
uint32_t evaluate_best_move_usi(
    const UsiEngineState* state,
    const std::vector<UsiMoveCandidate>& candidates,
    const float* current_potentials
) {
    if (!state || candidates.empty()) {
        return 0xFFFFFFFF;
    }
    return evaluate_best_move_usi(state, candidates.data(), static_cast<int>(candidates.size()), current_potentials);
}

} // extern "C"
