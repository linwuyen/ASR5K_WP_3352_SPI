/*
 * spi_pingpong.c
 *
 * M2R RxFrame Ping/Pong Buffer implementation and self-contained validation tests.
 */
#include "spi_pingpong.h"
#include <stddef.h>

#pragma DATA_SECTION(s_rxPingPong, "spib_pingpong_state")
static RxFramePingPong_t s_rxPingPong;

typedef struct {
    bool test1_pass;
    bool test2_pass;
    bool test3_pass;
    bool test4_pass;
    bool test5_pass;
    uint16_t failed_step;
} PingPong_Test_Result_t;

#pragma DATA_SECTION(g_pingpongTestResult, "spib_pingpong_state")
volatile PingPong_Test_Result_t g_pingpongTestResult = {
    .test1_pass = false,
    .test2_pass = false,
    .test3_pass = false,
    .test4_pass = false,
    .test5_pass = false,
    .failed_step = 0U
};

/**
 * @brief Initialize the Ping/Pong buffer structure and status
 */
void RxFramePingPong_Init(void)
{
    uint16_t idx;
    
    s_rxPingPong.pingState = BUFF_STATE_FILLING; /* Starts active */
    s_rxPingPong.pongState = BUFF_STATE_EMPTY;
    
    s_rxPingPong.activeDmaBuffer = 0U;     /* 0 = Ping */
    s_rxPingPong.activeParserBuffer = 0U;  /* 0 = Ping */
    
    s_rxPingPong.dmaWriteIdx = 0U;
    s_rxPingPong.parserReadIdx = 0U;
    
    s_rxPingPong.pingFullCount = 0UL;
    s_rxPingPong.pongFullCount = 0UL;
    s_rxPingPong.overrunCount = 0UL;
    s_rxPingPong.parseSuccessCount = 0UL;
    s_rxPingPong.parseFailCount = 0UL;
    
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        s_rxPingPong.pingBuffer[idx].cmd = 0U;
        s_rxPingPong.pingBuffer[idx].data = 0U;
        s_rxPingPong.pongBuffer[idx].cmd = 0U;
        s_rxPingPong.pongBuffer[idx].data = 0U;
    }
}

/**
 * @brief Simulates DMA CH3 writing a legacy register frame to the active buffer.
 * Handles software model of DMA destination switching and overrun detection.
 */
bool RxFramePingPong_DmaWrite(const RxFrame_t *frame)
{
    if (frame == NULL)
    {
        return false;
    }

    if (s_rxPingPong.activeDmaBuffer == 0U)
    {
        /* Active buffer is Ping */
        if (s_rxPingPong.pingState != BUFF_STATE_FILLING)
        {
            return false;
        }

        s_rxPingPong.pingBuffer[s_rxPingPong.dmaWriteIdx] = *frame;
        s_rxPingPong.dmaWriteIdx++;

        if (s_rxPingPong.dmaWriteIdx >= RX_BUFFER_SIZE)
        {
            s_rxPingPong.pingState = BUFF_STATE_FULL;
            s_rxPingPong.pingFullCount++;

            /* Try to switch to Pong */
            if (s_rxPingPong.pongState == BUFF_STATE_EMPTY)
            {
                s_rxPingPong.activeDmaBuffer = 1U;
                s_rxPingPong.pongState = BUFF_STATE_FILLING;
                s_rxPingPong.dmaWriteIdx = 0U;
            }
            else
            {
                /* Overrun occurred: alternate buffer is not empty */
                s_rxPingPong.overrunCount++;
                return false;
            }
        }
    }
    else
    {
        /* Active buffer is Pong */
        if (s_rxPingPong.pongState != BUFF_STATE_FILLING)
        {
            return false;
        }

        s_rxPingPong.pongBuffer[s_rxPingPong.dmaWriteIdx] = *frame;
        s_rxPingPong.dmaWriteIdx++;

        if (s_rxPingPong.dmaWriteIdx >= RX_BUFFER_SIZE)
        {
            s_rxPingPong.pongState = BUFF_STATE_FULL;
            s_rxPingPong.pongFullCount++;

            /* Try to switch to Ping */
            if (s_rxPingPong.pingState == BUFF_STATE_EMPTY)
            {
                s_rxPingPong.activeDmaBuffer = 0U;
                s_rxPingPong.pingState = BUFF_STATE_FILLING;
                s_rxPingPong.dmaWriteIdx = 0U;
            }
            else
            {
                /* Overrun occurred: alternate buffer is not empty */
                s_rxPingPong.overrunCount++;
                return false;
            }
        }
    }

    return true;
}

