package com.atshogi.android

import android.app.Activity
import android.os.Bundle
import android.widget.Toast

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // 1. ホーム画面から起動された場合、ユーザーに案内用のトーストを出す
        Toast.makeText(
            applicationContext, 
            "ATShogi-OM Extreme: 将棋GUI（ShogiHome等）のエンジン設定から呼び出してください。", 
            Toast.LENGTH_LONG
        ).show()
        
        // 2. 【最重要物理ガード】即座に自身を終了させ、Theme.NoDisplay の OS 起動トランザクションを正常完了させる
        finish()
    }
}