/*
 * spi_fifo.h
 *
 * M2 SPI FIFO buffer contract.
 */

#ifndef SPI_FIFO_H_
#define SPI_FIFO_H_

#include <stdint.h>
#include <stdbool.h>
#include "device.h"
#include "ctypedef.h"
#include "HwConfig.h"

#define SPI_FIFO_SIZE 128U

/* SPI Frame structure storing command and data */
typedef struct {
    uint16_t cmd;
    uint16_t data;
} SPI_FRAME_t;

/* SPI FIFO buffer structure with diagnostic counters */
typedef struct {
    SPI_FRAME_t buffer[SPI_FIFO_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;

    uint32_t pushCount;
    uint32_t popCount;
    uint32_t overflowCount;
    uint32_t underflowCount;
    uint16_t maxDepth;
} SPI_FIFO_t;

/* Public API Contract */
void SPI_FIFO_Init(SPI_FIFO_t *fifo);
bool SPI_FIFO_Push(SPI_FIFO_t *fifo, const SPI_FRAME_t *frame);
bool SPI_FIFO_Pop(SPI_FIFO_t *fifo, SPI_FRAME_t *frame);
bool FIFO_Test_Run(void);

#endif /* SPI_FIFO_H_ */