/**
 * @brief Simulates background parser processing filled buffers.
 */
void RxFramePingPong_Process(void)
{
    uint16_t idx;

    /* Background parser checks for FULL buffers in FIFO order */
    if (s_rxPingPong.pingState == BUFF_STATE_FULL)
    {
        s_rxPingPong.pingState = BUFF_STATE_PARSING;
        
        /* Allow frame inspection/processing */
        for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
        {
            (void)s_rxPingPong.pingBuffer[idx].cmd;
            (void)s_rxPingPong.pingBuffer[idx].data;
        }
        
        s_rxPingPong.pingState = BUFF_STATE_EMPTY;
        s_rxPingPong.parseSuccessCount++;
    }

    if (s_rxPingPong.pongState == BUFF_STATE_FULL)
    {
        s_rxPingPong.pongState = BUFF_STATE_PARSING;
        
        /* Allow frame inspection/processing */
        for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
        {
            (void)s_rxPingPong.pongBuffer[idx].cmd;
            (void)s_rxPingPong.pongBuffer[idx].data;
        }
        
        s_rxPingPong.pongState = BUFF_STATE_EMPTY;
        s_rxPingPong.parseSuccessCount++;
    }
}

/**
 * @brief Self-contained verification suite for the Ping/Pong contract
 */
