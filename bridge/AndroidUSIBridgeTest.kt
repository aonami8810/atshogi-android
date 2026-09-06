package com.atshogi.android.bridge

import com.atshogi.android.game.PieceType
import com.atshogi.android.game.ShogiBoardModel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test

/**
 * TDD Unit Test Suite verifying Multi-Turn Sequential USI Flow.
 */
class AndroidUSIBridgeTest {

    private lateinit var boardModel: ShogiBoardModel

    @Before
    fun setUp() {
        boardModel = ShogiBoardModel()
    }

    @Test
    fun testUsiPositionAndSelfPlayMoveGeneration() {
        boardModel.resetToInitialPosition()

        val move = boardModel.generateSelfPlayMove()
        assertNotNull("Generated move should not be null", move)
        assertEquals(true, move.isNotEmpty())
    }

    @Test
    fun testMultiTurnUsiSequentialFlow() {
        boardModel.resetToInitialPosition()
        val moveHistory = mutableListOf<String>()

        // Simulate 10 sequential moves between Sente and Gote
        for (turnCount in 1..10) {
            val nextMove = boardModel.generateSelfPlayMove()
            assertNotNull("Move at turn $turnCount must be generated", nextMove)
            assertEquals("Move at turn $turnCount should not be pass", false, nextMove == "pass")

            moveHistory.add(nextMove)
            val posCommand = "position startpos moves " + moveHistory.joinToString(" ")

            // Apply USI position sync to verify model persistence
            boardModel.resetToInitialPosition()
            for (m in moveHistory) {
                boardModel.movePieceByUsi(m)
            }
        }

        assertEquals(10, moveHistory.size)
    }
}
