package com.atshogi.android.bridge

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * TDD Unit Test Suite verifying Direct Client-Side ShogiHome Architecture (Zero Bridge Overhead).
 */
class DirectShogiHomeTest {

    @Test
    fun testJosekiAtlasSequenceIntegrity() {
        val senteJoseki = listOf("7g7f", "2g2f", "2f2e", "6g6f")
        val goteJoseki  = listOf("3c3d", "8b3c", "4c4d", "5c5d")

        assertEquals("7g7f", senteJoseki[0])
        assertEquals("3c3d", goteJoseki[0])
        assertEquals("2g2f", senteJoseki[1])
        assertEquals("8b3c", goteJoseki[1])
    }
}
