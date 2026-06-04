/*
 * spi_slave.h
 *
 *  Created on: Apr 17, 2026
 *      Author: Antigravity
 */

#ifndef SPI_SLAVE_H_
#define SPI_SLAVE_H_

#include <stdbool.h>
#include "cmd_parser.h"

typedef enum {
    _INIT_SPI_AS_SLAVE =  0x00000000,
    _POP_RXD_FROM_SPI  = (0x00010000<<0),
    _WAIT_FOR_SPI_TIMEOUT,

    _MASK_ERROR_FOR_SPI =  0x80000000
}FSM_SPI_SLAVE;

typedef enum {
    _NO_STAT_OF_SSS =  0x00000000,
    _INIT_SSS_READY = (0x00000001<<0),
    _SSS_GET_ERROR  = (0x80000000)
}SSS_STAT;

// Base address for the SPI module used for system communication
#ifndef SPIB_SYSTEM_BASE
#define SPIB_SYSTEM_BASE       SPIB_BASE
#endif

#define SIZE_OF_SSS_BUFFER     64
#define SIZE_OF_SPI_BLOCK_RAM  4095U
#define SPIB_RX_REG_WORDS      2U

#define SPIB_RX_ERR_DMA_DONE_TIMEOUT 0x0001U
#define SPIB_RX_ERR_RX_FIFO_OVERFLOW 0x0002U
#define SPIB_RX_ERR_DMA_ERROR        0x0004U
#define SPIB_RX_ERR_FRAME_PARSE_FAIL 0x0008U

typedef struct
{
    uint16_t cmd;
    uint16_t data;
} SpibRegFrame;

typedef enum {
    FLASH_COMMIT_IDLE = 0,
    FLASH_COMMIT_BUSY,
    FLASH_COMMIT_DONE,
    FLASH_COMMIT_ERROR
} FLASH_COMMIT_STATE_e;

typedef volatile struct {
    FSM_SPI_SLAVE fsm;
    SSS_STAT stat;

    U32_PACK u32RxD[SIZE_OF_SSS_BUFFER];
    uint16_t u16Rpush;
    uint16_t u16Rpop;
    uint16_t u16Rcnt;
    uint16_t u16Reserved;

    uint32_t u32TimeMark;
    uint32_t u32TimeStamp;
    uint32_t u32Timeout;

    /* Diagnostic fields */
    uint32_t u32ResetCount;
    uint32_t u32MaxRcnt;
    uint16_t u16LastRcntBeforeReset;

    /* Fast/block direct path diagnostics */
    uint32_t u32FastPathCount;
    uint32_t u32FallbackPathCount;
    uint32_t u32BlockRxCount;
    uint32_t u32BlockOverflowCount;
    uint16_t u16BlockReady;
    uint16_t u16BlockWriteIndex;
    uint16_t u16BlockExpectedLen;
    uint16_t u16BlockChecksum;
    uint16_t u16BlockStatus;

    /* Background flash commit variables */
    uint16_t u16FlashCommitPending;
    uint16_t u16BlockProgress;
    FLASH_COMMIT_STATE_e eFlashState;
}ST_SPI_SLAVE;

typedef ST_SPI_SLAVE * HAL_SPI_SLAVE;

extern ST_SPI_SLAVE spiB_slave;
extern volatile uint16_t g_u16SpiBlockRam[SIZE_OF_SPI_BLOCK_RAM];
extern volatile uint16_t gSpibRxRegFrame[SPIB_RX_REG_WORDS];
extern volatile bool gSpibRxRegFrameReady;
extern volatile uint32_t gSpibRxDmaDoneCount;
extern volatile uint32_t gSpibRxParseOkCount;
extern volatile uint32_t gSpibRxParseFailCount;
extern volatile uint32_t gSpibRxDmaRestartCount;
extern volatile uint16_t gSpibRxErrorFlags;
extern volatile uint16_t OUTPUT_ON;
extern volatile uint32_t g_u32DebugLastTx;
extern volatile uint32_t g_u32DebugLastValidResponse;
extern bool SPIB_RxDmaIsDone(void);
extern void SPIB_RxDmaClearDone(void);
extern void SPIB_RxDmaRestart(void);
extern void SPIB_RxDmaResetDebugCounters(void);
extern bool SPIB_ParseRegFrame(uint16_t u16Cmd, uint16_t u16Data);
extern void runSPIBslave(void);

#endif /* SPI_SLAVE_H_ */
