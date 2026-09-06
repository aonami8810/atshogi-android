package com.atshogi.android.bridge

import org.junit.Assert.assertEquals
import org.junit.Test
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * TDD Unit Test verifying Strict USI Queue Serialization.
 */
class SerializedUsiQueueTest {

    @Test
    fun testSequentialExecutionOrderInSingleThreadExecutor() {
        val executor = Executors.newSingleThreadExecutor()
        val processedQueue = ConcurrentLinkedQueue<Int>()

        for (i in 1..100) {
            val num = i
            executor.execute {
                processedQueue.add(num)
            }
        }

        executor.shutdown()
        executor.awaitTermination(2, TimeUnit.SECONDS)

        assertEquals(100, processedQueue.size)
        var expected = 1
        for (item in processedQueue) {
            assertEquals(expected, item)
            expected++
        }
    }
}
