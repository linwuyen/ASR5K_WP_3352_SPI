/*
 * SPI_master.h
 *
 * Created on: May 19, 2026
 * Author: roger_lin
 * Description: SPI Master header contract. Strictly formatted to ASCII and ASR5K rules.
 */

#ifndef SPI_MASTER_H_
#define SPI_MASTER_H_

#include "cmd_id.h"
#include "Diag/comm_diag.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// STRICT_STDINT Compliance Types
// ============================================================================
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef float    f32;

// ============================================================================
// Enums and Defines
// ============================================================================
#define SPI_TX_QUEUE_SIZE 8U
#define SPI_STRESS_BUFFER_SIZE 3U
#define SPI_SEQ_BUFFER_SIZE 18U
#define SPI_SINE_TABLE_SIZE 4095U
#define SPI_WAVE_BLOCK_TRANSFER_FRAMES (SPI_SINE_TABLE_SIZE + 2U)
#define SPI_WAVE_DOWNLOAD_TRANSFER_FRAMES \
  (1U + WAVE_BURST_GUARD_FRAME_COUNT + WAVE_BURST_SAMPLE_COUNT)
#define SPI_BLOCK_TRANSFER_MAX_FRAMES SPI_WAVE_DOWNLOAD_TRANSFER_FRAMES
#define SPI_RAW_FRAME_STRESS_COUNT 1000U
#define SPI_PACKET_HEADER_MAGIC 0xA55AU
#define SPI_PACKET_PADDING_WORD 0x0000U

/* B01D post-burst gate snapshot bits. */
#define SPIA_DIAG_GATE_PENDING_CLEAR  0x0001U
#define SPIA_DIAG_GATE_OUT_OF_WAVE    0x0002U
#define SPIA_DIAG_GATE_DMA_ARMED      0x0004U
#define SPIA_DIAG_GATE_GUARD_ELAPSED  0x0008U
#define SPIA_DIAG_GATE_READY_MASK     0x0007U
#define SPIA_DIAG_GATE_SEND_MASK      0x000FU

typedef enum {
  SPI_CMD_IDLE = 0,
  SPI_CMD_WAIT_ACK,
  SPI_CMD_WAIT_SLAVE,
  SPI_CMD_WAIT_DATA,
  SPI_CMD_WAIT_IDLE,
  SPI_CMD_BLOCK_TX
} SPI_MASTER_STATE_e;

typedef enum {
  SPIA_MASTER_INIT = 0,
  SPIA_MASTER_RUN
} SPIA_MASTER_FSM_e;

typedef enum {
  SPI_OP_READ = 0,
  SPI_OP_WRITE
} SPI_OP_TYPE_e;

typedef enum {
  SPI_APP_STATE_IDLE = 0,
  SPI_APP_STATE_SINGLE_WRITE,
  SPI_APP_STATE_SINGLE_READ,
  SPI_APP_STATE_BLOCK_WRITE,
  SPI_APP_STATE_BLOCK_READ,
  SPI_APP_STATE_BLOCK_WR_RD,
  SPI_APP_STATE_POLL_SLAVE,
  SPI_APP_STATE_RAW_FRAME_STRESS,
  SPI_APP_STATE_PACKET_WRITE
} SPI_APP_STATE_e;

typedef enum {
  SPI_MASTER_TEST_CMD_NONE = 0,
  SPI_MASTER_TEST_CMD_WRITE,
  SPI_MASTER_TEST_CMD_READ,
  SPI_MASTER_TEST_CMD_SEQ_WRITE_16,
  SPI_MASTER_TEST_CMD_WAVE_4095,
  SPI_MASTER_TEST_CMD_SINE_4095,
  SPI_MASTER_TEST_CMD_REG_FRAME_1000,
  SPI_MASTER_TEST_CMD_PACKET_WRITE,
  SPI_MASTER_TEST_CMD_WAVE_DOWNLOAD
} SPI_MASTER_TEST_CMD_e;

