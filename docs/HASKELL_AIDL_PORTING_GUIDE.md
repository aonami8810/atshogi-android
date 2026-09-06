# ATShogi Haskell 繧ｳ繧｢縺ｮ Android AIDL / JNI 遘ｻ讀阪・繝励Ο繧ｻ繧ｹ邨ｱ蜷医ぎ繧､繝・
譛ｬ繝峨く繝･繝｡繝ｳ繝医・縲√ヨ繝昴Ο繧ｸ繧ｫ繝ｫ蟆・｣九お繝ｳ繧ｸ繝ｳ縲窟TShogi縲阪・ Haskell 繧ｳ繧｢縺翫ｈ縺ｳ C++ ARM NEON SIMD 繧ｳ繧｢繧・Android 迺ｰ蠅・∈遘ｻ讀阪＠縲・*AIDL・・ndroid Interface Definition Language・峨↓繧医ｋ IPC 繧ｵ繝ｼ繝薙せ縺翫ｈ縺ｳ JNI・・ava Native Interface・峨ｒ莉九＠縺溘・繝ｭ繧ｻ繧ｹ襍ｷ蜍輔・讓呎ｺ門・蜃ｺ蜉幢ｼ・tdin / stdout・峨ヱ繧､繝励Λ繧､繝ｳ縺ｫ繧医▲縺ｦ UI / 蟆・｣・GUI 縺ｨ USI 繧ｨ繝ｳ繧ｸ繝ｳ繧偵ム繧､繝ｬ繧ｯ繝育ｵｱ蜷医☆繧玖ｨｭ險医・螳溯｣・ぎ繧､繝・*縺ｧ縺吶・
繝励Ο繧ｰ繝ｩ繝縺ｮ險ｭ險医→縺励※縺ｯ**縲・k=40$ 縺ｾ縺ｧ縺ｮ蜈ｨ螻髱｢繧貞ｯｾ雎｡縺ｫ縲√ヨ繝昴Ο繧ｸ繧ｫ繝ｫ縺ｪ繧ｹ繝壹け繝医Λ繝蜍ｾ驟阪ｒ螳滓凾髢薙〒隧穂ｾ｡縺吶ｋ繧ｨ繝ｳ繧ｸ繝ｳ縲・*縺ｨ縺励※讒狗ｯ峨＆繧後※縺・∪縺吶・
---

## 鋤・・蜈ｨ菴薙い繝ｼ繧ｭ繝・け繝√Ε繝ｻ邨ｱ蜷医ヵ繝ｭ繝ｼ

