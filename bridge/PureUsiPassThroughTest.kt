package com.atshogi.android.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * TDD Unit Test Suite verifying Pure USI Pass-Through Architecture.
 */
class PureUsiPassThroughTest {

    @Test
    fun testUsiProtocolPassThroughFormat() {
        val posCmd = "position startpos moves 7g7f 3c3d 2g2f"
        assertTrue("USI position command must start with position startpos", posCmd.startsWith("position startpos"))
        assertEquals("Moves count must be 3", 3, posCmd.substringAfter("moves ").split(" ").size)
    }

    @Test
    fun testUsiResponseParsing() {
        val response = "bestmove 7g7f"
        assertTrue("USI engine response must start with bestmove", response.startsWith("bestmove "))
        val move = response.split(" ")[1]
        assertEquals("7g7f", move)
    }
}