typedef enum {
  SPI_MASTER_TEST_IDLE = 0,
  SPI_MASTER_TEST_TRIGGER,
  SPI_MASTER_TEST_RUNNING,
  SPI_MASTER_TEST_DONE,
  SPI_MASTER_TEST_ERROR
} SPI_MASTER_TEST_STATE_e;

typedef enum {
  SPI_TEST_STATUS_IDLE = 0,
  SPI_TEST_STATUS_RUNNING,
  SPI_TEST_STATUS_PASSED,
  SPI_TEST_STATUS_FAILED
} SPI_TEST_STATUS_e;

typedef enum {
  SPIA_MODULE_STATE_INIT = 0,
  SPIA_MODULE_STATE_READY,
  SPIA_MODULE_STATE_ACTIVE,
  SPIA_MODULE_STATE_FAULT
} SPIA_MODULE_STATE_e;

// ============================================================================
// Data Structures
// ============================================================================
typedef union {
  u32 u32All;
  struct {
    u16 u16Data;    /* Low 16 bits (Word 2) */
    u16 u16Address; /* High 16 bits (Word 1) */
  } stPack;
} ST_SPI_MASTER_PACKET;

typedef void (*pfSpiRxCallback)(u16 u16RxAddr, u16 u16RxData);
typedef void (*pfSpiBlockCallback)(void);

typedef struct {
  ST_SPI_MASTER_PACKET stTxPacket;
  SPI_OP_TYPE_e eOp;
  pfSpiRxCallback pfCallback;
} ST_SPI_TRANSACTION;

typedef struct {
  const ST_SPI_MASTER_PACKET *pstTxBuf;
  ST_SPI_MASTER_PACKET *pstRxBuf;
  u16 u16Length;
  u16 u16TxIndex;
  u16 u16RxIndex;
  u16 bNullSent;
  u16 bFirstDiscarded;
  u16 bLoop;
  u16 u16WaveMode; /* 0=trapezoid (idx+1)*16, 1=sine table prepared on trigger */
  pfSpiBlockCallback pfCallback;
} ST_SPI_BLOCK_TRANSFER;

typedef struct {
  u32 u32SpiBaseAddr;

  ST_SPI_TRANSACTION stTxQueue[SPI_TX_QUEUE_SIZE];
  u16 u16QHead;
  u16 u16QTail;
  u16 u16QCount;

  SPI_MASTER_STATE_e eState;
  ST_SPI_TRANSACTION stActiveTx;

  u16 u16FinalRxData;
  u16 u16RxAddrChecksum;

  u32 u32WaitSlaveCnt;
  u32 u32TimeoutCnt;
  u32 u32IdleLimitTicks;

  ST_SPI_BLOCK_TRANSFER stBlockTx;
} ST_SPI_MASTER;

typedef struct {
  u16 u16TestAddr;
  u16 u16TestData;
  SPI_MASTER_TEST_CMD_e eTestCmd;
  SPI_MASTER_TEST_STATE_e eTestState;
  SPI_MASTER_TEST_CMD_e eLastTestCmd;
  u16 u16LastResult;

  u16 u16StressEnable;
  u32 u32StressPassCnt;
  u32 u32StressFailCnt;
  u16 u16StressStep;
  u16 u16ContinuousMode;
  u16 u16StressLength;

  SPI_APP_STATE_e eState;

  u16 u16PollState;
  u32 u32PollTimer;
  u32 u32PollTimeoutCnt;
  u16 bPollPending;

  u16 u16RawFrameTarget;
  u16 u16RawFrameIndex;
  u16 u16RawFrameAddr;
  u16 u16RawFrameLastData;
} ST_SPI_APP;

typedef enum {
  SPIA_HEALTH_OK = 0,
  SPIA_HEALTH_WARNING,
  SPIA_HEALTH_FAULT
} SPIA_HEALTH_e;

typedef enum {
  SPIA_FAULT_SOURCE_NONE = 0,
  SPIA_FAULT_SOURCE_DRIVER,
  SPIA_FAULT_SOURCE_PROTOCOL,
  SPIA_FAULT_SOURCE_SERVICE
} SPIA_FAULT_SOURCE_e;

