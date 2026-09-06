#!/bin/bash
# ==============================================================================
# ATShogi ➡ ATShogi-Android (AArch64) 一気通貫自動ビルド＆自動配備スクリプト (v23)
# ==============================================================================
set -e

export ANDROID_NDK_HOME=$HOME/android-ndk-r26b
export API=33
export WINDOWS_PROJECT_DIR="/mnt/c/VS/Workspace/atshogi-android"
export ASSETS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/assets"
export JNI_LIBS_DIR="$WINDOWS_PROJECT_DIR/app/src/main/jniLibs/arm64-v8a"

echo "===================================================================="
echo "    ATShogi-OM Extreme V23: Fully Autonomous Build & Deploy Pipeline"
echo "===================================================================="

# Makefile-v23 を用いてクロスコンパイル、アセットハッシュ検証を実行
make -f Makefile-v23 clean
make -f Makefile-v23 deploy_all

echo "===================================================================="
echo "    Deploy Pipeline (v23) Complete! Ready for Android Studio Run."
echo "===================================================================="
