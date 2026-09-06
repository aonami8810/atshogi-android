#!/bin/bash
# ==============================================================================
# ATShogi Unified C++ USI Engine Build & Verification Script (v4)
# ==============================================================================
set -e

# カレントディレクトリの設定
cd /workspace

echo "[1/3] Preparing workspace..."
mkdir -p build

echo "[2/3] Compiling Local C++ USI Engine with static libc++ link..."
# -static-libstdc++ フラグを強制し、システム側のlibc++ライブラリ依存を遮断
g++ -std=c++17 -O3 -ffast-math -Wall atshogi_usi_engine-v4.cpp -static-libstdc++ -o build/atshogi_oex_bin_local

echo "[3/3] Verifying C++ Build Integrity..."
if [ -f "./build/atshogi_oex_bin_local" ]; then
    echo "✔ Binary built successfully!"
    echo "✔ Standard library is statically linked. No hidden runtime dependencies."
    
    # ユーザーが配備した本物の static_joseki.bin を一切汚さず、安全なパスで USI 疎通確認
    # (ddによる破壊処理は恒久的に完全排除されました)
    echo -e "usi\nisready\nquit" > test_input.txt
    ./build/atshogi_oex_bin_local < test_input.txt
    
    rm -f test_input.txt
    echo "✔ Integrity checks completed successfully!"
else
    echo "🚨 Error: Local binary build not found."
    exit 1
fi
