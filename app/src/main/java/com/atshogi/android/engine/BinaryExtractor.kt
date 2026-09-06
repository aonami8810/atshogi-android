package com.atshogi.android.engine

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

/**
 * Android 15 (Target SDK 35 / 16KB-64KB Page Size / W^X) に準拠した
 * 一本道ネイティブ USI エンジン（libatshogi_engine.so）解決ユーティリティ。
 */
object BinaryExtractor {

    private const val TAG = "BinaryExtractor"
    private const val ENGINE_DIR_NAME = "engine_core"

    /**
     * エンジン実行用プロセスのクリーンな環境変数を構築する。
     */
    fun getCleanEnvironment(context: Context): Map<String, String> {
        val masterDir = getEngineDir(context)
        val env = HashMap<String, String>()
        env["LANG"] = "C.UTF-8"
        env["LC_ALL"] = "C.UTF-8"
        env["GHC_CHARENC"] = "UTF-8"
        env["HASKELL_LOCALE_ENCODING"] = "UTF-8"
        env["HOME"] = masterDir.absolutePath
        env["TMPDIR"] = context.cacheDir.absolutePath
        env["USER"] = "atshogi"
        env["LD_LIBRARY_PATH"] = context.applicationInfo.nativeLibraryDir
        return env
    }

    /**
     * エンジンディレクトリを取得（存在しない場合は作成）。
     */
    fun getEngineDir(context: Context): File {
        val dir = File(context.filesDir, ENGINE_DIR_NAME)
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    /**
     * assets/engine 配下の定跡データファイル (.atmp / .bin) を展開する。
     */
    fun extractAssetsIfNeeded(context: Context) {
        val targetDir = getEngineDir(context)
        try {
            val assetManager = context.assets
            val files = assetManager.list("engine") ?: return
            for (filename in files) {
                val outFile = File(targetDir, filename)
                val assetDescriptor = try { assetManager.openFd("engine/$filename") } catch (e: Exception) { null }
                val assetLen = assetDescriptor?.length ?: -1L
                assetDescriptor?.close()

                if (!outFile.exists() || outFile.length() != assetLen) {
                    Log.i(TAG, "Extracting updated asset: engine/$filename -> ${outFile.absolutePath}")
                    assetManager.open("engine/$filename").use { input ->
                        FileOutputStream(outFile).use { output ->
                            input.copyTo(output)
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to extract engine assets", e)
        }
    }

    /**
     * 一本道アーキテクチャ: nativeLibraryDir 配下の libatshogi_engine.so を直接解決する。
     */
    fun getEngineExecutable(context: Context): File {
        extractAssetsIfNeeded(context)
        return File(context.applicationInfo.nativeLibraryDir, "libatshogi_engine.so")
    }
}