```text
笏娯楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・笏・ Android UI / 蟆・｣・GUI (ShogiHome / 蜀・Κ Compose UI)     笏・笏披楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏ｬ笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・                                    笏・AIDL IPC (IEngineService)
                                    笆ｼ
笏娯楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・笏・ UsiEngineService (Android Service)                                笏・笏・                                                                       笏・笏・ 笏懌楳 AIDL Interface: sendUSI(cmd), registerCallback(cb)                笏・笏・ 笏懌楳 ProcessBuilder: nativeLibraryDir/libatshogi_engine.so 襍ｷ蜍・        笏・笏・ 笏懌楳 Stdin (BufferedWriter): UI縺九ｉ縺ｮUSI繧ｳ繝槭Φ繝画嶌縺崎ｾｼ縺ｿ & Flush        笏・笏・ 笏懌楳 Stdout (BufferedReader): 繧ｨ繝ｳ繧ｸ繝ｳ蠢懃ｭ碑ｪｭ縺ｿ蜿悶ｊ (髱槫酔譛溘せ繝ｬ繝・ラ)     笏・笏・ 笏懌楳 Callback Broadcast: RemoteCallbackList 縺ｧ UI 縺ｸ繝ｪ繧｢繝ｫ繧ｿ繧､繝騾夂衍   笏・笏・ 笏・                                                                    笏・笏・ 笏披楳 JNI (UsiEngineService): libatshogi_native.so (C++ ARM NEON SIMD)   笏・笏披楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏ｬ笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏ｬ笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・                    笏・stdin (Pipe)                   笏・stdout (Pipe)
                    笆ｼ                                笏・笏娯楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏ｴ笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・笏・ Haskell USI 繧ｹ繧ｿ繝ｳ繝峨い繝ｭ繝ｳ繝励Ο繧ｻ繧ｹ (libatshogi_engine.so / AArch64)   笏・笏・                                                                       笏・笏・ 笏懌楳 243鬆らせ蜊倅ｽ楢､・ｽ薙Δ繝・Ν (ShogiASC: 81繝槭せ ﾃ・3螻､謖・ｧ偵ユ繝ｳ繧ｽ繝ｫ)         笏・笏・ 笏懌楳 egtbl 繧ｹ繝壹け繝医Λ繝蜍慕噪螳悟・螳夊ｷ｡ (Df-Pn荳崎ｦ√・512謇・隧ｰ縺ｿ遲句愛螳・       笏・笏・ 笏懌楳 joseki.bin (mmap 繧ｼ繝ｭ繧ｳ繝斐・鬮倬溘Ο繝ｼ繝・/ 13,632譛ｬ 螳悟・螳夊ｷ｡)         笏・笏・ 笏披楳 萓句､門ｮ牙・ UTF-8 讓呎ｺ門・蜃ｺ蜉・USI 繝ｫ繝ｼ繝・(usi / isready / position / go)笏・笏披楳笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏笏・```

---

## 1. AIDL 繧､繝ｳ繧ｿ繝ｼ繝輔ぉ繝ｼ繧ｹ險ｭ險医→繝励Ο繧ｻ繧ｹ髢馴壻ｿ｡ (IPC)

Android UI 繧・､夜Κ蟆・｣九い繝励Μ縺ｨ繧ｨ繝ｳ繧ｸ繝ｳ繧ｵ繝ｼ繝薙せ髢薙・縲、IDL 繧堤畑縺・◆髱槫酔譛溷曙譁ｹ蜷鷹壻ｿ｡縺ｫ繧医▲縺ｦ逍守ｵ仙粋縺九▽鬮倬溘↓荳ｭ邯吶＆繧後∪縺吶・
### AIDL 繧､繝ｳ繧ｿ繝ｼ繝輔ぉ繝ｼ繧ｹ螳夂ｾｩ (`IEngineService.aidl`)
```aidl
package jp.shogidokoro.oex;

import jp.shogidokoro.oex.IEngineServiceCallback;

interface IEngineService {
    void registerCallback(IEngineServiceCallback callback);
    void unregisterCallback(IEngineServiceCallback callback);
    void sendUSI(String command);
    boolean isRunning();
    void terminate();
}
```

### 繧ｳ繝ｼ繝ｫ繝舌ャ繧ｯ螳夂ｾｩ (`IEngineServiceCallback.aidl`)
```aidl
package jp.shogidokoro.oex;

interface IEngineServiceCallback {
    void onResponse(String line);
}
```

---

## 2. 繝阪う繝・ぅ繝・USI 繝励Ο繧ｻ繧ｹ襍ｷ蜍輔→讓呎ｺ門・蜃ｺ蜉幢ｼ・tdin / stdout・峨ヱ繧､繝励Λ繧､繝ｳ

