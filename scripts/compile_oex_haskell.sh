#!/bin/bash
# compile_oex_haskell.sh
# ATShogi Haskell Core AArch64 Compilation Script for Android OEX
set -e

# Execute AArch64 compilation via chroot environment
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$SCRIPT_DIR/compile_oex_haskell_chroot.sh"

ASSET_SRC="/home/haonami/ubuntu-arm64-full/tmp/atshogi_engine_aarch64"
if [ -f "$ASSET_SRC" ]; then
    cp "$ASSET_SRC" "/mnt/c/VS/Workspace/atshogi-android/app/src/main/assets/libatshogi_engine.so"
    echo "Successfully updated /mnt/c/VS/Workspace/atshogi-android/app/src/main/assets/libatshogi_engine.so"
fi
echo "Done."
