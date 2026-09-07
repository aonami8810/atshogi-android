#!/bin/bash
# ====================================================================
# ATShogi-OM Extreme V26: Fully Autonomous Build & Deploy Pipeline
# ====================================================================
set -e

echo "===================================================================="
echo "    ATShogi-OM Extreme V26: Fully Autonomous Build & Deploy Pipeline"
echo "===================================================================="

# 1. クリーンアップとビルド
make -f Makefile-v26 clean
make -f Makefile-v26 deploy_all

echo "===================================================================="
echo "    Deploy Pipeline (v26) Complete! Ready for Android Studio Run."
echo "===================================================================="