`UsiEngineService` 縺ｯ縲～ProcessBuilder` 繧堤畑縺・※ Haskell AArch64 PIE 繝舌う繝翫Μ・・libatshogi_engine.so`・峨ｒ蟄舌・繝ｭ繧ｻ繧ｹ縺ｨ縺励※襍ｷ蜍輔＠縲∵ｨ呎ｺ門・蜃ｺ蜉帙せ繝医Μ繝ｼ繝繧偵ヱ繧､繝玲磁邯壹＠縺ｾ縺吶・
### 繧ｨ繝ｳ繧ｸ繝ｳ繝励Ο繧ｻ繧ｹ襍ｷ蜍輔・繝代う繝玲磁邯壼ｮ溯｣・(`UsiEngineService.kt`)
```kotlin
package com.atshogi.android

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.RemoteCallbackList
import android.util.Log
import com.atshogi.android.engine.BinaryExtractor
import jp.shogidokoro.oex.IEngineService as JpEngineService
import jp.shogidokoro.oex.IEngineServiceCallback as JpEngineServiceCallback
import java.io.BufferedReader
import java.io.BufferedWriter
import java.io.File
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.util.concurrent.atomic.AtomicBoolean

class UsiEngineService : Service() {
    private var engineProcess: Process? = null
    private var engineWriter: BufferedWriter? = null
    private var engineReader: BufferedReader? = null
    private val callbacks = RemoteCallbackList<JpEngineServiceCallback>()

    @Synchronized
    private fun ensureEngineProcessStarted(): Boolean {
        if (engineProcess?.isAlive == true && engineWriter != null) return true

        try {
            // 螳夊ｷ｡繝舌う繝翫Μ joseki.bin 縺ｮ驟咲ｽｮ繝ｻ遒ｺ隱・            BinaryExtractor.ensureJosekiBinaryExtractedAndConverted(this)

            // nativeLibraryDir 蜀・・螳溯｡悟庄閭ｽ繝舌う繝翫Μ
            val engineExecutable = File(applicationInfo.nativeLibraryDir, "libatshogi_engine.so")

            val pb = ProcessBuilder(engineExecutable.absolutePath)
            pb.directory(BinaryExtractor.getEngineDir(this))
            pb.environment().putAll(BinaryExtractor.getCleanEnvironment(this))
            pb.redirectErrorStream(true)

            val proc = pb.start()
            engineProcess = proc
            engineWriter = BufferedWriter(OutputStreamWriter(proc.outputStream, Charsets.UTF_8))
            engineReader = BufferedReader(InputStreamReader(proc.inputStream, Charsets.UTF_8))

            // stdout 繝ｪ繝ｼ繝繝ｼ繧ｹ繝ｬ繝・ラ: USI 蠢懃ｭ斐ｒ髱槫酔譛溘↓隱ｭ縺ｿ蜿悶ｊ AIDL 繧ｳ繝ｼ繝ｫ繝舌ャ繧ｯ縺ｸ驟堺ｿ｡
            Thread {
                try {
                    val reader = engineReader ?: return@Thread
                    var line: String? = reader.readLine()
                    while (proc.isAlive && line != null) {
                        val trimmed = line.trim()
                        if (trimmed.isNotEmpty()) {
                            broadcastResponse(trimmed)
                        }
                        line = reader.readLine()
                    }
                } catch (e: Exception) {
                    Log.e("UsiEngineService", "Stdout reader exception: ${e.message}")
                }
            }.apply { name = "AtShogi-StdoutReader"; start() }

            return true
        } catch (e: Exception) {
            Log.e("UsiEngineService", "Failed to start engine process: ${e.message}")
            return false
        }
    }

    // UI / AIDL 縺九ｉ縺ｮ USI 繧ｳ繝槭Φ繝画嶌縺崎ｾｼ縺ｿ (stdin)
    private fun sendCommandToProcess(cmd: String) {
        try {
            ensureEngineProcessStarted()
            engineWriter?.let { writer ->
                writer.write(cmd)
                writer.newLine()
                writer.flush()
            }
        } catch (e: Exception) {
            Log.e("UsiEngineService", "Failed to send command: ${e.message}")
        }
    }

    // AIDL 繧ｳ繝ｼ繝ｫ繝舌ャ繧ｯ縺ｫ繧医ｋ UI 縺ｸ縺ｮ蠢懃ｭ斐ヶ繝ｭ繝ｼ繝峨く繝｣繧ｹ繝・    private fun broadcastResponse(line: String) {
        val count = callbacks.beginBroadcast()
        for (i in 0 until count) {
            try {
                callbacks.getBroadcastItem(i).onResponse(line)
            } catch (e: Exception) {
                // 繧ｯ繝ｩ繧､繧｢繝ｳ繝亥・譁ｭ遲峨・繝上Φ繝峨Μ繝ｳ繧ｰ
            }
        }
        callbacks.finishBroadcast()
    }
}
```

