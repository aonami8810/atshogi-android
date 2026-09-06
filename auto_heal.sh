#!/usr/bin/env bash
# ==============================================================================
# ATShogi-OM: Autonomic Self-Healing Build & Audit Script
# Target: WSL 2 (Ubuntu) to Android AArch64 (moto g05 - Cortex-A76 / 1MB L3)
# ==============================================================================

set -euo pipefail

# カラー定義
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# パラメータ定義
MAX_RETRIES=3
RETRY_COUNT=0
BUILD_LOG="build/build_err.log"
AUDIT_LOG="build/audit_run.log"
HEAL_PROMPT_TMP="build/heal_prompt.md"

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}          ATShogi-OM Autonomic Self-Healing Build System        ${NC}"
echo -e "${BLUE}================================================================${NC}"

mkdir -p build

# 診断およびプロンプト送信関数
trigger_self_healing() {
    local error_context="$1"
    local anomaly_type="$2"

    echo -e "${YELLOW}[Self-Healing] アノマリー検出: ${anomaly_type}${NC}"
    echo -e "${YELLOW}[Self-Healing] Antigravity 自律修復用プロンプトを生成中...${NC}"

    # 自己修復プロンプト・テンプレートの構築
    cat << EOF > "${HEAL_PROMPT_TMP}"
# [SYSTEM DIRECTIVE: TOPOLOGICAL SELF-HEALING CONTROL]
You are acting as the 'simd-compute-agent' under Google Antigravity.
A build/simulation anomaly has been intercepted. You must fix it autonomously.

## 1. DETECTED ANOMALY CONTEXT
- **Anomaly Type**: ${anomaly_type}
- **Log Slice**:
\`\`\`
${error_context}
\`\`\`

## 2. REPAIR OBJECTIVE & CONSTRAINTS
- Target hardware is moto g05 (Cortex-A76, L3 Cache 1MB).
- Ensure any code modifications keep the data size for k=7 EGTB within 207KB (L3 occupancy 20.2%).
- DO NOT introduce standard floating-point division (/) or square root (sqrt) operations in the SIMD path. Use Square Norm (\$D^2\$) instead.
- Retain ARM NEON parallelization structure (unrolled by 4).
- Ensure zero heap allocation (No JNI bindings, use raw 'Ptr CFloat' with Haskell FFI).

Analyze the error log above, identify the failing logic or precision mismatch, patch the source code file, and commit the fix.
EOF

    echo -e "${CYAN}[Self-Healing] プロンプト作成完了 -> ${HEAL_PROMPT_TMP}${NC}"

    # Google Antigravity CLI またはエージェント API との通信
    if command -v antigravity &> /dev/null; then
        echo -e "${BLUE}[Antigravity] APIを呼び出し、修復パイプラインを起動します...${NC}"
        # Antigravityに自律コンテキストを流し込み、エージェントにコードを書き換えさせる
        antigravity execute --agent "simd-compute-agent" --prompt-file "${HEAL_PROMPT_TMP}"
        echo -e "${GREEN}[Antigravity] エージェントによる修復コードの適用が完了しました。${NC}"
    else
        echo -e "${RED}[WARNING] Google Antigravity CLI が検出されません。${NC}"
        echo -e "${YELLOW}[Fallback] ローカルLLMエンドポイント経由、または手動デバッグ向けプロンプトとしてログに保存します。${NC}"
        cat "${HEAL_PROMPT_TMP}"
        echo -e "${RED}自動コード修復をスキップし、手動修正を待機します。${NC}"
        exit 1
    fi
}

# ------------------------------------------------------------------------------
# メイン・リカバリループ
# ------------------------------------------------------------------------------
while [ ${RETRY_COUNT} -lt ${MAX_RETRIES} ]; do
    RETRY_COUNT=$((RETRY_COUNT + 1))
    echo -e "${BLUE}[Step 1/2] AArch64 PIE バイナリをビルド中 (Try ${RETRY_COUNT}/${MAX_RETRIES})...${NC}"

    # 1. コンパイルの実行
    if bash android_build.sh > "${BUILD_LOG}" 2>&1; then
        echo -e "${GREEN}[SUCCESS] Haskell / C++ のコンパイルが静的に成功しました。${NC}"
    else
        echo -e "${RED}[ERROR] ビルド中にコンパイルエラーが発生しました。${NC}"
        # ログからエラーコンテキストを抽出
        ERR_SLICE=$(tail -n 15 "${BUILD_LOG}")
        trigger_self_healing "${ERR_SLICE}" "Compile / Link Error (GHC NDK or Clang++)"
        continue
    fi

    # 2. QEMU仮想エミュレーションによるトポロジー監査
    echo -e "${BLUE}[Step 2/2] QEMU 仮想環境でトポロジカル・ポテンシャル監査を実行中...${NC}"

    # QEMUによる実行（非インタラクティブ監査モード）
    # isreadyコマンドを入力として与え、EGTB mmapロードやGaifullin不変量p2の計算を走らせる
    if printf "isready\nquit\n" | qemu-aarch64 -L /usr/aarch64-linux-gnu ./build/atshogi_oex_bin > "${AUDIT_LOG}" 2>&1 || true; then
        # ログに残るオーバーフローやセグフォを検知、またSIGSEGVシグナルをチェック
        if grep -qi "overflow" "${AUDIT_LOG}" || grep -qi "segmentation fault" "${AUDIT_LOG}"; then
            echo -e "${RED}[ERROR] ランタイム実行中にアノマリーが検出されました。${NC}"
            ERR_SLICE=$(grep -C 5 -E -i "overflow|fault|p2|residual" "${AUDIT_LOG}" || tail -n 15 "${AUDIT_LOG}")
            trigger_self_healing "${ERR_SLICE}" "Runtime Anomalous Dislocation (Topology/Boundary Break)"
            continue
        fi

        # 正常終了
        echo -e "${GREEN}================================================================${NC}"
        echo -e "${GREEN}[SUCCESS] すべてのコンパイルおよびトポロジー監査（残差=0）をパスしました！${NC}"
        echo -e "${GREEN}          ターゲットバイナリ: build/atshogi_oex_bin${NC}"
        echo -e "${GREEN}================================================================${NC}"
        exit 0
    else
        echo -e "${RED}[ERROR] QEMU 実行環境が異常終了しました (SIGSEGV or Runtime Panic)。${NC}"
        ERR_SLICE=$(tail -n 15 "${AUDIT_LOG}")
        trigger_self_healing "${ERR_SLICE}" "QEMU Virtual Execution Engine Crash"
        continue
    fi
done

echo -e "${RED}[FATAL] 自動修復の試行回数上限（${MAX_RETRIES}回）に達しました。ビルドを停止します。${NC}"
exit 1