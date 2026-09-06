package com.atshogi.android

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.RemoteCallbackList
import android.util.Log
import java.io.*

import shogi.oex.IEngineService
import shogi.oex.IEngineServiceCallback

class EngineService : Service() {
    private val TAG = "ATShogiEngineService"
    private var engineProcess: Process? = null
    private var processInput: BufferedWriter? = null
    private var processOutput: BufferedReader? = null
    private var outputReaderThread: Thread? = null

    private val callbackList = RemoteCallbackList<IEngineServiceCallback>()

    private val binder = object : IEngineService.Stub() {
        override fun registerCallback(callback: IEngineServiceCallback?) {
            if (callback != null) {
                callbackList.register(callback)
                Log.i(TAG, "OEX Client registered successfully.")
            }
        }

        override fun unregisterCallback(callback: IEngineServiceCallback?) {
            if (callback != null) {
                callbackList.unregister(callback)
                Log.i(TAG, "OEX Client unregistered.")
            }
        }

        override fun writeCommand(cmd: String?) {
            if (cmd != null) {
                writeToEngine(cmd)
            }
        }

        override fun getInfo(): String {
            return """
            {
                "name": "ATShogi-OM Extreme V6",
                "author": "aonami8810",
                "description": "Topological Morse Shogi USI Engine"
            }
            """.trimIndent()
        }
    }

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "Service onCreate. Spawning C++ USI process dynamically...")
        try {
            extractJosekiIfNeeded()
            startEngineProcess()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to spawn native USI engine process", e)
            stopSelf()
        }
    }

    override fun onBind(intent: Intent?): IBinder {
        Log.i(TAG, "OEX onBind Bind triggered with shogi.oex.ENGINE")
        return binder
    }

    private fun extractJosekiIfNeeded(): File {
        val targetFile = File(filesDir, "static_joseki.bin")
        assets.open("static_joseki.bin").use { inputStream ->
            FileOutputStream(targetFile).use { outputStream ->
                inputStream.copyTo(outputStream)
            }
        }
        Log.i(TAG, "Successfully extracted static_joseki.bin (${targetFile.length()} bytes) to filesDir")
        return targetFile
    }

    private fun startEngineProcess() {
        val appLibraryDir = File(applicationInfo.nativeLibraryDir)
        val executableFile = File(appLibraryDir, "libatshogi_oex_bin.so")

        if (!executableFile.exists()) {
            throw FileNotFoundException("Native USI binary not found in JNI library dir")
        }

        val pb = ProcessBuilder(executableFile.absolutePath)
        pb.directory(filesDir)
        pb.redirectErrorStream(true)

        engineProcess = pb.start()
        processInput = BufferedWriter(OutputStreamWriter(engineProcess!!.outputStream, "UTF-8"))
        processOutput = BufferedReader(InputStreamReader(engineProcess!!.inputStream, "UTF-8"))

        outputReaderThread = Thread {
            try {
                var line: String?
                while (true) {
                    line = processOutput?.readLine() ?: break
                    broadcastToClients(line)
                }
            } catch (e: IOException) {
                Log.i(TAG, "Engine output stream closed safely.")
            } finally {
                Log.i(TAG, "Engine-Output-Reader thread exited safely.")
            }
        }
        outputReaderThread?.start()
    }

    private fun writeToEngine(cmd: String) {
        Thread {
            try {
                processInput?.write(cmd + "\n")
                processInput?.flush()
            } catch (e: IOException) {
                Log.e(TAG, "Failed to write USI command to engine process", e)
            }
        }.start()
    }

    private fun broadcastToClients(cmd: String) {
        val count = callbackList.beginBroadcast()
        for (i in 0 until count) {
            try {
                callbackList.getBroadcastItem(i).onReceiveResponse(cmd)
            } catch (e: Exception) {
                Log.e(TAG, "Callback broadcast transaction failed", e)
            }
        }
        callbackList.finishBroadcast()
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "Service onDestroy. Tearing down C++ process cleanly...")
        try {
            processInput?.close()
            processOutput?.close()
        } catch (ignored: Exception) {}
        engineProcess?.destroy()
        engineProcess = null
        outputReaderThread?.interrupt()
        callbackList.kill()
    }
}