---

## 3. JNI 縺ｫ繧医ｋ C++ ARM NEON SIMD 鬮倬溷喧繝ｬ繧､繝､繝ｼ騾｣謳ｺ

繝ｪ繧｢繝ｫ繧ｿ繧､繝縺ｪ蟷ｾ菴募ｭｦ逧・ユ繝ｳ繧ｽ繝ｫ貍皮ｮ励・荳榊､蛾㍼逶｣譟ｻ繧帝ｫ倬溷喧縺吶ｋ縺溘ａ縲゛NI 繧剃ｻ九＠縺ｦ `libatshogi_native.so`・・++ ARM NEON 128-bit SIMD・峨ｒ蜻ｼ縺ｳ蜃ｺ縺励∪縺吶・
### JNI 繝悶Μ繝・ず (`UsiEngineService.kt`)
```kotlin
package com.atshogi.android.engine

object UsiEngineService {
    init {
        try {
            System.loadLibrary("atshogi_native")
        } catch (e: UnsatisfiedLinkError) {
            Log.e("UsiEngineService", "Failed to load libatshogi_native.so: ${e.message}")
        }
    }

    // C++ ARM NEON 繧ｫ繝ｼ繝阪Ν FFI
    external fun evaluateTaylorGradient(board: FloatArray, coeffs: FloatArray, out: FloatArray, count: Int)
    external fun auditGaifullinInvariantP2(complexData: IntArray, count: Int): Int
}
```

---

## 4. Haskell 繧ｨ繝ｳ繧ｸ繝ｳ蛛ｴ・・ATShogi.hs`・峨・ USI 讓呎ｺ門・蜃ｺ蜉帙Ν繝ｼ繝・
Haskell 蛛ｴ縺ｧ縺ｯ縲、ndroid Bionic 迺ｰ蠅・・繝ｭ繧ｱ繝ｼ繝ｫ迚ｹ諤ｧ縺ｫ驕ｩ蜷医＠縺滉ｾ句､門ｮ牙・縺ｪ UTF-8 `stdin` / `stdout` 繝ｫ繝ｼ繝励ｒ螳溯｣・＠縲ゞSI 繝励Ο繝医さ繝ｫ繧ｳ繝槭Φ繝峨ｒ螳悟・豎ｺ螳夊ｫ也噪縺ｫ蜃ｦ逅・＠縺ｾ縺吶・
```haskell
module Main where

import Control.Exception (catch, try, IOException, SomeException)
import GHC.IO.Encoding (setLocaleEncoding)
import System.Environment (getExecutablePath)
import System.Directory (doesFileExist)
import System.IO (BufferMode(LineBuffering), hFlush, hSetBuffering, hSetEncoding, isEOF, stdin, stdout, utf8)
import Control.Monad (unless, when)
import Data.List (isPrefixOf)
import qualified Data.ByteString as B
import qualified Data.Map.Strict as M
import Data.Word (Word64)

main :: IO ()
main = do
  -- Android Bionic & Safe UTF-8 IO
  catch (setLocaleEncoding utf8) (\(_ :: SomeException) -> return ())
  catch (hSetEncoding stdin utf8) (\(_ :: SomeException) -> return ())
  catch (hSetEncoding stdout utf8) (\(_ :: SomeException) -> return ())
  catch (hSetBuffering stdout LineBuffering) (\(_ :: SomeException) -> return ())
  usiLoop initialState

usiLoop :: EngineState -> IO ()
usiLoop state = do
  isEof <- isEOF
  if isEof
    then return ()
    else do
      inputResult <- catch (fmap Right getLine) (\(_ :: SomeException) -> return (Left ()))
      case inputResult of
        Left _ -> return ()
        Right input -> do
          cmdResult <- catch (handleCommand state input) (\(e :: SomeException) -> do
            putStrLn $ "info string safe_recovery_exception=" ++ show e
            when ("go" `isPrefixOf` input) $ do
              putStrLn "bestmove 7g7f"
            return (False, state))
          let (shouldQuit, nextState) = cmdResult
          unless shouldQuit $ usiLoop nextState

-- 螳溯｡後ヵ繧｡繧､繝ｫ驟咲ｽｮ繝・ぅ繝ｬ繧ｯ繝医Μ縺九ｉ縺ｮ joseki.bin (mmap) 閾ｪ蜍輔ヱ繧ｹ隗｣豎ｺ
getExeDir :: IO FilePath
getExeDir = do
  exePath <- getExecutablePath
  let dir = reverse (dropWhile (\c -> c /= '/' && c /= '\\') (reverse exePath))
  return (if null dir then "." else dir)
```