typedef enum {
  SPIA_DRV_STATE_IDLE = 0,
  SPIA_DRV_STATE_TX_RX,
  SPIA_DRV_STATE_WAIT_ACK,
  SPIA_DRV_STATE_WAIT_SLAVE,
  SPIA_DRV_STATE_WAIT_DATA,
  SPIA_DRV_STATE_WAIT_IDLE,
  SPIA_DRV_STATE_BLOCK
} SPIA_DRIVER_STATE_e;

typedef enum {
  SPIA_DRV_FAULT_NONE = 0,
  SPIA_DRV_FAULT_ACK_TIMEOUT = 1,
  SPIA_DRV_FAULT_DATA_TIMEOUT = 2,
  SPIA_DRV_FAULT_DMA_ERROR = 3,
  SPIA_DRV_FAULT_FIFO_OVERFLOW = 4,
  SPIA_DRV_FAULT_BLOCK_TIMEOUT = 5
} SPIA_DRIVER_FAULT_e;

typedef enum {
  SPIA_PROT_STATE_IDLE = 0,
  SPIA_PROT_STATE_PARSING_HEADER,
  SPIA_PROT_STATE_PARSING_PAYLOAD,
  SPIA_PROT_STATE_VERIFYING
} SPIA_PROTOCOL_STATE_e;

typedef enum {
  SPIA_PROT_FAULT_NONE = 0,
  SPIA_PROT_FAULT_BAD_HEADER = 1,
  SPIA_PROT_FAULT_BAD_LENGTH = 2,
  SPIA_PROT_FAULT_CHECKSUM = 3,
  SPIA_PROT_FAULT_UNSUPPORTED_CMD = 4,
  SPIA_PROT_FAULT_PADDING = 5,
  SPIA_PROT_FAULT_REG_PARSER = 6
} SPIA_PROTOCOL_FAULT_e;

typedef enum {
  SPIA_SERV_STATE_IDLE = 0,
  SPIA_SERV_STATE_BUSY,
  SPIA_SERV_STATE_RECEIVING,
  SPIA_SERV_STATE_COMMITTING,
  SPIA_SERV_STATE_TEST_RUNNING
} SPIA_SERVICE_STATE_e;

typedef enum {
  SPIA_SERV_FAULT_NONE = 0,
  SPIA_SERV_FAULT_OUT_OF_SEQUENCE = 1,
  SPIA_SERV_FAULT_LENGTH_MISMATCH = 2,
  SPIA_SERV_FAULT_CHECKSUM_MISSING = 3,
  SPIA_SERV_FAULT_CHECKSUM_MISMATCH = 4,
  SPIA_SERV_FAULT_OVERFLOW = 5,
  SPIA_SERV_FAULT_FLASH_COMMIT = 6,
  SPIA_SERV_FAULT_POLL_TIMEOUT = 7,
  SPIA_SERV_FAULT_STRESS_TIMEOUT = 8,
  SPIA_SERV_FAULT_PACKET_TIMEOUT = 9
} SPIA_SERVICE_FAULT_e;

typedef struct {
  SPIA_DRIVER_STATE_e eState;
  SPIA_DRIVER_FAULT_e eFault;
  ST_COMM_DIAG stComm;
} ST_SPIA_DRIVER_DIAG;

typedef struct {
  SPIA_PROTOCOL_STATE_e eState;
  SPIA_PROTOCOL_FAULT_e eFault;
  ST_COMM_DIAG stComm;
} ST_SPIA_PROTOCOL_DIAG;

typedef struct {
  SPIA_SERVICE_STATE_e eState;
  SPIA_SERVICE_FAULT_e eFault;
  ST_COMM_DIAG stComm;
} ST_SPIA_SERVICE_DIAG;

typedef struct {
  SPIA_HEALTH_e eHealth;
  SPIA_FAULT_SOURCE_e eFaultSource;
  u16 u16FaultCode;

  ST_SPIA_DRIVER_DIAG stDriver;
  ST_SPIA_PROTOCOL_DIAG stProtocol;
  ST_SPIA_SERVICE_DIAG stService;
} ST_SPIA_MODULE_DIAG;

