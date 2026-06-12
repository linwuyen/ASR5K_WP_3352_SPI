/*
 * spi_pingpong.h
 *
 * DMA Ping/Pong buffer abstraction for SPI-B single-frame DMA transfers.
 *
 * Each DMA transfer moves exactly SPIB_PINGPONG_FRAME_WORDS words (one
 * register frame).  After each transfer the caller swaps buffers so the
 * next DMA targets the idle buffer while the just-filled buffer is parsed.
 *
 * Replaces the six raw globals previously scattered in spi_b_slave.c:
 *   gSpibRxRegFrame / gSpibRxAltFrame / gSpibRxM3ActiveBuf /
 *   gSpibRxM3PingFullCount / gSpibRxM3PongFullCount / gSpibRxM3OverrunCount
 */

#ifndef SPI_PINGPONG_H_
#define SPI_PINGPONG_H_

#include <stdint.h>
#include <stdbool.h>

#define SPIB_PINGPONG_FRAME_WORDS  2U

typedef struct {
    volatile uint16_t pingBuf[SPIB_PINGPONG_FRAME_WORDS];
    volatile uint16_t pongBuf[SPIB_PINGPONG_FRAME_WORDS];
    volatile uint16_t activeDmaBuf;     /* 0 = DMA targets Ping, 1 = Pong */
    volatile uint32_t pingDoneCount;
    volatile uint32_t pongDoneCount;
} SpibDmaPingPong_t;

/* Initialize all fields to zero; activeDmaBuf starts at Ping (0). */
void SpibPingPong_Init(SpibDmaPingPong_t *pp);

/* Returns the buffer the DMA engine should write its next transfer into. */
volatile uint16_t *SpibPingPong_GetDmaDst(SpibDmaPingPong_t *pp);

/*
 * Called after DMA done.
 * Returns a pointer to the frame that was just completed (for parsing),
 * and pre-switches activeDmaBuf to the alternate buffer so the next
 * DMA restart immediately targets the idle buffer.
 */
volatile uint16_t *SpibPingPong_SwapAndGetFrame(SpibDmaPingPong_t *pp);

#endif /* SPI_PINGPONG_H_ */
