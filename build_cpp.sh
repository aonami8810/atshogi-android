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
NDK_CLANGXX='/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang++'

cd /mnt/c/VS/Workspace/atshogi-android

echo \"[1/2] Compiling C++17 Unified USI Engine with Android NDK r26b...\"
mkdir -p build
rm -f build/atshogi_oex_bin

\$NDK_CLANGXX -O3 -std=c++17 -fPIE -pie -static-libstdc++ -Wl,-z,max-page-size=65536 -Wl,-z,common-page-size=65536 src/cpp/atshogi_usi_engine.cpp -o build/atshogi_oex_bin

echo \"[2/2] Stripping Binary...\"
/opt/android-ndk-r26b/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip build/atshogi_oex_bin
echo \"✔ Successfully built atshogi_oex_bin\"
"
