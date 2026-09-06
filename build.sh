#!/bin/bash
set -e

echo "[1/4] Cleaning previous build artifacts..."
make clean

echo "[2/4] Compiling AArch64 Static PIE Binary..."
make all

echo "[3/4] Deploying binary to Android assets..."
strip build/atshogi_oex_bin
mkdir -p app/src/main/assets/atshogi_bin
cp build/atshogi_oex_bin app/src/main/assets/atshogi_bin/atshogi_oex_bin
echo "✔ Successfully deployed atshogi_oex_bin to app/src/main/assets/atshogi_bin"

echo "[4/4] Verifying binary ELF header..."
readelf -h ./build/atshogi_oex_bin | grep -E "Class|Type|Machine"
file ./build/atshogi_oex_bin

echo "========================================================"
echo "      ATShogi Android OEX Build Completed Successfully!"
echo "========================================================"
