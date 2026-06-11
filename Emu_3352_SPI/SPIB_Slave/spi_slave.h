/*
 * spi_slave.h
 *
 * Created on: Apr 17, 2026
 * Author: Antigravity
 */

#ifndef SPI_SLAVE_H_
#define SPI_SLAVE_H_

#include <stdbool.h>
#include "cmd_parser.h"
#include "Diag/comm_diag.h"
#include "wave_download.h"

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

#define SPIB_PACKET_HEADER_MAGIC     0xA55AU
#define SPIB_PACKET_PADDING_WORD     0x0000U
#define SPIB_PACKET_MAX_PAYLOAD_WORDS SIZE_OF_SPI_BLOCK_RAM

#define SPIB_RX_ERR_DMA_DONE_TIMEOUT 0x0001U
#define SPIB_RX_ERR_RX_FIFO_OVERFLOW 0x0002U
#define SPIB_RX_ERR_DMA_ERROR        0x0004U
#define SPIB_RX_ERR_FRAME_PARSE_FAIL 0x0008U

#define SPI_BLOCK_ERROR_NONE             0x0000U
#define SPI_BLOCK_ERROR_OUT_OF_SEQUENCE  0x0001U
#define SPI_BLOCK_ERROR_LENGTH_MISMATCH  0x0002U
#define SPI_BLOCK_ERROR_CHECKSUM_MISSING 0x0003U
#define SPI_BLOCK_ERROR_CHECKSUM_MISMATCH 0x0004U
#define SPI_BLOCK_ERROR_OVERFLOW         0x0005U
#define SPI_BLOCK_ERROR_FLASH_COMMIT     0x0006U

#define SPIB_PACKET_ERROR_NONE             0x0000U
#define SPIB_PACKET_ERROR_BAD_LENGTH       0x0001U
#define SPIB_PACKET_ERROR_CHECKSUM         0x0002U
#define SPIB_PACKET_ERROR_UNSUPPORTED_CMD  0x0003U
#define SPIB_PACKET_ERROR_BLOCK_BUSY       0x0004U
#define SPIB_PACKET_ERROR_PADDING          0x0005U

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

typedef enum {
    SPIB_PACKET_STATE_IDLE = 0,
    SPIB_PACKET_STATE_CMD,
    SPIB_PACKET_STATE_LENGTH,
    SPIB_PACKET_STATE_PAYLOAD,
    SPIB_PACKET_STATE_CHECKSUM
} SPIB_PACKET_STATE_e;

typedef enum {
    SPIB_HEALTH_OK = 0,
    SPIB_HEALTH_WARNING,
    SPIB_HEALTH_FAULT
} SPIB_HEALTH_e;

typedef enum {
    SPIB_FAULT_SOURCE_NONE = 0,
    SPIB_FAULT_SOURCE_DRIVER,
    SPIB_FAULT_SOURCE_PROTOCOL,
    SPIB_FAULT_SOURCE_SERVICE
} SPIB_FAULT_SOURCE_e;

typedef enum {
    SPIB_DRV_STATE_INIT = 0,
    SPIB_DRV_STATE_POP_RXD,
    SPIB_DRV_STATE_WAIT_TIMEOUT
} SPIB_DRIVER_STATE_e;

typedef enum {
    SPIB_DRV_FAULT_NONE = 0,
    SPIB_DRV_FAULT_DMA_TIMEOUT = 1,
    SPIB_DRV_FAULT_FIFO_OVERFLOW = 2,
    SPIB_DRV_FAULT_DMA_ERROR = 4,
    SPIB_DRV_FAULT_OVERRUN = 8          /* M3: DMA done with no free alternate buffer */
} SPIB_DRIVER_FAULT_e;

typedef enum {
    SPIB_PROT_STATE_IDLE = 0,
    SPIB_PROT_STATE_CMD,
    SPIB_PROT_STATE_LENGTH,
    SPIB_PROT_STATE_PAYLOAD,
    SPIB_PROT_STATE_CHECKSUM
} SPIB_PROTOCOL_STATE_e;

typedef enum {
    SPIB_PROT_FAULT_NONE = 0,
    SPIB_PROT_FAULT_BAD_LENGTH = 1,
    SPIB_PROT_FAULT_CHECKSUM = 2,
    SPIB_PROT_FAULT_UNSUPPORTED_CMD = 3,
    SPIB_PROT_FAULT_BLOCK_BUSY = 4,
    SPIB_PROT_FAULT_PADDING = 5,
    SPIB_PROT_FAULT_FRAME_PARSE_FAIL = 8
} SPIB_PROTOCOL_FAULT_e;

typedef enum {
    SPIB_SERV_STATE_IDLE = 0,
    SPIB_SERV_STATE_BUSY,
    SPIB_SERV_STATE_RECEIVING,
    SPIB_SERV_STATE_COMMITTING
} SPIB_SERVICE_STATE_e;

