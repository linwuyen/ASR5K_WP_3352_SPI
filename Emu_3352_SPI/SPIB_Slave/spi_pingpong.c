/*
 * spi_pingpong.c
 *
 * DMA Ping/Pong buffer implementation and self-contained validation tests.
 */

#include "spi_pingpong.h"

void SpibPingPong_Init(SpibDmaPingPong_t *pp)
{
    pp->pingBuf[0]    = 0U;
    pp->pingBuf[1]    = 0U;
    pp->pongBuf[0]    = 0U;
    pp->pongBuf[1]    = 0U;
    pp->activeDmaBuf  = 0U;
    pp->pingDoneCount = 0UL;
    pp->pongDoneCount = 0UL;
    pp->overrunCount  = 0UL;
}

volatile uint16_t *SpibPingPong_GetDmaDst(SpibDmaPingPong_t *pp)
{
    return (pp->activeDmaBuf == 0U) ? pp->pingBuf : pp->pongBuf;
}

volatile uint16_t *SpibPingPong_SwapAndGetFrame(SpibDmaPingPong_t *pp)
{
    volatile uint16_t *pFrame;

    if (pp->activeDmaBuf == 0U)
    {
        pFrame = pp->pingBuf;
        pp->pingDoneCount++;
        pp->activeDmaBuf = 1U;   /* next DMA targets Pong */
    }
    else
    {
        pFrame = pp->pongBuf;
        pp->pongDoneCount++;
        pp->activeDmaBuf = 0U;   /* next DMA targets Ping */
    }

    return pFrame;
}

bool SpibPingPong_Test_Run(void)
{
    SpibDmaPingPong_t pp;
    volatile uint16_t *pDst;
    volatile uint16_t *pFrame;

    SpibPingPong_Init(&pp);

    /* verify init state */
    if (pp.activeDmaBuf  != 0U)  { return false; }
    if (pp.pingDoneCount != 0UL) { return false; }
    if (pp.pongDoneCount != 0UL) { return false; }

    /* === cycle 1: DMA targets Ping === */
    pDst = SpibPingPong_GetDmaDst(&pp);
    if (pDst != pp.pingBuf) { return false; }

    pDst[0] = 0x1234U;
    pDst[1] = 0x5678U;

    pFrame = SpibPingPong_SwapAndGetFrame(&pp);
    if (pFrame        != pp.pingBuf) { return false; }
    if (pFrame[0]     != 0x1234U)    { return false; }
    if (pFrame[1]     != 0x5678U)    { return false; }
    if (pp.pingDoneCount != 1UL)     { return false; }
    if (pp.pongDoneCount != 0UL)     { return false; }
    if (pp.activeDmaBuf  != 1U)      { return false; }

    /* === cycle 2: DMA targets Pong === */
    pDst = SpibPingPong_GetDmaDst(&pp);
    if (pDst != pp.pongBuf) { return false; }

    pDst[0] = 0xABCDU;
    pDst[1] = 0xEF01U;

    pFrame = SpibPingPong_SwapAndGetFrame(&pp);
    if (pFrame        != pp.pongBuf) { return false; }
    if (pFrame[0]     != 0xABCDU)   { return false; }
    if (pFrame[1]     != 0xEF01U)   { return false; }
    if (pp.pingDoneCount != 1UL)     { return false; }
    if (pp.pongDoneCount != 1UL)     { return false; }
    if (pp.activeDmaBuf  != 0U)      { return false; }

    /* === cycle 3: back to Ping (full alternation) === */
    pDst = SpibPingPong_GetDmaDst(&pp);
    if (pDst != pp.pingBuf) { return false; }

    pDst[0] = 0xDEADU;
    pDst[1] = 0xBEEFU;

    pFrame = SpibPingPong_SwapAndGetFrame(&pp);
    if (pFrame        != pp.pingBuf) { return false; }
    if (pFrame[0]     != 0xDEADU)   { return false; }
    if (pFrame[1]     != 0xBEEFU)   { return false; }
    if (pp.pingDoneCount != 2UL)     { return false; }
    if (pp.activeDmaBuf  != 1U)      { return false; }

    return true;
}
