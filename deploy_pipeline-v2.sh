#!/bin/bash
# ==============================================================================
# ATShogi ? ATShogi-Android (AArch64) ??????????????????? (v2)
# ==============================================================================
set -e

export ANDROID_NDK_HOME=$HOME/android-ndk-r26b
export API=33
export WINDOWS_PROJECT_DIR="/mnt/c/VS/Workspace/atshogi-android"
export ASSETS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/assets"
export JNI_LIBS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/jniLibs/arm64-v8a"

echo "===================================================================="
echo "    ATShogi-OM Extreme V5: Automated Build & Deploy Pipeline (v2)"
echo "===================================================================="

echo "[1/4] Verifying raw static_joseki.bin (1.0 MB)..."
if [ -f "static_joseki.bin" ]; then
    FILE_SIZE=$(stat -c%s "static_joseki.bin")
    if [ "$FILE_SIZE" -eq 1048576 ]; then
        echo "? Verified: static_joseki.bin exists and has the correct size ($FILE_SIZE bytes)."
    else
        echo "?? ERROR: static_joseki.bin ???? ($FILE_SIZE bytes) ???????"
        echo "  ??? 1.0 MB?1,048,576 bytes????????????????????"
        echo "  ???????12????????????????????????????exit 1?????"
        exit 1
    fi
else
    echo "?? ERROR: static_joseki.bin ???????????????????"
    echo "  Windows?ATShogi????? 1.0 MB ????????????????????????????"
    exit 1
fi

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

echo "[3/4] Packaging and deploying to Windows Android Studio Project..."
if [ -d "$WINDOWS_PROJECT_DIR" ]; then
    mkdir -p "$ASSETS_DIR"
    mkdir -p "$JNI_LIBS_DIR"
    
    cp static_joseki.bin "$ASSETS_DIR/static_joseki.bin"
    cp build/libatshogi_oex_bin.so "$JNI_LIBS_DIR/libatshogi_oex_bin.so"
    
    echo "? Successfully deployed: libatshogi_oex_bin.so ? $JNI_LIBS_DIR"
    echo "? Successfully deployed: static_joseki.bin (1.0 MB) ? $ASSETS_DIR"
else
    echo "?? ERROR: Windows Project Directory not found at: $WINDOWS_PROJECT_DIR"
    exit 1
fi

echo "===================================================================="
echo "    Deploy Pipeline (v2) Complete! Ready for Android Studio Run."
echo "===================================================================="