/*
 * Formal test interface for firmware API and CCS Expressions.
 * Set request fields, then set u16Start to 1. Firmware clears u16Start.
 * Normal test operation only needs this structure.
 */
typedef struct {
  SPI_MASTER_TEST_CMD_e eCommand;
  u16 u16Address;
  u16 u16Data;
  u16 u16Start;

  SPI_TEST_STATUS_e eStatus;
  u16 u16Result;
  u32 u32Detail;
} ST_SPI_TEST_INTERFACE;

/*
 * Single CCS Expressions entry point for all SPIA Master runtime state.
 * Driver, application, diagnostics, and test buffers remain logically separate.
 */
typedef struct {
  SPIA_MASTER_FSM_e fsm;
  volatile ST_SPI_TEST_INTERFACE stTest;
  ST_SPI_MASTER stDriver;
  ST_SPI_APP stApp;
  volatile ST_SPIA_MODULE_DIAG stDiag;

  ST_SPI_MASTER_PACKET *pastStressTxBuf;
  ST_SPI_MASTER_PACKET *pastStressRxBuf;
  ST_SPI_MASTER_PACKET *pastSeqTxBuf;
  ST_SPI_MASTER_PACKET *pastSeqRxBuf;
  u16 *pau16SineTable;
} ST_SPI_MASTER_CONTROL;

extern ST_SPI_MASTER_CONTROL spiA_master;
extern volatile u16 g_u16SpiMasterWaveRam[SPI_SINE_TABLE_SIZE];
extern volatile u32 g_u32DiagMasterBurstDoneTick;
extern volatile u32 g_u32DiagMasterWaitAckStartTick;
extern volatile u32 g_u32DiagMasterWaitAckFailTick;
extern volatile u16 g_u16DiagMasterLastTxCmd;
extern volatile u16 g_u16DiagMasterLastTxData;
extern volatile u16 g_u16DiagMasterGateSeen;

// ============================================================================
// Fixed SPIA Wrapper APIs
// ============================================================================

/**
 * @brief Initialize on first call, then run the global SPIA Master task
 */
void runSPIAmaster(void);

/**
 * @brief Start one formal SPI communication test
 * @return 1 when accepted, 0 when another test is active
 */
u16 startSPIAmasterTest(SPI_MASTER_TEST_CMD_e eCommand,
                        u16 u16Address,
                        u16 u16Data);

#define clearSPIAmasterFault() do {                                      \
  spiA_master.stDiag.eHealth = SPIA_HEALTH_OK;                           \
  spiA_master.stDiag.eFaultSource = SPIA_FAULT_SOURCE_NONE;              \
  spiA_master.stDiag.u16FaultCode = 0U;                                  \
  spiA_master.stDiag.stDriver.eState = SPIA_DRV_STATE_IDLE;              \
  spiA_master.stDiag.stDriver.eFault = SPIA_DRV_FAULT_NONE;              \
  spiA_master.stDiag.stProtocol.eState = SPIA_PROT_STATE_IDLE;           \
  spiA_master.stDiag.stProtocol.eFault = SPIA_PROT_FAULT_NONE;           \
  spiA_master.stDiag.stService.eState = SPIA_SERV_STATE_IDLE;            \
  spiA_master.stDiag.stService.eFault = SPIA_SERV_FAULT_NONE;            \
  CommDiag_ClearLatch((ST_COMM_DIAG *)&spiA_master.stDiag.stDriver.stComm); \
  CommDiag_ClearLatch((ST_COMM_DIAG *)&spiA_master.stDiag.stProtocol.stComm); \
  CommDiag_ClearLatch((ST_COMM_DIAG *)&spiA_master.stDiag.stService.stComm); \
} while (0)

/**
 * @brief Enqueue a read request on the global SPIA Master
 */
u16 readSPIAmaster(u16 u16Addr, pfSpiRxCallback pfCb);

/**
 * @brief Enqueue a write request on the global SPIA Master
 */
u16 writeSPIAmaster(u16 u16Addr, u16 u16Data, pfSpiRxCallback pfCb);

/**
 * @brief Start a block transfer on the global SPIA Master
 */
