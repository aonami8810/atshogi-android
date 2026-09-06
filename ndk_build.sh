#!/bin/bash
set -e

CHROOT_DIR="/home/haonami/ubuntu-arm64-full"
mount --bind /mnt/c/VS/Workspace $CHROOT_DIR/mnt/c/VS/Workspace 2>/dev/null || true
mount --bind /home/haonami/android-ndk-r26b $CHROOT_DIR/opt/android-ndk-r26b 2>/dev/null || true
mount --bind /lib64 $CHROOT_DIR/lib64 2>/dev/null || true
mount --bind /usr/lib/x86_64-linux-gnu $CHROOT_DIR/usr/lib/x86_64-linux-gnu 2>/dev/null || true

chroot /home/haonami/ubuntu-arm64-full bash -c "
set -e
export LD_LIBRARY_PATH=/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/lib
NDK_CLANG='/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang'
NDK_CLANGXX='/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang++'

cd /mnt/c/VS/Workspace/atshogi-android

echo \"[1/3] Compiling C++ Bridge with Android NDK r26b...\"
rm -rf build/atshogi_oex_bin build/TopologicalUsiBridge.o
mkdir -p build
\$NDK_CLANGXX -O3 -fPIC -c src/cpp/TopologicalUsiBridge.cpp -o build/TopologicalUsiBridge.o

echo \"[2/3] Compiling Haskell and Linking via NDK...\"
mkdir -p /tmp/libndk_stubs
ar rcs /tmp/libndk_stubs/librt.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libgmp.a /tmp/libndk_stubs/libgmp.a 2>/dev/null || true
cp /usr/lib/aarch64-linux-gnu/libffi.a /tmp/libndk_stubs/libffi.a 2>/dev/null || true

ghc -O2 -threaded -fPIE -pie \\
    -pgmc=\$NDK_CLANG \\
    -pgmcxx=\$NDK_CLANGXX \\
    -pgml=\$NDK_CLANGXX \\
    -optl-static-pie \\
    -optl-L/tmp/libndk_stubs \\
    -optl-Wl,--unresolved-symbols=ignore-all \\
    -isrc/haskell \\
    src/haskell/Main.hs \\
    src/haskell/OrientedMatroid/TopologicalFFI.hs \\
    build/TopologicalUsiBridge.o \\
    -o build/atshogi_oex_bin

echo \"[3/3] Stripping Binary...\"
/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip build/atshogi_oex_bin
echo \"✔ Successfully built atshogi_oex_bin\"
"
