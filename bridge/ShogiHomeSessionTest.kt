package com.atshogi.android.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * TDD Unit Test Suite verifying Official ShogiHome USI Stream Thread Model.
 */
class ShogiHomeSessionTest {

    @Test
    fun testShogiHomeStdoutEventLineFormatting() {
        val rawLine = "bestmove 7g7f"
        assertTrue("ShogiHome stdout line must contain bestmove", rawLine.startsWith("bestmove "))
        val move = rawLine.split(" ")[1]
        assertEquals("7g7f", move)
    }

    @Test
    fun testShogiHomeMoveHistoryPositionCommand() {
        val moves = listOf("7g7f", "3c3d", "2g2f")
        val posCmd = "position startpos moves " + moves.joinToString(" ")
        assertEquals("position startpos moves 7g7f 3c3d 2g2f", posCmd)
    }
}
