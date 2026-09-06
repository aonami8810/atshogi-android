#!/bin/bash
set -e

CHROOT_DIR="/home/haonami/ubuntu-arm64-full"
mount --bind /mnt/c/VS/Workspace $CHROOT_DIR/mnt/c/VS/Workspace 2>/dev/null || true

chroot /home/haonami/ubuntu-arm64-full bash -c "
set -e
cd /mnt/c/VS/Workspace/atshogi-android

echo \"[1/3] Compiling C++ Bridge...\"
rm -rf build/atshogi_oex_bin build/TopologicalUsiBridge.o
mkdir -p build
g++ -O3 -fPIC -static -c src/cpp/TopologicalUsiBridge.cpp -o build/TopologicalUsiBridge.o

echo \"[2/3] Compiling Haskell and Fully Static Linking...\"
ghc -O2 -threaded -static -optl-static -isrc/haskell \\
    src/haskell/Main.hs \\
    src/haskell/OrientedMatroid/TopologicalFFI.hs \\
    build/TopologicalUsiBridge.o \\
    -lstdc++ -o build/atshogi_oex_bin

echo \"[3/3] Stripping Binary...\"
strip build/atshogi_oex_bin
echo \"✔ Successfully built static atshogi_oex_bin\"
"
