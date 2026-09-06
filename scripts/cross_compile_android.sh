#!/bin/bash
set -e

# ==============================================================================
# Android Cross-Compilation Pipeline for Topological Shogi Engine
# Target: AArch64 (moto g05)
# Requirements: GHC 9.4.8, Android NDK r26b (LLVM 17)
# ==============================================================================

# User decisions from User Review
GHC_VERSION="9.4.8"
NDK_VERSION="26.1.10909125" # r26b
API_LEVEL="34"
TARGET_TRIPLE="aarch64-linux-android"

echo "========================================================"
echo "Starting Cross-Compilation Environment Setup"
echo "Target: $TARGET_TRIPLE"
echo "GHC Version: $GHC_VERSION"
echo "NDK Version: $NDK_VERSION"
echo "========================================================"

# Validate NDK Path
if [ -z "$ANDROID_NDK_ROOT" ]; then
    echo "Error: ANDROID_NDK_ROOT is not set."
    echo "Please set it to your NDK r26b directory."
    exit 1
fi

TOOLCHAIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
CC="$TOOLCHAIN/bin/${TARGET_TRIPLE}${API_LEVEL}-clang"
CXX="$TOOLCHAIN/bin/${TARGET_TRIPLE}${API_LEVEL}-clang++"

if [ ! -f "$CC" ]; then
    echo "Error: Clang toolchain not found at $CC"
    exit 1
fi

echo "[1/3] Configuring GHC $GHC_VERSION cross-compiler..."
# Note: Assuming ghc-$GHC_VERSION is installed via ghcup or similar, 
# and a cross-compiler is configured (e.g., via ghc-android or custom build)
# For this script, we'll configure the flags for the PIE build.

GHC_FLAGS="-O2 -fllvm -fPIC -optl-pie -optl-fuse-ld=lld"
GHC_FLAGS="$GHC_FLAGS -pgmc=$CC -pgma=$CC -pgml=$CC"

echo "[2/3] Compiling C++ NEON Kernels..."
mkdir -p build/android
$CXX -O3 -flto -march=armv8-a+simd -fPIC -std=c++17 \
     -c src/cpp/tensor_sweeper_v2.cpp -o build/android/tensor_sweeper_v2.o
$CXX -O3 -flto -march=armv8-a+simd -fPIC -std=c++17 \
     -c src/cpp/EgtbCancellation.cpp -o build/android/EgtbCancellation.o
$CXX -O3 -flto -march=armv8-a+simd -fPIC -std=c++17 \
     -c src/cpp/CobordismBridge.cpp -o build/android/CobordismBridge.o
$CXX -O3 -flto -march=armv8-a+simd -fPIC -std=c++17 \
     -c src/cpp/PersistentHomology.cpp -o build/android/PersistentHomology.o

echo "[3/3] Cross-compiling Haskell Core and Linking..."
# We use the cross-compiled ghc (e.g., aarch64-linux-android-ghc)
if command -v ${TARGET_TRIPLE}-ghc &> /dev/null; then
    CROSS_GHC="${TARGET_TRIPLE}-ghc"
else
    echo "Warning: ${TARGET_TRIPLE}-ghc not found in PATH."
    echo "Falling back to 'ghc' (Ensure it is configured for cross-compilation!)"
    CROSS_GHC="ghc"
fi

$CROSS_GHC $GHC_FLAGS \
    -isrc/haskell \
    -c src/haskell/PersistentSafety.hs \
    -c src/haskell/ParallelOmniGeneratorV2.hs \
    -c src/haskell/SpectralEGTB.hs \
    -c src/haskell/CobordismBridge.hs

# Final PIE Link
$CROSS_GHC $GHC_FLAGS -o bin/atshogi-android \
    src/haskell/*.o build/android/*.o

echo "========================================================"
echo "Build Successful: bin/atshogi-android (PIE static/dynamic)"
echo "========================================================"