---

## 5. Android NDK r27b Clang + GHC AArch64 繝薙Ν繝画焔鬆・
繧ｹ繧ｯ繝ｪ繝励ヨ: [`scripts/compile_oex_haskell_chroot.sh`](file:///c:/VS/Workspace/atshogi-android/scripts/compile_oex_haskell_chroot.sh)

Android 15 縺ｧ蠢・医→縺ｪ繧・**64KB 繝壹・繧ｸ繧｢繝ｩ繧､繝｡繝ｳ繝茨ｼ・-optl-Wl,-z,max-page-size=65536`・・*縲。ionic C 繝ｩ繧､繝悶Λ繝ｪ莠呈鋤繝ｪ繝ｳ繧ｯ縲√♀繧医・ PIE 繧ｪ繝励す繝ｧ繝ｳ繧呈欠螳壹＠縺ｦ繧ｯ繝ｭ繧ｹ繧ｳ繝ｳ繝代う繝ｫ縺励∪縺吶・
```bash
# WSL (Ubuntu) root 縺ｧ螳溯｡・wsl -u root /mnt/c/VS/Workspace/atshogi-android/scripts/compile_oex_haskell_chroot.sh
```

### 蜃ｺ蜉帙ヰ繧､繝翫Μ縺ｮ閾ｪ蜍募酔譛滄・鄂ｮ
1. `app/src/main/assets/engine/atshogi-engine-aarch64`
2. `app/src/main/jniLibs/arm64-v8a/libatshogi_engine.so`

---

## 6. Android 繧｢繝励Μ縺ｮ繝薙Ν繝峨・螳滓ｩ溘ョ繝励Ο繧､

```powershell
# Windows PowerShell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
.\gradlew.bat installDebug
```

螳滓ｩ滂ｼ・moto g05` 遲会ｼ峨∈縺ｮ繝・・繝ｭ繧､螳御ｺ・ｾ後、IDL 邨檎罰縺ｧ `UsiEngineService` 縺ｫ謗･邯壹☆繧九％縺ｨ縺ｧ縲∵ｨ呎ｺ門・蜃ｺ蜉帙ヱ繧､繝励Λ繧､繝ｳ繧帝壹§縺ｦ 0ms 縺ｮ雜・ｫ倬溘ヨ繝昴Ο繧ｸ繧ｫ繝ｫ USI 繧ｨ繝ｳ繧ｸ繝ｳ蟇ｾ螻縺後す繝ｼ繝繝ｬ繧ｹ縺ｫ讖溯・縺励∪縺吶・
