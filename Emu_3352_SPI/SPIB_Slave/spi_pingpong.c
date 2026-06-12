/*
 * spi_pingpong.c
 *
 * DMA Ping/Pong buffer implementation.
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
