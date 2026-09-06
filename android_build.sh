#!/bin/bash
# ==============================================================================
# ATShogi-Android Separated C++ & Haskell Cross-Compilation Script
# Automatically uses the QEMU AArch64 chroot environment from the atshogi workspace.
# Run this script with: wsl -u root -- bash android_build.sh
# ==============================================================================
set -e

CHROOT_DIR="/home/haonami/ubuntu-arm64-full"

if [ ! -d "$CHROOT_DIR" ]; then
    echo "Error: CHROOT_DIR $CHROOT_DIR not found."
    echo "Make sure you are running this in the correct WSL instance with the rootfs."
    exit 1
fi

echo "========================================================"
echo " [Step 0] Preparing QEMU AArch64 chroot & NDK mounts..."
echo "========================================================"
mkdir -p "$CHROOT_DIR/mnt/c/VS/Workspace"
mkdir -p "$CHROOT_DIR/opt/android-ndk-r27b"
mkdir -p "$CHROOT_DIR/tmp/libndk_stubs"

mkdir -p "$CHROOT_DIR/lib64"
mkdir -p "$CHROOT_DIR/usr/lib/x86_64-linux-gnu"

# Mount Windows drives and NDK into chroot if not already mounted
mount --bind /mnt/c/VS/Workspace "$CHROOT_DIR/mnt/c/VS/Workspace" 2>/dev/null || true
mount --bind /opt/android-ndk-r27b "$CHROOT_DIR/opt/android-ndk-r27b" 2>/dev/null || true
mount --bind /lib64 "$CHROOT_DIR/lib64" 2>/dev/null || true
mount --bind /usr/lib/x86_64-linux-gnu "$CHROOT_DIR/usr/lib/x86_64-linux-gnu" 2>/dev/null || true

# Enter the chroot to perform the separated build
echo "Entering chroot to perform C++ and Haskell separated build..."
chroot "$CHROOT_DIR" bash -c "
set -e
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd /mnt/c/VS/Workspace/atshogi-android

echo '========================================================'
echo ' [Step 1] Building C++ / C Wrapper Library (.so)...'
echo '========================================================'

NDK_CLANG=\"/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang\"
NDK_CLANGXX=\"/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android35-clang++\"
STRIP=\"/opt/android-ndk-r27b/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip\"

OUT_DIR=\"./build\"
mkdir -p \"\$OUT_DIR\"
LIB_CPP=\"\$OUT_DIR/libatshogicpp.a\"
TARGET_ANDROID=\"\$OUT_DIR/atshogi_oex_bin\"

# Compile C code
\$NDK_CLANG -O3 -fPIC -c \\
    -D__ARM_NEON \\
    app/src/main/cpp/cshogi_core.c \\
    app/src/main/cpp/android_bionic_compat.c

# Compile C++ code into object files
\$NDK_CLANGXX -O3 -fPIC -c \\
    -std=c++17 -D__ARM_NEON -Wno-deprecated \\
    app/src/main/cpp/OrbifoldMorseNEON.cpp \\
    app/src/main/cpp/TaylorKernelNEON.cpp \\
    app/src/main/cpp/GaifullinP2NEON.cpp \\
    app/src/main/cpp/EgtbMpsNEON.cpp \\
    app/src/main/cpp/DecayKernel.cpp \\
    app/src/main/cpp/BlunderDetector.cpp \\
    app/src/main/cpp/StratifiedCobordism40.cpp \\
    app/src/main/cpp/MeraTensorNetworkNEON.cpp \\
    app/src/main/cpp/PosixAtlasLoader.cpp

# Archive into a static library
ar rcs \"\$LIB_CPP\" *.o
rm -f *.o

echo 'Successfully built C++ wrapper library: '\$LIB_CPP

echo 'Compiling bionic compat as static object...'

echo '========================================================'
echo ' [Step 2] Building Haskell FFI and Linking...'
echo '========================================================'

# Prepare NDK stub archives for GHC linking
ar rcs /tmp/libndk_stubs/librt.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libgmp.a /tmp/libndk_stubs/libgmp.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libffi.a /tmp/libndk_stubs/libffi.a 2>/dev/null || true

# Compile Haskell and link statically against the newly created C++ shared library
ghc -O2 \\
    -pgmc=\"\$NDK_CLANG\" \\
    -pgmcxx=\"\$NDK_CLANGXX\" \\
    -pgml=\"\$NDK_CLANGXX\" \\
    -optc-I/usr/include/aarch64-linux-gnu -optc-I/usr/include \\
    -optc-fPIC -optl-static -static \\
    -optl-pthread -threaded -rtsopts \\
    \"-with-rtsopts=-N\" \\
    -optl-L/tmp/libndk_stubs \\
    -optl-Wl,--unresolved-symbols=ignore-in-shared-libs \\
    -cpp -D__ARM_NEON \\
    -i./src \\
    src/ATShogi.hs \\
    \"\$LIB_CPP\" \\
    -o \"\$TARGET_ANDROID\"

echo 'Successfully built Haskell Android PIE binary: '\$TARGET_ANDROID

echo '========================================================'
echo ' [Step 3] Stripping binary and deploying...'
echo '========================================================'
\$STRIP \"\$TARGET_ANDROID\"

JNI_DIR=\"./app/src/main/jniLibs/arm64-v8a\"
mkdir -p \"\$JNI_DIR\"
cp \"\$TARGET_ANDROID\" \"\$JNI_DIR/libatshogi_engine.so\"

echo 'Successfully deployed to jniLibs.'
"

echo "Done! The separated cross-compilation via WSL chroot has completed successfully."
