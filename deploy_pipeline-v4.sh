#!/bin/bash
# ==============================================================================
# ATShogi ? ATShogi-Android ????????????????????????? (v4)
# ==============================================================================
set -e

export ANDROID_NDK_HOME=$HOME/android-ndk-r26b
export API=33
export WINDOWS_PROJECT_DIR="/mnt/c/VS/Workspace/atshogi-android"
export ASSETS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/assets"
export JNI_LIBS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/jniLibs/arm64-v8a"

echo "===================================================================="
echo "    ATShogi-OM Extreme V5: Automated Build & Deploy Pipeline (v4)"
echo "===================================================================="

# 1. ???????????static_joseki.bin????????????
echo "[1/4] Verifying raw static_joseki.bin in source directory..."
if [ -f "static_joseki.bin" ]; then
    SRC_SIZE=$(stat -c%s "static_joseki.bin")
    if [ "$SRC_SIZE" -eq 1048576 ]; then
        SRC_HASH=$(sha256sum "static_joseki.bin" | awk '{print $1}')
        echo "? Source verified: static_joseki.bin size=$SRC_SIZE bytes, SHA-256=$SRC_HASH"
    else
        echo "?? ERROR: ????? static_joseki.bin ???? ($SRC_SIZE bytes) ???????"
        echo "  AI????????????????????????????????????????"
        echo "  ?????????? 1,048,576 ??????????????????????"
        exit 1
    fi
else
    echo "?? ERROR: static_joseki.bin ???????????????????"
    exit 1
fi

# 2. C++ ??USI????????????? (stringstream????)
echo "[2/4] Cross-compiling C++ USI Engine for AArch64 (PIE + Static STDCPP + 64KB Align)..."
mkdir -p build
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$API-clang++ \
    -std=c++17 -O3 -ffast-math -Wall \
    atshogi_usi_engine-v4.cpp \
    -static-libstdc++ \
    -Wl,-z,max-page-size=65536 \
    -pie \
    -o build/libatshogi_oex_bin.so

echo "? Compilation succeeded. Stripping debug symbols..."
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip build/libatshogi_oex_bin.so

# 3. ?????????????????SHA-256??????????????
echo "[3/4] Copying assets and performing bit-by-bit dynamic hash verification..."
if [ -d "$WINDOWS_PROJECT_DIR" ]; then
    mkdir -p "$ASSETS_DIR"
    mkdir -p "$JNI_LIBS_DIR"
    
    # ?????????
    cp static_joseki.bin "$ASSETS_DIR/static_joseki.bin"
    cp build/libatshogi_oex_bin.so "$JNI_LIBS_DIR/libatshogi_oex_bin.so"
    
    # ??????????????????????????????????????
    DEST_HASH=$(sha256sum "$ASSETS_DIR/static_joseki.bin" | awk '{print $1}')
    
    echo "  -> Source Hash: $SRC_HASH"
    echo "  -> Copied Hash: $DEST_HASH"
    
    if [ "$SRC_HASH" = "$DEST_HASH" ]; then
        echo "? PASS: Transfer verified. Source and destination file hashes are 100% IDENTICAL!"
    else
        echo "?? ERROR: ?????????????????????????????????????????"
        exit 1
    fi
else
    echo "?? ERROR: Windows Project Directory not found at: $WINDOWS_PROJECT_DIR"
    exit 1
fi

echo "===================================================================="
echo "    Deploy Pipeline (v4) Complete! Proceed to local Unit Tests."
echo "===================================================================="
