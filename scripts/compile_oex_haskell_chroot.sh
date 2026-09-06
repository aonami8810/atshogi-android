#!/bin/bash
# compile_oex_haskell_chroot.sh
# Native AArch64 GHC Compilation script using Official Android NDK r27b Clang Toolchain (PIE + Bionic)
set -e

CHROOT_DIR="/home/haonami/ubuntu-arm64-full"

echo "Preparing QEMU AArch64 chroot & NDK mounts..."
mkdir -p "$CHROOT_DIR/mnt/c/VS/Workspace"
mkdir -p "$CHROOT_DIR/opt/android-ndk-r27b"
mkdir -p "$CHROOT_DIR/lib64"
mkdir -p "$CHROOT_DIR/usr/lib/x86_64-linux-gnu"
mkdir -p "$CHROOT_DIR/tmp/libndk_stubs"

mount --bind /mnt/c/VS/Workspace "$CHROOT_DIR/mnt/c/VS/Workspace" 2>/dev/null || true
mount --bind /opt/android-ndk-r27b "$CHROOT_DIR/opt/android-ndk-r27b" 2>/dev/null || true
mount --bind /lib64 "$CHROOT_DIR/lib64" 2>/dev/null || true
mount --bind /usr/lib/x86_64-linux-gnu "$CHROOT_DIR/usr/lib/x86_64-linux-gnu" 2>/dev/null || true

echo "Executing Android NDK r27b Clang & GHC compilation inside chroot..."
chroot "$CHROOT_DIR" bash -c "
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd /mnt/c/VS/Workspace/atshogi

# Prepare NDK stub archives for GHC linking
mkdir -p /tmp/libndk_stubs
ar rcs /tmp/libndk_stubs/librt.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libgmp.a /tmp/libndk_stubs/libgmp.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libffi.a /tmp/libndk_stubs/libffi.a 2>/dev/null || true

NDK_CLANG=\"/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang\"
NDK_CLANGXX=\"/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang++\"

echo 'Pre-compiling C/C++ SIMD kernels & Bionic compat with Android NDK Clang (API 35)...'
\$NDK_CLANGXX -O2 -fPIC -fPIE -c TopologicalShogi/SIMD/TaylorKernelAVX512.cpp -o TopologicalShogi/SIMD/TaylorKernelAVX512.o
\$NDK_CLANGXX -O2 -fPIC -fPIE -c TopologicalShogi/SIMD/OrbifoldMorseAVX512.cpp -o TopologicalShogi/SIMD/OrbifoldMorseAVX512.o
\$NDK_CLANGXX -O2 -fPIC -fPIE -c TopologicalShogi/SIMD/GaifullinP2AVX512.cpp -o TopologicalShogi/SIMD/GaifullinP2AVX512.o
\$NDK_CLANG -O2 -fPIC -fPIE -c cl_topological_engine.c -o cl_topological_engine.o
\$NDK_CLANG -O2 -fPIC -fPIE -c android_bionic_compat.c -o android_bionic_compat.o

echo 'Linking native AArch64 Haskell USI engine with Android NDK Clang driver (PIE + Bionic + 64KB alignment)...'
ghc -O2 -fPIC -optc-fPIE -optl-pie -pie \
    -threaded -rtsopts -optl-pthread \
    -pgmc \"\$NDK_CLANG\" \
    -pgml \"\$NDK_CLANG\" \
    -optl-L/tmp/libndk_stubs \
    -optl-static-libgcc \
    -optl-Wl,-z,common-page-size=65536 -optl-Wl,-z,max-page-size=65536 \
    -cpp -D__ARM_NEON \
    -i. -iTopologicalShogi/SIMD/ \
    ATShogi.hs \
    cl_topological_engine.o \
    android_bionic_compat.o \
    TopologicalShogi/SIMD/TaylorKernelAVX512.o \
    TopologicalShogi/SIMD/OrbifoldMorseAVX512.o \
    TopologicalShogi/SIMD/GaifullinP2AVX512.o \
    -o /tmp/atshogi_engine_aarch64

/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip /tmp/atshogi_engine_aarch64
"

TARGET_BIN="$CHROOT_DIR/tmp/atshogi_engine_aarch64"
echo "Deploying Android NDK AArch64 PIE binary to Android assets..."
ASSET_DIR="/mnt/c/VS/Workspace/atshogi-android/app/src/main/assets/engine"
mkdir -p "$ASSET_DIR"
cp "$TARGET_BIN" "$ASSET_DIR/atshogi-engine-aarch64"

# Synchronize with jniLibs libatshogi_engine.so
mkdir -p "/mnt/c/VS/Workspace/atshogi-android/app/src/main/jniLibs/arm64-v8a"
cp "$TARGET_BIN" "/mnt/c/VS/Workspace/atshogi-android/app/src/main/jniLibs/arm64-v8a/libatshogi_engine.so"

echo "--------------------------------------------------------"
echo "AArch64 Android NDK PIE Haskell USI Engine compiled successfully!"
echo "Binary path: $TARGET_BIN"
echo "Deployed asset: $ASSET_DIR/atshogi-engine-aarch64"
file "$ASSET_DIR/atshogi-engine-aarch64"
echo "--------------------------------------------------------"
echo "Successfully updated assets & jniLibs libatshogi_engine.so"
echo "Done."

echo "--------------------------------------------------------"
echo "AArch64 PIE Haskell USI Engine compiled successfully!"
echo "Binary path: $TARGET_BIN"
echo "Deployed asset: $ASSET_DIR/atshogi-engine-aarch64"
file "$ASSET_DIR/atshogi-engine-aarch64"
echo "--------------------------------------------------------"
