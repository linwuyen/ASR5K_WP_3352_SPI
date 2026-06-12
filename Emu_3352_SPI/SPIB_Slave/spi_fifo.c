/*
 * spi_fifo.c
 *
 * M2 SPI FIFO buffer implementation.
 */

#include "spi_fifo.h"
#include <stddef.h>

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
