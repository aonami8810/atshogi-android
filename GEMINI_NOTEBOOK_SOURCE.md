# ATShogi (v12) Specifications & Status for Gemini Notebook

## 1. 仕様 (Specifications)
- **完全自律型・動的USIエンジン**: cshogi のロジックを移植し、盤面管理・王手判定・合法手生成を100%動的に行う。
- **定跡マッピングプロトコル**: 1MBの \static_joseki.bin\ (Windows版から生成されたFloat実数ポテンシャル) を mmap でキャッシュへマウント。
- **最急降下法による最善手探索**: 合法手群から現在の「絶対手順数(move_index)」に完全一致する手を検索し、Zobristハッシュの誤爆を排除した上で、真のポテンシャル値(実数)を比較する。

## 2. 設計 (Design)
- **C++17 Native Engine (v12)**: \pp/src/main/cpp/atshogi_usi_engine.cpp\ に実装。
- **NDK AArch64 (PIE + Static)**: Android実機(ShogiHome)で最速動作するための静的ビルド。
- **厳格なハッシュ検証 (v20 Pipeline)**: \deploy_pipeline.sh\ において \static_joseki.bin\ のSHA-256ハッシュ(d46...\)とファイルサイズ(1048576)を100%検証する。

## 3. 実装 (Implementation)
- 居玉投了バグの根絶: Zobristハッシュの剰余アクセスのすれ違いを廃止し、「合法手順と手順数インデックスの動的一致マッピング」を実装済み。
- Android Studio (Gradle) 上での \ssembleDebug\ および \	estDebugUnitTest\ 連携を完全自動化済み。

## 4. TODOs
- [ ] 定跡外の局面(target_state_idx=99999)に突入した際の、動的推論アルゴリズム(または MERAテンソル評価)の統合。
- [ ] 中盤・終盤の自己対局データに基づくリアルタイム形勢判断ロジックの実装。
