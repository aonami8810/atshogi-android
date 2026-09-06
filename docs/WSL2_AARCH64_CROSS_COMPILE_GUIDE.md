# ATShogi: WSL 2 (AArch64) クロスコンパイル環境 & 完全分離ビルドシステム設計書

## 1. クロスコンパイルの極意（なぜ単純な GHC クロスコンパイラを使わないのか）

Haskell と NDK (C++) を組み合わせたクロスコンパイルにおいて、Windows (x86_64) 上の GHC から直接 Android (AArch64) バイナリを吐き出そうとすると、以下の「死の罠」にハマります。

1. **Glibc vs Bionic の衝突**: GHC のランタイム (RTS) は通常 glibc に依存しますが、Android は Bionic libc を使用するため、リンクフェーズで `__fprintf_chk` 等のシンボルエラーが多発します。
2. **GHC LLVM ツールチェーン不整合**: GHC の `-fllvm` と NDK 側の `opt` や `llc` のバージョンが噛み合わず、最適化フェーズ (Exit code 127) でクラッシュします。
3. **NEON / SIMD の ABI 不整合**: C++ の `std::` 空間と Haskell 側の FFI 境界での呼び出し規約や構造体のパディングが、ホストとターゲット間で食い違います。

これらを完全に回避するため、本プロジェクトでは **「WSL 上に QEMU AArch64 のフル chroot (Ubuntu) を構築し、その中で NDK Clang と ネイティブ AArch64 GHC を動作させてリンクフェーズを分離する」** という極めて高度かつ確実な手法を採用しています。

---

## 2. ビルドパイプラインの全体像 (android_build.sh)

ビルドは `atshogi-android` ワークスペースにある [`android_build.sh`](file:///c:/VS/Workspace/atshogi-android/android_build.sh) によって自動化されています。

```bash
# Windows の PowerShell または CMD から以下を実行
wsl -u root -- bash android_build.sh
```

### 【Step 0】 Chroot とワークスペースのマウント
WSL 側の `/home/haonami/ubuntu-arm64-full` (QEMU AArch64 chroot 環境) に対して、Windows のワークスペース (`/mnt/c/VS/Workspace`) と x86_64 ホストのライブラリ (`/lib64`, `/usr/lib/x86_64-linux-gnu`) をマウントし、`chroot` コマンドで内部に侵入します。

### 【Step 1】 C++ / NEON カーネルの単体ビルド (.so)
Haskell のコンパイルを巻き込まず、C++ 側だけで NDK の `aarch64-linux-android35-clang++` を使用して共有ライブラリ `libatshogicpp.so` をビルドします。
また、GHC RTS が必要とする libc シンボルの Bionic 向けスタブ (`android_bionic_compat.c`) は、共有ライブラリに入れるとリンク解決に失敗するため、別途 `android_bionic_compat.o` という静的オブジェクトとして単体コンパイルします。

### 【Step 2】 Haskell のコンパイルと最終リンク
ここが最大のポイントです。最新の純粋な Haskell 理論実装は `atshogi` ワークスペース (`c:\VS\Workspace\atshogi\ATShogi.hs`) に存在するため、`-i/mnt/c/VS/Workspace/atshogi` を指定してそちらからソースを読み込みます。

- `-fllvm` は使用せず、AArch64 ネイティブの **NCG (Native Code Generator)** に任せます。
- `android_bionic_compat.o` と `libatshogicpp.so` を GHC に直接渡し、C++ の機能 (`cshogi_from_sfen` や C++ NEON の計算カーネル) を静的/動的に結びつけます。
- `-optl-Wl,--unresolved-symbols=ignore-in-shared-libs` を用いて、共有ライブラリ内の未解決シンボル（Android 実機側で提供されるもの等）によるリンクエラーを回避します。

### 【Step 3】 ストリップと Assets デプロイ
NDK の `llvm-strip` を用いて PIE バイナリからデバッグシンボルを削ぎ落とし、サイズを軽量化します。
最終的に `app/src/main/assets/engine/atshogi-engine-aarch64` として配置され、Android Studio 側のビルド (APK/AAB) に自動で取り込まれる状態になります。

---

## 3. オープンな課題と回避策 (Stubs)

- **OpenCL / GPU 処理**: 現在の Android 側 C++ 実装では OpenCL の実装が未着手（NEON 置き換え）のため、Haskell 側から呼ばれる `runMirrorBatchOpenCL` や `gpuVramMateLookup` に対しては、`cshogi_core.c` の末尾で `extern "C"` を用いた空のスタブ関数（ダミー実装）を定義することでリンクエラーを回避しています。

---

## 4. ファイル構成まとめ

| ファイルパス | 役割 |
|---|---|
| [`android_build.sh`](file:///c:/VS/Workspace/atshogi-android/android_build.sh) | 【統括】 WSL2 chroot を用いた分離ビルドスクリプト |
| `atshogi/ATShogi.hs` | 最新の位相数学的将棋理論 (Haskell 実装) |
| `atshogi-android/app/src/main/cpp/cshogi_core.c` | Android 向け FFI インターフェース (C API) |
| `atshogi-android/app/src/main/cpp/android_bionic_compat.c` | GHC と Android Bionic libc の互換スタブ |
| `atshogi-android/app/src/main/assets/engine/atshogi-engine-aarch64` | 最終出力される Android AArch64 ネイティブ PIE バイナリ |
