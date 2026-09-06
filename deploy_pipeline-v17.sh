#!/bin/bash
# ==============================================================================
# ATShogi ➡ ATShogi-Android (AArch64) 一気通貫自動ビルド＆自動配備スクリプト (v17)
# ==============================================================================
set -e

export ANDROID_NDK_HOME=$HOME/android-ndk-r26b
export API=33
export WINDOWS_PROJECT_DIR="/mnt/c/VS/Workspace/atshogi-android"
export ASSETS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/assets"
export JNI_LIBS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/jniLibs/arm64-v8a"

echo "===================================================================="
echo "    ATShogi-OM Extreme V17: Fully Autonomous Build & Deploy Pipeline"
echo "===================================================================="

# 1. Makefile-v17 を使用してクロスコンパイル、定跡転送、ハッシュ動的比較を完全自律実行
make -f Makefile-v17 clean
make -f Makefile-v17 deploy_all

echo "===================================================================="
echo "    Deploy Pipeline (v17) Complete! Ready for Android Studio Run."
echo "===================================================================="
