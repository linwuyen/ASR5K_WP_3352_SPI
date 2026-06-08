/*
 * spi_fifo.c
 *
 * M2 SPI FIFO buffer implementation and self-contained validation tests.
 */

#include "spi_fifo.h"
#include <stddef.h>

/* Global FIFO test results for CCS Expressions visibility */
typedef struct {
    bool test1_pass;
    bool test2_pass;
    bool test3_pass;
    bool test4_pass;
    bool test5_pass;
    bool test6_pass;
    uint16_t failed_step;
} FIFO_Test_Result_t;

#pragma DATA_SECTION(g_fifoTestResult, "spi_fifo_state")
volatile FIFO_Test_Result_t g_fifoTestResult = {
    .test1_pass = false,
    .test2_pass = false,
    .test3_pass = false,
    .test4_pass = false,
    .test5_pass = false,
    .test6_pass = false,
    .failed_step = 0U
};

/* FIFO buffer instance for testing */
#pragma DATA_SECTION(s_testFifo, "spi_fifo_state")
static SPI_FIFO_t s_testFifo;

/**
 * @brief Initialize the FIFO instance
 */
void SPI_FIFO_Init(SPI_FIFO_t *fifo)
{
    uint16_t i;
    if (fifo == NULL) {
        return;
    }

    fifo->head = 0U;
    fifo->tail = 0U;
    fifo->count = 0U;
    fifo->pushCount = 0UL;
    fifo->popCount = 0UL;
    fifo->overflowCount = 0UL;
    fifo->underflowCount = 0UL;
    fifo->maxDepth = 0U;

    for (i = 0U; i < SPI_FIFO_SIZE; i++) {
        fifo->buffer[i].cmd = 0U;
        fifo->buffer[i].data = 0U;
    }
}

/**
 * @brief Push a frame into the FIFO. Prevents overwriting if full.
 */
bool SPI_FIFO_Push(SPI_FIFO_t *fifo, const SPI_FRAME_t *frame)
{
    if (fifo == NULL || frame == NULL) {
        return false;
    }

    if (fifo->count >= SPI_FIFO_SIZE) {
        fifo->overflowCount++;
        return false;
    }

    fifo->buffer[fifo->head] = *frame;
    fifo->head = (fifo->head + 1) % SPI_FIFO_SIZE;
    fifo->count++;
    fifo->pushCount++;

    if (fifo->count > fifo->maxDepth) {
        fifo->maxDepth = fifo->count;
    }

    return true;
}

/**
 * @brief Pop a frame from the FIFO. Prevents invalid pop if empty.
 */
bool SPI_FIFO_Pop(SPI_FIFO_t *fifo, SPI_FRAME_t *frame)
{
    if (fifo == NULL || frame == NULL) {
        return false;
    }

    if (fifo->count == 0U) {
        fifo->underflowCount++;
        return false;
    }

    *frame = fifo->buffer[fifo->tail];
    fifo->tail = (fifo->tail + 1) % SPI_FIFO_SIZE;
    fifo->count--;
    fifo->popCount++;

    return true;
}

/**
 * @brief Run the self-contained M2 FIFO test suite.
 * Verifies all 6 required criteria.
 */
