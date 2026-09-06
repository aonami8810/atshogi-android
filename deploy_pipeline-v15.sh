#!/bin/bash
# ==============================================================================
# ATShogi ➡ ATShogi-Android (AArch64) 一気通貫自動ビルド＆自動配備スクリプト (v15)
# ==============================================================================
set -e

export ANDROID_NDK_HOME=$HOME/android-ndk-r26b
export API=33
export WINDOWS_PROJECT_DIR="/mnt/c/VS/Workspace/atshogi-android"
export ASSETS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/assets"
export JNI_LIBS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/jniLibs/arm64-v8a"

echo "===================================================================="
echo "    ATShogi-OM Extreme V15: Fully Autonomous Build & Deploy Pipeline"
echo "===================================================================="

# 1. Makefile を使用してクロスコンパイル、定跡自律生成、転送、およびハッシュ動的比較を完全自律実行
make -f Makefile-v15 clean
make -f Makefile-v15 deploy_all

echo "===================================================================="
echo "    Deploy Pipeline (v15) Complete! Ready for Android Studio Run."
echo "===================================================================="