typedef enum {
    SPIB_SERV_FAULT_NONE = 0,
    SPIB_SERV_FAULT_OUT_OF_SEQUENCE = 1,
    SPIB_SERV_FAULT_LENGTH_MISMATCH = 2,
    SPIB_SERV_FAULT_CHECKSUM_MISSING = 3,
    SPIB_SERV_FAULT_CHECKSUM_MISMATCH = 4,
    SPIB_SERV_FAULT_OVERFLOW = 5,
    SPIB_SERV_FAULT_FLASH_COMMIT = 6
} SPIB_SERVICE_FAULT_e;

typedef struct {
    SPIB_DRIVER_STATE_e eState;
    SPIB_DRIVER_FAULT_e eFault;
    ST_COMM_DIAG stComm;
} ST_SPIB_DRIVER_DIAG;

typedef struct {
    SPIB_PROTOCOL_STATE_e eState;
    SPIB_PROTOCOL_FAULT_e eFault;
    ST_COMM_DIAG stComm;
} ST_SPIB_PROTOCOL_DIAG;

typedef struct {
    SPIB_SERVICE_STATE_e eState;
    SPIB_SERVICE_FAULT_e eFault;
    ST_COMM_DIAG stComm;
} ST_SPIB_SERVICE_DIAG;

typedef struct {
    SPIB_HEALTH_e eHealth;
    SPIB_FAULT_SOURCE_e eFaultSource;
    uint16_t u16FaultCode;

    ST_SPIB_DRIVER_DIAG stDriver;
    ST_SPIB_PROTOCOL_DIAG stProtocol;
    ST_SPIB_SERVICE_DIAG stService;
} ST_SPIB_MODULE_DIAG;

typedef volatile struct {
    FSM_SPI_SLAVE fsm;
    SSS_STAT stat;
    volatile ST_SPIB_MODULE_DIAG stDiag;

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
    uint16_t u16BlockExpectedChecksum;
    uint16_t u16BlockExpectedChecksumValid;
    uint16_t u16BlockExpectedIndex;
    uint16_t u16BlockErrorCode;
    uint16_t u16BlockStatus;

    /* Background flash commit variables */
    uint16_t u16FlashCommitPending;
    uint16_t u16BlockProgress;
    FLASH_COMMIT_STATE_e eFlashState;

    /* Protocol switch diagnostics: legacy 2-word frame and packet mode share handlers. */
    SPIB_PACKET_STATE_e ePacketState;
    uint16_t u16PacketCmd;
    uint16_t u16PacketLength;
    uint16_t u16PacketIndex;
    uint16_t u16PacketChecksum;
    uint16_t u16PacketLastChecksum;
    uint16_t u16PacketFirstPayload;
    uint16_t u16PacketErrorCode;
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
/* M3: Ping/Pong alternate buffer and counters */
extern volatile uint16_t gSpibRxAltFrame[SPIB_RX_REG_WORDS];
extern volatile uint16_t gSpibRxM3ActiveBuf;
extern volatile uint32_t gSpibRxM3PingFullCount;
extern volatile uint32_t gSpibRxM3PongFullCount;
extern volatile uint32_t gSpibRxM3OverrunCount;
extern volatile uint16_t OUTPUT_ON;
extern volatile uint32_t g_u32DebugLastTx;
extern volatile uint32_t g_u32DebugLastValidResponse;
extern bool SPIB_RxDmaIsDone(void);
extern void SPIB_RxDmaClearDone(void);
extern void SPIB_RxDmaRestart(void);
extern void SPIB_RxDmaResetDebugCounters(void);
extern void SPIB_RxDma_ConfigureRegFrame(uint16_t *pDst);
extern void SPIB_RxDma_Start(void);
extern void SPIB_RxDma_Stop(void);
extern void SPIB_RxDma_ClearFlags(void);
#define SPIB_ClearModuleFault() do {                                     \
    spiB_slave.stDiag.eHealth = SPIB_HEALTH_OK;                          \
    spiB_slave.stDiag.eFaultSource = SPIB_FAULT_SOURCE_NONE;             \
    spiB_slave.stDiag.u16FaultCode = 0U;                                 \
    spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_INIT;             \
    spiB_slave.stDiag.stDriver.eFault = SPIB_DRV_FAULT_NONE;             \
    spiB_slave.stDiag.stProtocol.eState = SPIB_PROT_STATE_IDLE;          \
    spiB_slave.stDiag.stProtocol.eFault = SPIB_PROT_FAULT_NONE;          \
    spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_IDLE;           \
    spiB_slave.stDiag.stService.eFault = SPIB_SERV_FAULT_NONE;           \
    gSpibRxErrorFlags = 0U;                                              \
    spiB_slave.u16PacketErrorCode = SPIB_PACKET_ERROR_NONE;              \
    spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_NONE;                 \
    spiB_slave.stat &= ~_SSS_GET_ERROR;                                  \
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stDriver.stComm);   \
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stProtocol.stComm); \
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stService.stComm);  \
} while (0)
extern bool SPIB_ParseRegFrame(uint16_t u16Cmd, uint16_t u16Data);
extern void runSPIBslave(void);

#endif /* SPI_SLAVE_H_ */