bool FIFO_Test_Run(void)
{
    SPI_FIFO_t *fifo = &s_testFifo;
    SPI_FRAME_t txFrame;
    SPI_FRAME_t rxFrame;
    uint16_t i;

    /* Reset global results */
    g_fifoTestResult.test1_pass = false;
    g_fifoTestResult.test2_pass = false;
    g_fifoTestResult.test3_pass = false;
    g_fifoTestResult.test4_pass = false;
    g_fifoTestResult.test5_pass = false;
    g_fifoTestResult.test6_pass = false;
    g_fifoTestResult.failed_step = 0U;

    /* -------------------------------------------------------------------------
     * 1. Push 64 / Pop 64 / order preserved
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    for (i = 0U; i < 64U; i++) {
        txFrame.cmd = 0x1000U + i;
        txFrame.data = 0xA000U + i;
        if (!SPI_FIFO_Push(fifo, &txFrame)) {
            g_fifoTestResult.failed_step = 101U;
            return false;
        }
    }

    if (fifo->count != 64U || fifo->pushCount != 64UL || fifo->maxDepth != 64U) {
        g_fifoTestResult.failed_step = 102U;
        return false;
    }

    for (i = 0U; i < 64U; i++) {
        if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
            g_fifoTestResult.failed_step = 103U;
            return false;
        }
        if (rxFrame.cmd != (0x1000U + i) || rxFrame.data != (0xA000U + i)) {
            g_fifoTestResult.failed_step = 104U;
            return false;
        }
    }

    if (fifo->count != 0U || fifo->popCount != 64UL) {
        g_fifoTestResult.failed_step = 105U;
        return false;
    }
    g_fifoTestResult.test1_pass = true;

    /* -------------------------------------------------------------------------
     * 2. Push 100 / Pop 100 / order preserved
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    for (i = 0U; i < 100U; i++) {
        txFrame.cmd = 0x2000U + i;
        txFrame.data = 0xB000U + i;
        if (!SPI_FIFO_Push(fifo, &txFrame)) {
            g_fifoTestResult.failed_step = 201U;
            return false;
        }
    }

    if (fifo->count != 100U || fifo->pushCount != 100UL || fifo->maxDepth != 100U) {
        g_fifoTestResult.failed_step = 202U;
        return false;
    }

    for (i = 0U; i < 100U; i++) {
        if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
            g_fifoTestResult.failed_step = 203U;
            return false;
        }
        if (rxFrame.cmd != (0x2000U + i) || rxFrame.data != (0xB000U + i)) {
            g_fifoTestResult.failed_step = 204U;
            return false;
        }
    }

    if (fifo->count != 0U || fifo->popCount != 100UL) {
        g_fifoTestResult.failed_step = 205U;
        return false;
    }
    g_fifoTestResult.test2_pass = true;

    /* -------------------------------------------------------------------------
     * 3. Wrap around
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    /* Push 80 frames, then Pop 40 frames to shift tail index */
    for (i = 0U; i < 80U; i++) {
        txFrame.cmd = i;
        txFrame.data = i * 2U;
        (void)SPI_FIFO_Push(fifo, &txFrame);
    }
    for (i = 0U; i < 40U; i++) {
        (void)SPI_FIFO_Pop(fifo, &rxFrame);
    }

    /* Push another 80 frames. Since size is 128, this will wrap head back to index 0. */
    for (i = 80U; i < 160U; i++) {
        txFrame.cmd = i;
        txFrame.data = i * 2U;
        if (!SPI_FIFO_Push(fifo, &txFrame)) {
            g_fifoTestResult.failed_step = 301U;
            return false;
        }
    }

    /* Check that current count is 120 (160 total pushed, 40 popped) */
    if (fifo->count != 120U) {
        g_fifoTestResult.failed_step = 302U;
        return false;
    }

    /* Pop all 120 frames and verify correctness */
    for (i = 40U; i < 160U; i++) {
        if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
            g_fifoTestResult.failed_step = 303U;
            return false;
        }
        if (rxFrame.cmd != i || rxFrame.data != (i * 2U)) {
            g_fifoTestResult.failed_step = 304U;
            return false;
        }
    }

    if (fifo->count != 0U) {
        g_fifoTestResult.failed_step = 305U;
        return false;
    }
    g_fifoTestResult.test3_pass = true;

    /* -------------------------------------------------------------------------
     * 4. Overflow detection, no overwrite
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    /* Fill the FIFO completely */
    for (i = 0U; i < SPI_FIFO_SIZE; i++) {
        txFrame.cmd = 0x4000U + i;
        txFrame.data = 0xC000U + i;
        if (!SPI_FIFO_Push(fifo, &txFrame)) {
            g_fifoTestResult.failed_step = 401U;
            return false;
        }
    }

    /* Attempt to push 1 more frame (should fail and trigger overflow) */
    txFrame.cmd = 0xFFFFU;
    txFrame.data = 0xFFFFU;
    if (SPI_FIFO_Push(fifo, &txFrame)) {
        g_fifoTestResult.failed_step = 402U;
        return false;
    }

    if (fifo->overflowCount != 1UL || fifo->count != SPI_FIFO_SIZE || fifo->pushCount != (uint32_t)SPI_FIFO_SIZE) {
        g_fifoTestResult.failed_step = 403U;
        return false;
    }

    /* Verify no overwrite occurred by popping all elements and checking values */
    for (i = 0U; i < SPI_FIFO_SIZE; i++) {
        if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
            g_fifoTestResult.failed_step = 404U;
            return false;
        }
        if (rxFrame.cmd != (0x4000U + i) || rxFrame.data != (0xC000U + i)) {
            g_fifoTestResult.failed_step = 405U;
            return false;
        }
    }

    if (fifo->count != 0U) {
        g_fifoTestResult.failed_step = 406U;
        return false;
    }
    g_fifoTestResult.test4_pass = true;

    /* -------------------------------------------------------------------------
     * 5. Underflow detection, no invalid pop
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    /* Attempt to pop from empty FIFO (should fail) */
    rxFrame.cmd = 0xAAAAU;
    rxFrame.data = 0x5555U;
    if (SPI_FIFO_Pop(fifo, &rxFrame)) {
        g_fifoTestResult.failed_step = 501U;
        return false;
    }

    if (fifo->underflowCount != 1UL || fifo->popCount != 0UL) {
        g_fifoTestResult.failed_step = 502U;
        return false;
    }

    /* Verify that rxFrame target content was not modified by the failed pop */
    if (rxFrame.cmd != 0xAAAAU || rxFrame.data != 0x5555U) {
        g_fifoTestResult.failed_step = 503U;
        return false;
    }
    g_fifoTestResult.test5_pass = true;

    /* -------------------------------------------------------------------------
     * 6. 1000-frame stress, no lost frame
     * ------------------------------------------------------------------------- */
    SPI_FIFO_Init(fifo);
    /* In a loop, push 4 frames and pop 4 frames until we have successfully
     * pushed 1000 frames. Then pop the remaining. Check order and counters. */
    uint32_t expectedPushCount = 0UL;
    uint32_t expectedPopCount = 0UL;

    while (expectedPushCount < 1000UL) {
        /* Push 4 frames */
        for (i = 0U; i < 4U; i++) {
            if (expectedPushCount < 1000UL) {
                txFrame.cmd = (uint16_t)(expectedPushCount & 0xFFFFU);
                txFrame.data = (uint16_t)((expectedPushCount * 3U) & 0xFFFFU);
                if (!SPI_FIFO_Push(fifo, &txFrame)) {
                    g_fifoTestResult.failed_step = 601U;
                    return false;
                }
                expectedPushCount++;
            }
        }

        /* Pop 4 frames */
        for (i = 0U; i < 4U; i++) {
            if (fifo->count > 0U) {
                if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
                    g_fifoTestResult.failed_step = 602U;
                    return false;
                }
                if (rxFrame.cmd != (uint16_t)(expectedPopCount & 0xFFFFU) ||
                    rxFrame.data != (uint16_t)((expectedPopCount * 3U) & 0xFFFFU)) {
                    g_fifoTestResult.failed_step = 603U;
                    return false;
                }
                expectedPopCount++;
            }
        }
    }

    /* Pop all remaining frames in FIFO */
    while (fifo->count > 0U) {
        if (!SPI_FIFO_Pop(fifo, &rxFrame)) {
            g_fifoTestResult.failed_step = 604U;
            return false;
        }
        if (rxFrame.cmd != (uint16_t)(expectedPopCount & 0xFFFFU) ||
            rxFrame.data != (uint16_t)((expectedPopCount * 3U) & 0xFFFFU)) {
            g_fifoTestResult.failed_step = 605U;
            return false;
        }
        expectedPopCount++;
    }

    if (fifo->count != 0U || fifo->pushCount != 1000UL || fifo->popCount != 1000UL ||
        fifo->overflowCount != 0UL || fifo->underflowCount != 0UL) {
        g_fifoTestResult.failed_step = 606U;
        return false;
    }
    g_fifoTestResult.test6_pass = true;

    return true;
}
