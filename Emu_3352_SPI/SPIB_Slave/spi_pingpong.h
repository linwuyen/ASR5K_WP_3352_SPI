/*
 * spi_pingpong.h
 *
 * M2R RxFrame Ping/Pong Buffer contract.
 */
#ifndef SPI_PINGPONG_H_
#define SPI_PINGPONG_H_

#include <stdint.h>
#include <stdbool.h>
#include "device.h"
#include "ctypedef.h"
#include "HwConfig.h"

#define RX_BUFFER_SIZE 64U  /* Size of each buffer in legacy frames */

/* Legacy frame structure */
typedef struct {
    uint16_t cmd;   /* Register address / command ID */
    uint16_t data;  /* Register data value */
} RxFrame_t;

/* Buffer States */
typedef enum {
    BUFF_STATE_EMPTY = 0,
    BUFF_STATE_FILLING,
    BUFF_STATE_FULL,
    BUFF_STATE_PARSING
} RxBufferState_e;

/* Ping/Pong Management Structure */
typedef struct {
    RxFrame_t pingBuffer[RX_BUFFER_SIZE];
    RxFrame_t pongBuffer[RX_BUFFER_SIZE];
    
    volatile RxBufferState_e pingState;
    volatile RxBufferState_e pongState;
    
    uint16_t activeDmaBuffer;      /* 0 = Ping, 1 = Pong */
    uint16_t activeParserBuffer;   /* 0 = Ping, 1 = Pong */
    
    uint16_t dmaWriteIdx;          /* Index for DMA writes */
    uint16_t parserReadIdx;        /* Index for parser reads */
    
    /* Diagnostics and Counters */
    uint32_t pingFullCount;
    uint32_t pongFullCount;
    uint32_t overrunCount;
    uint32_t parseSuccessCount;
    uint32_t parseFailCount;
} RxFramePingPong_t;

/* Public API */
void RxFramePingPong_Init(void);
bool RxFramePingPong_DmaWrite(const RxFrame_t *frame);
void RxFramePingPong_Process(void);
bool RxFramePingPong_Test_Run(void);

#endif /* SPI_PINGPONG_H_ */
