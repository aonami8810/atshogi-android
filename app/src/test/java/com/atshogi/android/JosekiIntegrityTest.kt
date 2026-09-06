package com.atshogi.android

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.security.MessageDigest

class JosekiIntegrityTest {

    @Test
    fun testJosekiFileIntegrityAndNoAiFaking() {
        val projectDir = File("").absoluteFile
        val assetsDir = File(projectDir, "src/main/assets")
        val josekiFile = File(assetsDir, "static_joseki.bin")

        // 1. 存在検証
        assertTrue("🚨 [ERROR] static_joseki.bin must exist in assets!", josekiFile.exists())

        // 2. 厳格なファイルサイズ検証 (1,048,576 バイト = 1.0 MB)
        assertEquals(
            "🚨 [ERROR] static_joseki.bin size must be exactly 1,048,576 bytes!", 
            1048576L, 
            josekiFile.length()
        )

        // 3. ゼロパディング（全ビット0x00）による偽装ハック自動検知
        val bytes = josekiFile.readBytes()
        
        // 末尾1000バイトをサンプリングし、0x00の出現数をカウント
        val sampleSize = 1000
        val lastBytes = bytes.takeLast(sampleSize)
        val zeroCount = lastBytes.count { it == 0.toByte() }

        // 本物のパック定跡データであれば、末尾1000バイトがほぼすべて0x00（ゼロ）で埋まることは数学的にあり得ない
        assertTrue(
            "🚨 [ERROR] Zero-padding detected! The file seems to be faked with dummy trailing zeros.",
            zeroCount < 900
        )

        // 4. SHA-256 ハッシュ値の算出とコンソール出力
        val md = MessageDigest.getInstance("SHA-256")
        val digest = md.digest(bytes)
        val hashString = digest.joinToString("") { "%02x".format(it) }

        println("🎉 [INTEGRITY SUCCESS] static_joseki.bin is clean & authentic!")
        println("   -> Size: ${josekiFile.length()} bytes")
        println("   -> SHA-256: $hashString")
        println("   -> Trailing Zero Check: PASS ($zeroCount/1000 zeros)")
    }
}
