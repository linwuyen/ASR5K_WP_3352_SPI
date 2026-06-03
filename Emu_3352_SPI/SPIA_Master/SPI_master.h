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
#define SPI_STRESS_BUFFER_SIZE 2U
#define SPI_SEQ_BUFFER_SIZE 17U
#define SPI_SINE_TABLE_SIZE 4095U

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
  SPI_APP_STATE_POLL_SLAVE
} SPI_APP_STATE_e;

typedef enum {
  SPI_TEST_CMD_IDLE = 0,
  SPI_TEST_CMD_TRIGGER,
  SPI_TEST_CMD_RUNNING
} SPI_TEST_CMD_STATE_e;

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
  u16 u16WaveMode; /* 0=trapezoid (idx+1)*16, 1=sine table */
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
  SPI_TEST_CMD_STATE_e u16CmdWrite;
  SPI_TEST_CMD_STATE_e u16CmdRead;
  SPI_TEST_CMD_STATE_e u16CmdSeqWriteTest;
  SPI_TEST_CMD_STATE_e u16CmdWave4095Test;
  SPI_TEST_CMD_STATE_e u16CmdSineWave4095Test;

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
} ST_SPI_APP;

typedef struct {
  u32 u32TotalTxCount;          /* Total transmitted transaction packets count */
  u32 u32TotalRxCount;          /* Total received transaction packets count */
  u16 u16LastErrType;           /* 0:None, 1:WAIT_ACK timeout, 2:WAIT_DATA timeout, 3:Callback/checksum mismatch */
  u16 u16LastErrStep;           /* Current stress state machine step when error happened */
  u16 u16LastErrRxAddr;         /* Mismatched/incorrect raw address received */
  u16 u16LastErrRxData;         /* Mismatched/incorrect raw data received */
  u16 u16LastErrExpAddr;        /* Expected address value */
  u16 u16LastErrExpData;        /* Expected data value */
  u16 u16ChecksumErrorCount;    /* Total checksum errors count */
  u16 u16ConsecutiveSuccess;    /* Current continuous success count */
  u16 u16MaxConsecutiveSuccess; /* Historical maximum consecutive success count */
  u16 u16SelfTestStatus;        /* Self-test status (0:Not run, 1:Running, 2:Passed, 3:Failed) */
  u16 u16SelfTestErrorCount;    /* Total errors occurred during loopback self-test */
} ST_SPI_DIAG;

/*
 * Single CCS Expressions entry point for all SPIA Master runtime state.
 * Driver, application, diagnostics, and test buffers remain logically separate.
 */
typedef struct {
  SPIA_MASTER_FSM_e fsm;
  ST_SPI_MASTER stDriver;
  ST_SPI_APP stApp;
  ST_SPI_DIAG stDiag;

  ST_SPI_MASTER_PACKET *pastStressTxBuf;
  ST_SPI_MASTER_PACKET *pastStressRxBuf;
  ST_SPI_MASTER_PACKET *pastSeqTxBuf;
  ST_SPI_MASTER_PACKET *pastSeqRxBuf;
  u16 *pau16SineTable;
} ST_SPI_MASTER_CONTROL;

extern ST_SPI_MASTER_CONTROL spiA_master;

// ============================================================================
// Fixed SPIA Wrapper APIs
// ============================================================================

/**
 * @brief Initialize on first call, then run the global SPIA Master task
 */
void runSPIAmaster(void);

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
 * @param u16WaveMode 0 for ramp/trapezoid test data, 1 for sine table
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
ST_SPI_DIAG *getSpiDiagHandle(void);

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