bool RxFramePingPong_Test_Run(void)
{
    RxFrame_t tempFrame;
    uint16_t idx;
    
    g_pingpongTestResult.test1_pass = false;
    g_pingpongTestResult.test2_pass = false;
    g_pingpongTestResult.test3_pass = false;
    g_pingpongTestResult.test4_pass = false;
    g_pingpongTestResult.test5_pass = false;
    g_pingpongTestResult.failed_step = 0U;
    
    /* -------------------------------------------------------------------------
     * Step 1: Initial State Verification
     * ------------------------------------------------------------------------- */
    RxFramePingPong_Init();
    
    if (s_rxPingPong.pingState != BUFF_STATE_FILLING ||
        s_rxPingPong.pongState != BUFF_STATE_EMPTY ||
        s_rxPingPong.activeDmaBuffer != 0U ||
        s_rxPingPong.dmaWriteIdx != 0U)
    {
        g_pingpongTestResult.failed_step = 101U;
        return false;
    }
    g_pingpongTestResult.test1_pass = true;
    
    /* -------------------------------------------------------------------------
     * Step 2: Buffer Switch & Fill (Ping Buffer Fill)
     * ------------------------------------------------------------------------- */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        tempFrame.cmd = 0x1000U + idx;
        tempFrame.data = 0xA000U + idx;
        if (!RxFramePingPong_DmaWrite(&tempFrame))
        {
            g_pingpongTestResult.failed_step = 201U;
            return false;
        }
    }
    
    /* Ping should be FULL, and DMA active switched to Pong */
    if (s_rxPingPong.pingState != BUFF_STATE_FULL ||
        s_rxPingPong.pongState != BUFF_STATE_FILLING ||
        s_rxPingPong.activeDmaBuffer != 1U ||
        s_rxPingPong.dmaWriteIdx != 0U ||
        s_rxPingPong.pingFullCount != 1UL)
    {
        g_pingpongTestResult.failed_step = 202U;
        return false;
    }
    
    /* Verify data consistency directly in Ping buffer before processing/release */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        if (s_rxPingPong.pingBuffer[idx].cmd != (0x1000U + idx) ||
            s_rxPingPong.pingBuffer[idx].data != (0xA000U + idx))
        {
            g_pingpongTestResult.failed_step = 203U;
            return false;
        }
    }
    g_pingpongTestResult.test2_pass = true;
    
    /* -------------------------------------------------------------------------
     * Step 3: Concurrent Parse & Fill (Simulate Parsing Ping while Filling Pong)
     * ------------------------------------------------------------------------- */
    /* Background parser takes Ping buffer */
    RxFramePingPong_Process();
    
    /* Ping state should return to EMPTY, parser counts incremented */
    if (s_rxPingPong.pingState != BUFF_STATE_EMPTY ||
        s_rxPingPong.parseSuccessCount != 1UL)
    {
        g_pingpongTestResult.failed_step = 301U;
        return false;
    }
    
    /* Fill Pong buffer completely */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        tempFrame.cmd = 0x2000U + idx;
        tempFrame.data = 0xB000U + idx;
        if (!RxFramePingPong_DmaWrite(&tempFrame))
        {
            g_pingpongTestResult.failed_step = 302U;
            return false;
        }
    }
    
    /* Pong should be FULL, and active switched back to Ping (since Ping is EMPTY) */
    if (s_rxPingPong.pongState != BUFF_STATE_FULL ||
        s_rxPingPong.pingState != BUFF_STATE_FILLING ||
        s_rxPingPong.activeDmaBuffer != 0U ||
        s_rxPingPong.dmaWriteIdx != 0U ||
        s_rxPingPong.pongFullCount != 1UL)
    {
        g_pingpongTestResult.failed_step = 303U;
        return false;
    }
    
    /* Verify data consistency directly in Pong buffer before processing/release */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        if (s_rxPingPong.pongBuffer[idx].cmd != (0x2000U + idx) ||
            s_rxPingPong.pongBuffer[idx].data != (0xB000U + idx))
        {
            g_pingpongTestResult.failed_step = 304U;
            return false;
        }
    }
    g_pingpongTestResult.test3_pass = true;
    
    /* -------------------------------------------------------------------------
     * Step 4: Overrun Detection (Alternate Buffer NOT Empty)
     * ------------------------------------------------------------------------- */
    /* Currently Ping is FILLING, Pong is FULL (unprocessed).
     * Try to fill Ping without calling the background parser on Pong. */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        tempFrame.cmd = 0x3000U + idx;
        tempFrame.data = 0xC000U + idx;
        
        if (idx < (RX_BUFFER_SIZE - 1U))
        {
            /* Normal writes should succeed */
            if (!RxFramePingPong_DmaWrite(&tempFrame))
            {
                g_pingpongTestResult.failed_step = 401U;
                return false;
            }
        }
        else
        {
            /* The last write triggers a switch to Pong, which is FULL. Expect OVERRUN! */
            if (RxFramePingPong_DmaWrite(&tempFrame))
            {
                g_pingpongTestResult.failed_step = 402U;
                return false;
            }
        }
    }
    
    /* Overrun count should be incremented, states remain FULL/FULL */
    if (s_rxPingPong.overrunCount != 1UL ||
        s_rxPingPong.pingState != BUFF_STATE_FULL ||
        s_rxPingPong.pongState != BUFF_STATE_FULL)
    {
        g_pingpongTestResult.failed_step = 403U;
        return false;
    }
    g_pingpongTestResult.test4_pass = true;
    
    /* -------------------------------------------------------------------------
     * Step 5: Data Consistency & Recovery
     * ------------------------------------------------------------------------- */
    /* Verify both buffers' contents directly before calling Process() */
    for (idx = 0U; idx < RX_BUFFER_SIZE; idx++)
    {
        if (s_rxPingPong.pingBuffer[idx].cmd != (0x3000U + idx) ||
            s_rxPingPong.pingBuffer[idx].data != (0xC000U + idx))
        {
            g_pingpongTestResult.failed_step = 501U;
            return false;
        }
        if (s_rxPingPong.pongBuffer[idx].cmd != (0x2000U + idx) ||
            s_rxPingPong.pongBuffer[idx].data != (0xB000U + idx))
        {
            g_pingpongTestResult.failed_step = 502U;
            return false;
        }
    }
    
    /* Process both FULL buffers to clear the overrun condition */
    RxFramePingPong_Process(); /* Processes Ping and Pong */
    
    if (s_rxPingPong.pingState != BUFF_STATE_EMPTY ||
        s_rxPingPong.pongState != BUFF_STATE_EMPTY ||
        s_rxPingPong.parseSuccessCount != 3UL)
    {
        g_pingpongTestResult.failed_step = 503U;
        return false;
    }
    
    g_pingpongTestResult.test5_pass = true;
    return true;
}