u16 writeBlockSPIAmaster(const ST_SPI_MASTER_PACKET *pstTxBuf,
                         ST_SPI_MASTER_PACKET *pstRxBuf,
                         u16 u16Length,
                         pfSpiBlockCallback pfCb);

/**
 * @brief Start a generated waveform transfer on the global SPIA Master
 */
u16 writeWaveBlockSPIAmaster(u16 u16WaveMode, pfSpiBlockCallback pfCb);

// ============================================================================
// Reusable Driver APIs (verbCamelCase)
// ============================================================================

/**
 * @brief Initialize SPI Master driver instance
 */
void initSpiMaster(ST_SPI_MASTER *pstInst, u32 u32SpiBaseAddr);

/**
 * @brief Enqueue an asynchronous read transaction request
 * @return 1 for success, 0 if queue is full
 */
u16 readSpiMaster(ST_SPI_MASTER *pstInst, u16 u16Addr, pfSpiRxCallback pfCb);

/**
 * @brief Enqueue an asynchronous write transaction request
 * @return 1 for success, 0 if queue is full
 */
u16 writeSpiMaster(ST_SPI_MASTER *pstInst, u16 u16Addr, u16 u16Data, pfSpiRxCallback pfCb);

/**
 * @brief Low-level non-blocking communication state machine update, called periodically
 */
void runSpiMasterCommunication(ST_SPI_MASTER *pstInst);

/**
 * @brief Application layer logic loop for stress test and manual commands
 */
void runSpiMasterApp(ST_SPI_MASTER *pstInst);

/**
 * @brief Unified interface task, combining communication and application logic
 */
void spiMasterTask(ST_SPI_MASTER *pstInst);

/**
 * @brief Enqueue continuous block write transfer (up to 4095/4096 packets)
 */
u16 writeBlockSpiMaster(ST_SPI_MASTER *pstInst,
                        const ST_SPI_MASTER_PACKET *pstTxBuf,
                        ST_SPI_MASTER_PACKET *pstRxBuf,
                        u16 u16Length,
                        pfSpiBlockCallback pfCb);

/**
 * @brief Start a generated 4095-point waveform plus Block End transfer
 * @param u16WaveMode 0 for ramp/trapezoid test data, 1 for sine table prepared on trigger
 */
u16 writeWaveBlockSpiMaster(ST_SPI_MASTER *pstInst,
                            u16 u16WaveMode,
                            pfSpiBlockCallback pfCb);

/**
 * @brief Reset the Master communication state after a timeout or protocol error
 */
void recoverSpiMaster(ST_SPI_MASTER *pstInst);

/**
 * @brief Calculate the protocol byte-sum checksum for one 16-bit data word
 */
u16 calcSpiMasterChecksum(u16 u16Data);

/**
 * @brief Initialize the Master application/test layer
 */
void initSpiMasterApp(void);

/**
 * @brief Get global Master instance handle
 */
ST_SPI_MASTER *getSpiMasterHandle(void);

/**
 * @brief Get global Master App instance handle
 */
ST_SPI_APP *getSpiAppHandle(void);

/**
 * @brief Get global Master Diag instance handle
 */
ST_SPIA_MODULE_DIAG *getSpiDiagHandle(void);

// ============================================================================
// Expanded Diagnostics & Self-Test APIs
// ============================================================================

/**
 * @brief Trigger loopback diagnostic self-test
 */
void triggerSpiMasterSelfTest(ST_SPI_MASTER *pstInst);

/**
 * @brief Get diagnostic self-test status/result
 * @return 0:Not run, 1:Running, 2:Passed, 3:Failed
 */
u16 getSpiMasterSelfTestResult(ST_SPI_MASTER *pstInst);

/**
 * @brief Reset diagnostic counters and metrics
 */
void resetSpiMasterMetrics(ST_SPI_MASTER *pstInst);

/**
 * @brief Check if SPIA Master and Slave are communicating healthily (Heartbeat check)
 * @return true if healthy, false if connection lost
 */
bool checkSpiMasterHealth(ST_SPI_MASTER *pstInst);

#endif /* SPI_MASTER_H_ */
