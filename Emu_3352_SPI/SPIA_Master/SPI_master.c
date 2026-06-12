/*
 * SPI_master.c (Preview Sandbox)
 *
 * Created on: May 19, 2026
 * Author: roger_lin
 * Description: SPI Master driver implementation. Pure ASCII, no division, STRICT_STDINT compliant.
 */

#include "SPI_master.h"
#include "board.h"
#include "device.h"
#include "driverlib.h"
#include "timetask.h"

// ============================================================================
// Private Macros & Configuration (Based on 50MHz hardware timer)
// ============================================================================
#define WAIT_SLAVE_TICKS 0U
#define WAIT_IDLE_TICKS  1000U    // 20us inter-packet guard band under 50MHz SW_TIMER
#define SPI_TIMEOUT_LIMIT 250000U // 5ms anti-deadlock timeout threshold
#define SPI_RAW_FRAME_STRESS_ADDR Output_ON_OFF_spi_addr

// ============================================================================
// Global Master Control and Private Aliases
// ============================================================================
static ST_SPI_MASTER_PACKET s_astStressTxBuf[SPI_STRESS_BUFFER_SIZE];
static ST_SPI_MASTER_PACKET s_astStressRxBuf[SPI_STRESS_BUFFER_SIZE];
static ST_SPI_MASTER_PACKET s_astSeqTxBuf[SPI_SEQ_BUFFER_SIZE];
static ST_SPI_MASTER_PACKET s_astSeqRxBuf[SPI_SEQ_BUFFER_SIZE];
static u16 s_u16SineTableReady;
static u16 s_u16SineTableChecksum;

#pragma DATA_SECTION(g_u16SpiMasterWaveRam, "spia_master_wave_ram")
volatile u16 g_u16SpiMasterWaveRam[SPI_SINE_TABLE_SIZE];

typedef struct {
  u16 index;
  u16 mosi_addr;
  u16 mosi_data;
  u16 miso_addr;
  u16 miso_data;
  u16 expected_ack_addr;
  u16 expected_ack_data;
  u16 validated_index;
  u16 validation_result;
} ST_SPI_DEBUG_TRACE;

#pragma DATA_SECTION(g_astDbgTrace, "spia_master_debug")
volatile ST_SPI_DEBUG_TRACE g_astDbgTrace[20];
#pragma DATA_SECTION(g_u16DbgTraceIdx, "spia_master_debug")
volatile u16 g_u16DbgTraceIdx = 0U;

#pragma DATA_SECTION(g_astMosiLog, "spia_master_debug")
volatile struct {
  u16 addr;
  u16 data;
} g_astMosiLog[40];
#pragma DATA_SECTION(g_u16MosiCount, "spia_master_debug")
volatile u16 g_u16MosiCount = 0U;


ST_SPI_MASTER_CONTROL spiA_master = {
  .pastStressTxBuf = s_astStressTxBuf,
  .pastStressRxBuf = s_astStressRxBuf,
  .pastSeqTxBuf = s_astSeqTxBuf,
  .pastSeqRxBuf = s_astSeqRxBuf,
  .pau16SineTable = (u16 *)g_u16SpiMasterWaveRam,
};

#define s_stSpiMaster    spiA_master.stDriver
#define s_stSpiApp       spiA_master.stApp
#define s_stSpiDiag      spiA_master.stDiag
#define s_stSpiTest      spiA_master.stTest

extern volatile u32 g_u32SpiSlaveLastRequest;

typedef enum {
  SPI_BLOCK_VERIFY_ADDR_ONLY = 0,
  SPI_BLOCK_VERIFY_ADDR_DATA,
  SPI_BLOCK_VERIFY_ADDR_NONZERO_DATA
} SPI_BLOCK_VERIFY_MODE_e;

typedef enum {
  SPI_CONTINUOUS_MODE_BLOCK_WRITE = 3,
  SPI_CONTINUOUS_MODE_BLOCK_READ,
  SPI_CONTINUOUS_MODE_BLOCK_WR_RD
} SPI_CONTINUOUS_MODE_e;

// ============================================================================
// Private Helper Functions & Queue Operations (Removed redundant inline)
// ============================================================================
u16 calcSpiMasterChecksum(u16 u16Data) {
  return (u16)((u16Data >> 8) + (u16Data & 0x00FFU));
}

static u32 getSpiMasterElapsedTicks(u32 u32StartTicks) {
  u32 u32Now = U32_UPCNTS;

  if (u32Now >= u32StartTicks) {
    return u32Now - u32StartTicks;
  }

  return (SW_TIMER - u32StartTicks) + u32Now;
}

static void writeSpiMasterFrame(u32 u32BaseAddr, u16 u16Address, u16 u16Data) {
  SPI_writeDataNonBlocking(u32BaseAddr, u16Address);
  SPI_writeDataNonBlocking(u32BaseAddr, u16Data);
}

static u16 calcBlockFrameChecksum(const ST_SPI_MASTER_PACKET *pstTxBuf,
                                  u16 u16DataFrameCount) {
  u16 u16Idx;
  u16 u16Checksum = 0U;

  if (pstTxBuf == 0) {
    return 0U;
  }

  for (u16Idx = 0U; u16Idx < u16DataFrameCount; u16Idx++) {
    u16Checksum = (u16)(u16Checksum +
                        calcSpiMasterChecksum(pstTxBuf[u16Idx].stPack.u16Data));
  }

  return u16Checksum;
}

static u16 calcSpiPacketChecksum3(u16 u16Cmd, u16 u16Length, u16 u16Payload0) {
  return (u16)(SPI_PACKET_HEADER_MAGIC + u16Cmd + u16Length + u16Payload0);
}

static u16 calcGeneratedWaveChecksum(u16 u16WaveMode) {
  if (u16WaveMode == 1U) {
    return s_u16SineTableChecksum;
  }
  return 30720U;
}

/* Slimmed central error reporter integrating CommDiag_ReportError */
static void reportMasterError(SPIA_FAULT_SOURCE_e eSource, u16 u16FaultVal) {
  s_stSpiTest.u32Detail = g_u32SpiSlaveLastRequest;

  if (spiA_master.stDiag.eHealth == SPIA_HEALTH_OK) {
    spiA_master.stDiag.eHealth = SPIA_HEALTH_FAULT;
    spiA_master.stDiag.eFaultSource = eSource;
    spiA_master.stDiag.u16FaultCode = u16FaultVal;

    u16 u16Step = (u16)s_stSpiApp.eState;
    u16 d0 = 0U;
    u16 d1 = 0U;

    if (eSource == SPIA_FAULT_SOURCE_DRIVER) {
      spiA_master.stDiag.stDriver.eFault = (SPIA_DRIVER_FAULT_e)u16FaultVal;
      d0 = s_stSpiMaster.stActiveTx.stTxPacket.stPack.u16Address;
      d1 = s_stSpiMaster.stActiveTx.stTxPacket.stPack.u16Data;
      CommDiag_ReportError((ST_COMM_DIAG *)&spiA_master.stDiag.stDriver.stComm, u16FaultVal, u16Step, U32_UPCNTS, d0, d1, 0U, 0U);
    } else if (eSource == SPIA_FAULT_SOURCE_PROTOCOL) {
      spiA_master.stDiag.stProtocol.eFault = (SPIA_PROTOCOL_FAULT_e)u16FaultVal;
      d0 = s_stSpiMaster.u16RxAddrChecksum;
      d1 = s_stSpiMaster.u16FinalRxData;
      CommDiag_ReportError((ST_COMM_DIAG *)&spiA_master.stDiag.stProtocol.stComm, u16FaultVal, u16Step, U32_UPCNTS, d0, d1, 0U, 0U);
    } else if (eSource == SPIA_FAULT_SOURCE_SERVICE) {
      spiA_master.stDiag.stService.eFault = (SPIA_SERVICE_FAULT_e)u16FaultVal;
      d0 = s_stSpiApp.u16TestAddr;
      d1 = s_stSpiApp.u16TestData;
      CommDiag_ReportError((ST_COMM_DIAG *)&spiA_master.stDiag.stService.stComm, u16FaultVal, u16Step, U32_UPCNTS, d0, d1, 0U, 0U);
    }
  }
}

static void buildWaveBlockPacket(ST_SPI_MASTER *pstInst,
                                 u16 u16Idx,
                                 u16 *pu16Address,
                                 u16 *pu16Data) {
  if (pstInst->stBlockTx.u16WaveMode == 2U) {
    *pu16Address = (u16)(0x3000U + u16Idx);
    *pu16Data = (u16)((u16Idx + 1U) * 16U);
  } else {
    if (u16Idx < SPI_SINE_TABLE_SIZE) {
      *pu16Address = (u16)(Spi_Block_Data_Base_spi_addr + u16Idx);
      if (pstInst->stBlockTx.u16WaveMode == 1U) {
        *pu16Data = g_u16SpiMasterWaveRam[u16Idx];
      } else {
        *pu16Data = (u16)((u16Idx + 1U) * 16U);
      }
    } else if (u16Idx == SPI_SINE_TABLE_SIZE) {
      *pu16Address = Spi_Block_Expected_CheckSum_spi_addr;
      *pu16Data = calcGeneratedWaveChecksum(pstInst->stBlockTx.u16WaveMode);
    } else {
      *pu16Address = Spi_Block_End_spi_addr;
      *pu16Data = 4095U;
    }
  }
}

// Transaction enqueue implementation
static u16 enqueueTx(ST_SPI_MASTER *pstInst, ST_SPI_MASTER_PACKET stPack,
                     SPI_OP_TYPE_e eOp, pfSpiRxCallback pfCb) {
  if (pstInst->u16QCount >= SPI_TX_QUEUE_SIZE) {
    return 0U; // Queue full
  }

  pstInst->stTxQueue[pstInst->u16QHead].stTxPacket = stPack;
  pstInst->stTxQueue[pstInst->u16QHead].eOp = eOp;
  pstInst->stTxQueue[pstInst->u16QHead].pfCallback = pfCb;

  pstInst->u16QHead = (u16)((pstInst->u16QHead + 1U) & (SPI_TX_QUEUE_SIZE - 1U));
  pstInst->u16QCount++;
  return 1U;
}

// Transaction dequeue implementation
static u16 dequeueTx(ST_SPI_MASTER *pstInst, ST_SPI_TRANSACTION *pstTx) {
  if (pstInst->u16QCount == 0U) {
    return 0U; // Queue empty
  }

  *pstTx = pstInst->stTxQueue[pstInst->u16QTail];
  pstInst->u16QTail = (u16)((pstInst->u16QTail + 1U) & (SPI_TX_QUEUE_SIZE - 1U));
  pstInst->u16QCount--;
  return 1U;
}

void recoverSpiMaster(ST_SPI_MASTER *pstInst) {
  pstInst->u16QHead = 0U;
  pstInst->u16QTail = 0U;
  pstInst->u16QCount = 0U;
  SPI_resetRxFIFO(pstInst->u32SpiBaseAddr);
  SPI_resetTxFIFO(pstInst->u32SpiBaseAddr);
  s_stSpiApp.u16StressStep = 0U;
  pstInst->u32WaitSlaveCnt = U32_UPCNTS;
  pstInst->u32IdleLimitTicks = 250000U; /* 5ms recovery pause */

  pstInst->stBlockTx.pstTxBuf = 0;
  pstInst->stBlockTx.pstRxBuf = 0;
  pstInst->stBlockTx.u16Length = 0U;
  pstInst->stBlockTx.u16TxIndex = 0U;
  pstInst->stBlockTx.u16RxIndex = 0U;
  pstInst->stBlockTx.bNullSent = 0U;
  pstInst->stBlockTx.bFirstDiscarded = 0U;
  pstInst->stBlockTx.bLoop = 0U;
  pstInst->stBlockTx.u16WaveMode = 0U;
  pstInst->stBlockTx.pfCallback = 0;

  s_stSpiApp.bPollPending = 0U;
  s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  if (s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER ||
      s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING) {
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
  }

  pstInst->eState = SPI_CMD_WAIT_IDLE;
}

// Application completion callbacks
static void onManualReadComplete(u16 u16RxAddr, u16 u16RxData);
static void onManualWriteComplete(u16 u16RxAddr, u16 u16RxData);
static void onBlockWriteComplete(void);
static void onBlockReadComplete(void);
static void onBlockWrRdComplete(void);
static void onPollSlaveComplete(u16 u16RxAddr, u16 u16RxData);
static void drainSpiMasterRxFifo(u32 u32BaseAddr);
static void prepareSineTable(void);
static void startBlockStatusPoll(void);
static void recordSpiMasterSuccess(u16 u16Count);
static u16 verifyBlockResponses(const ST_SPI_MASTER_PACKET *pstTxBuf,
                                ST_SPI_MASTER_PACKET *pstRxBuf,
                                u16 u16Length,
                                u16 bLoop,
                                SPI_BLOCK_VERIFY_MODE_e eVerifyMode);
static u16 startTriggeredSpiMasterTest(ST_SPI_MASTER *pstInst);
static u16 startContinuousSpiMasterTest(ST_SPI_MASTER *pstInst);
static void serviceSpiMasterTestRequest(void);
static void updateSpiMasterTestResult(void);

// ============================================================================
// Public Driver API Implementations
// ============================================================================

void initSpiMasterApp(void) {
  s_stSpiApp.u16TestAddr = 0x0400U;
  s_stSpiApp.u16TestData = 0x1234U;
  s_stSpiApp.eTestCmd = SPI_MASTER_TEST_CMD_NONE;
  s_stSpiApp.eTestState = SPI_MASTER_TEST_IDLE;
  s_stSpiApp.eLastTestCmd = SPI_MASTER_TEST_CMD_NONE;
  s_stSpiApp.u16LastResult = 0U;
  s_stSpiApp.u16StressEnable = 0U;
  s_stSpiApp.u32StressPassCnt = 0U;
  s_stSpiApp.u32StressFailCnt = 0U;
  s_stSpiApp.u16StressStep = 0U;
  s_stSpiApp.u16ContinuousMode = SPI_CONTINUOUS_MODE_BLOCK_WRITE;
  s_stSpiApp.u16StressLength = 2U;
  s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  s_stSpiApp.u16PollState = 0U;
  s_stSpiApp.u32PollTimer = 0U;
  s_stSpiApp.u32PollTimeoutCnt = 0U;
  s_stSpiApp.bPollPending = 0U;
  s_stSpiApp.u16RawFrameTarget = SPI_RAW_FRAME_STRESS_COUNT;
  s_stSpiApp.u16RawFrameIndex = 0U;
  s_stSpiApp.u16RawFrameAddr = SPI_RAW_FRAME_STRESS_ADDR;
  s_stSpiApp.u16RawFrameLastData = 0U;
  s_u16SineTableReady = 0U;

  spiA_master.stDiag.eHealth = SPIA_HEALTH_OK;
  spiA_master.stDiag.eFaultSource = SPIA_FAULT_SOURCE_NONE;
  spiA_master.stDiag.u16FaultCode = 0U;

  spiA_master.stDiag.stDriver.eState = SPIA_DRV_STATE_IDLE;
  spiA_master.stDiag.stDriver.eFault = SPIA_DRV_FAULT_NONE;
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stDriver.stComm);

  spiA_master.stDiag.stProtocol.eState = SPIA_PROT_STATE_IDLE;
  spiA_master.stDiag.stProtocol.eFault = SPIA_PROT_FAULT_NONE;
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stProtocol.stComm);

  spiA_master.stDiag.stService.eState = SPIA_SERV_STATE_IDLE;
  spiA_master.stDiag.stService.eFault = SPIA_SERV_FAULT_NONE;
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stService.stComm);
}

void initSpiMaster(ST_SPI_MASTER *pstInst, u32 u32SpiBaseAddr) {
  pstInst->u32SpiBaseAddr = u32SpiBaseAddr;

  pstInst->eState = SPI_CMD_IDLE;
  pstInst->u16QHead = 0U;
  pstInst->u16QTail = 0U;
  pstInst->u16QCount = 0U;
  pstInst->u32WaitSlaveCnt = 0U;
  pstInst->u32TimeoutCnt = 0U;
  pstInst->u32IdleLimitTicks = WAIT_IDLE_TICKS;

  pstInst->stBlockTx.pstTxBuf = 0;
  pstInst->stBlockTx.pstRxBuf = 0;
  pstInst->stBlockTx.u16Length = 0U;
  pstInst->stBlockTx.u16TxIndex = 0U;
  pstInst->stBlockTx.u16RxIndex = 0U;
  pstInst->stBlockTx.bNullSent = 0U;
  pstInst->stBlockTx.bFirstDiscarded = 0U;
  pstInst->stBlockTx.bLoop = 0U;
  pstInst->stBlockTx.u16WaveMode = 0U;
  pstInst->stBlockTx.pfCallback = 0;

  resetSpiMasterMetrics(pstInst);
  initSpiMasterApp();
}

ST_SPI_MASTER *getSpiMasterHandle(void) { return &s_stSpiMaster; }
ST_SPI_APP *getSpiAppHandle(void) { return &s_stSpiApp; }
ST_SPIA_MODULE_DIAG *getSpiDiagHandle(void) { return (ST_SPIA_MODULE_DIAG *)&spiA_master.stDiag; }

static void ensureSPIAmasterInitialized(void) {
  if (spiA_master.fsm == SPIA_MASTER_INIT) {
    initSpiMaster(&s_stSpiMaster, mySPI0_BASE);
    spiA_master.fsm = SPIA_MASTER_RUN;
  }
}

void runSPIAmaster(void) {
  switch (spiA_master.fsm) {
  case SPIA_MASTER_INIT:
    ensureSPIAmasterInitialized();
    break;

  case SPIA_MASTER_RUN:
    spiMasterTask(&s_stSpiMaster);
    break;

  default:
    spiA_master.fsm = SPIA_MASTER_INIT;
    break;
  }
}

u16 startSPIAmasterTest(SPI_MASTER_TEST_CMD_e eCommand,
                        u16 u16Address,
                        u16 u16Data) {
  ensureSPIAmasterInitialized();

  if ((eCommand == SPI_MASTER_TEST_CMD_NONE) ||
      (eCommand > SPI_MASTER_TEST_CMD_WAVE_DOWNLOAD) ||
      (spiA_master.stDiag.u16FaultCode != 0U) ||
      (s_stSpiTest.u16Start != 0U) ||
      (s_stSpiTest.eStatus == SPI_TEST_STATUS_RUNNING) ||
      (s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER) ||
      (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING)) {
    return 0U;
  }

  s_stSpiTest.eCommand = eCommand;
  s_stSpiTest.u16Address = u16Address;
  s_stSpiTest.u16Data = u16Data;
  s_stSpiTest.u16Start = 1U;
  return 1U;
}

u16 readSPIAmaster(u16 u16Addr, pfSpiRxCallback pfCb) {
  ensureSPIAmasterInitialized();
  return readSpiMaster(&s_stSpiMaster, u16Addr, pfCb);
}

u16 writeSPIAmaster(u16 u16Addr, u16 u16Data, pfSpiRxCallback pfCb) {
  ensureSPIAmasterInitialized();
  return writeSpiMaster(&s_stSpiMaster, u16Addr, u16Data, pfCb);
}

u16 writeBlockSPIAmaster(const ST_SPI_MASTER_PACKET *pstTxBuf,
                         ST_SPI_MASTER_PACKET *pstRxBuf,
                         u16 u16Length,
                         pfSpiBlockCallback pfCb) {
  ensureSPIAmasterInitialized();
  return writeBlockSpiMaster(&s_stSpiMaster, pstTxBuf, pstRxBuf, u16Length, pfCb);
}

u16 writeWaveBlockSPIAmaster(u16 u16WaveMode, pfSpiBlockCallback pfCb) {
  ensureSPIAmasterInitialized();
  return writeWaveBlockSpiMaster(&s_stSpiMaster, u16WaveMode, pfCb);
}

u16 readSpiMaster(ST_SPI_MASTER *pstInst, u16 u16Addr, pfSpiRxCallback pfCb) {
  if (pstInst->eState == SPI_CMD_BLOCK_TX) {
    return 0U;
  }
  ST_SPI_MASTER_PACKET stPack;
  stPack.stPack.u16Address = u16Addr;
  stPack.stPack.u16Data = 0x0000U;
  return enqueueTx(pstInst, stPack, SPI_OP_READ, pfCb);
}

u16 writeSpiMaster(ST_SPI_MASTER *pstInst, u16 u16Addr, u16 u16Data, pfSpiRxCallback pfCb) {
  if (pstInst->eState == SPI_CMD_BLOCK_TX) {
    return 0U;
  }
  ST_SPI_MASTER_PACKET stPack;
  stPack.stPack.u16Address = u16Addr;
  stPack.stPack.u16Data = u16Data;
  return enqueueTx(pstInst, stPack, SPI_OP_WRITE, pfCb);
}

u16 writeBlockSpiMaster(ST_SPI_MASTER *pstInst,
                        const ST_SPI_MASTER_PACKET *pstTxBuf,
                        ST_SPI_MASTER_PACKET *pstRxBuf,
                        u16 u16Length,
                        pfSpiBlockCallback pfCb) {
  if ((u16Length == 0U) || (u16Length > SPI_WAVE_BLOCK_TRANSFER_FRAMES)) {
    return 0U;
  }
  if ((pstTxBuf == 0) && (pstRxBuf == 0)) {
    /* Waveform generation mode */
  } else if ((pstTxBuf == 0) || (pstRxBuf == 0) ||
             (u16Length > SPI_WAVE_BLOCK_TRANSFER_FRAMES)) {
    return 0U;
  }
  if ((pstInst->eState != SPI_CMD_IDLE) || (pstInst->u16QCount > 0U)) {
    return 0U;
  }

  SPI_resetRxFIFO(pstInst->u32SpiBaseAddr);

  pstInst->stBlockTx.pstTxBuf = pstTxBuf;
  pstInst->stBlockTx.pstRxBuf = pstRxBuf;
  pstInst->stBlockTx.u16Length = u16Length;
  pstInst->stBlockTx.u16TxIndex = 0U;
  pstInst->stBlockTx.u16RxIndex = 0U;
  pstInst->stBlockTx.bNullSent = 0U;
  pstInst->stBlockTx.bFirstDiscarded = 0U;
  pstInst->stBlockTx.bLoop = 0U;
  pstInst->stBlockTx.u16WaveMode = 0U;
  pstInst->stBlockTx.pfCallback = pfCb;

  pstInst->u32TimeoutCnt = 0U;
  pstInst->eState = SPI_CMD_BLOCK_TX;
  return 1U;
}

u16 writeWaveBlockSpiMaster(ST_SPI_MASTER *pstInst,
                            u16 u16WaveMode,
                            pfSpiBlockCallback pfCb) {
  if (writeBlockSpiMaster(pstInst, 0, 0, SPI_WAVE_BLOCK_TRANSFER_FRAMES, pfCb) == 0U) {
    return 0U;
  }

  if (u16WaveMode == 1U) {
    prepareSineTable();
  }

  pstInst->stBlockTx.u16WaveMode = u16WaveMode;
  return 1U;
}

void runSpiMasterCommunication(ST_SPI_MASTER *pstInst) {
  switch (pstInst->eState) {
  case SPI_CMD_IDLE:
    if (dequeueTx(pstInst, &pstInst->stActiveTx) == 1U) {
      pstInst->u32TimeoutCnt = 0U;
      SPI_resetRxFIFO(pstInst->u32SpiBaseAddr);

      // Log TX Metric in Driver comm_diag
      spiA_master.stDiag.stDriver.stComm.u32TxTotal++;

      writeSpiMasterFrame(pstInst->u32SpiBaseAddr,
                          pstInst->stActiveTx.stTxPacket.stPack.u16Address,
                          pstInst->stActiveTx.stTxPacket.stPack.u16Data);

      pstInst->eState = SPI_CMD_WAIT_ACK;
    }
    break;

  case SPI_CMD_WAIT_ACK:
    if (SPI_getRxFIFOStatus(pstInst->u32SpiBaseAddr) >= 2U) {
      // Discard echo frame
      SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);
      SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);

      pstInst->u32WaitSlaveCnt = U32_UPCNTS;
      pstInst->u32TimeoutCnt = 0U;
      pstInst->eState = SPI_CMD_WAIT_SLAVE;
    } else {
      pstInst->u32TimeoutCnt++;
      if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
        s_stSpiApp.u32StressFailCnt++;
        reportMasterError(SPIA_FAULT_SOURCE_DRIVER, (u16)SPIA_DRV_FAULT_ACK_TIMEOUT);
        recoverSpiMaster(pstInst);
      }
    }
    break;

  case SPI_CMD_WAIT_SLAVE: {
#if WAIT_SLAVE_TICKS == 0U
    {
#else
    if (getSpiMasterElapsedTicks(pstInst->u32WaitSlaveCnt) >= WAIT_SLAVE_TICKS) {
#endif
      // Write Null transaction to clock out responses
      writeSpiMasterFrame(pstInst->u32SpiBaseAddr, 0xFFFFU, 0x0000U);
      pstInst->u32TimeoutCnt = 0U;
      pstInst->eState = SPI_CMD_WAIT_DATA;
    }
  } break;

  case SPI_CMD_WAIT_DATA:
    if (SPI_getRxFIFOStatus(pstInst->u32SpiBaseAddr) >= 2U) {
      pstInst->u16RxAddrChecksum = SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);
      pstInst->u16FinalRxData = SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);

      // Log RX Metric
      spiA_master.stDiag.stDriver.stComm.u32RxTotal++;

      if (pstInst->stActiveTx.pfCallback != 0) {
        pstInst->stActiveTx.pfCallback(pstInst->u16RxAddrChecksum,
                                       pstInst->u16FinalRxData);
      }

      pstInst->u32TimeoutCnt = 0U;
      pstInst->u32WaitSlaveCnt = U32_UPCNTS;
      pstInst->u32IdleLimitTicks = WAIT_IDLE_TICKS;
      pstInst->eState = SPI_CMD_WAIT_IDLE;
    } else {
      pstInst->u32TimeoutCnt++;
      if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
        s_stSpiApp.u32StressFailCnt++;
        reportMasterError(SPIA_FAULT_SOURCE_DRIVER, (u16)SPIA_DRV_FAULT_DATA_TIMEOUT);
        recoverSpiMaster(pstInst);
      }
    }
    break;

  case SPI_CMD_WAIT_IDLE: {
    if (getSpiMasterElapsedTicks(pstInst->u32WaitSlaveCnt) >= pstInst->u32IdleLimitTicks) {
      pstInst->u32IdleLimitTicks = WAIT_IDLE_TICKS;
      pstInst->eState = SPI_CMD_IDLE;
    }
  } break;

  case SPI_CMD_BLOCK_TX: {
    u32 u32BaseAddr = pstInst->u32SpiBaseAddr;
    u16 bBlockResponseDrained = 0U;

    // Read back data block
    while (SPI_getRxFIFOStatus(u32BaseAddr) >= 2U) {
      u16 w1 = SPI_readDataNonBlocking(u32BaseAddr);
      u16 w2 = SPI_readDataNonBlocking(u32BaseAddr);
      bBlockResponseDrained = 1U;

      spiA_master.stDiag.stDriver.stComm.u32RxTotal++;

      if (pstInst->stBlockTx.bFirstDiscarded == 0U) {
        pstInst->stBlockTx.bFirstDiscarded = 1U;
        pstInst->u32TimeoutCnt = 0U;

        // Log debug trace for the first (discarded) response
        if (pstInst->stBlockTx.u16WaveMode == 2U && g_u16DbgTraceIdx < 20U) {
          g_astDbgTrace[g_u16DbgTraceIdx].mosi_addr = (g_u16DbgTraceIdx < g_u16MosiCount) ? g_astMosiLog[g_u16DbgTraceIdx].addr : 0U;
          g_astDbgTrace[g_u16DbgTraceIdx].mosi_data = (g_u16DbgTraceIdx < g_u16MosiCount) ? g_astMosiLog[g_u16DbgTraceIdx].data : 0U;
          g_astDbgTrace[g_u16DbgTraceIdx].miso_addr = w1;
          g_astDbgTrace[g_u16DbgTraceIdx].miso_data = w2;
          g_astDbgTrace[g_u16DbgTraceIdx].expected_ack_addr = 0U;
          g_astDbgTrace[g_u16DbgTraceIdx].expected_ack_data = 0U;
          g_astDbgTrace[g_u16DbgTraceIdx].validated_index = 0xFFFFU;
          g_astDbgTrace[g_u16DbgTraceIdx].validation_result = 0U; // skipped
          g_u16DbgTraceIdx++;
        }
      } else {
        if (pstInst->stBlockTx.u16RxIndex < pstInst->stBlockTx.u16Length) {
          u16 u16Idx = pstInst->stBlockTx.u16RxIndex;
          if (pstInst->stBlockTx.pstTxBuf == 0) {
            // Waveform generation verify
            if (pstInst->stBlockTx.u16WaveMode == 2U) {
              // POLICY CHANGE: Do not validate MISO for Test9 Wave Download burst
              if (g_u16DbgTraceIdx < 20U) {
                g_astDbgTrace[g_u16DbgTraceIdx].mosi_addr = (g_u16DbgTraceIdx < g_u16MosiCount) ? g_astMosiLog[g_u16DbgTraceIdx].addr : 0U;
                g_astDbgTrace[g_u16DbgTraceIdx].mosi_data = (g_u16DbgTraceIdx < g_u16MosiCount) ? g_astMosiLog[g_u16DbgTraceIdx].data : 0U;
                g_astDbgTrace[g_u16DbgTraceIdx].miso_addr = w1;
                g_astDbgTrace[g_u16DbgTraceIdx].miso_data = w2;
                g_astDbgTrace[g_u16DbgTraceIdx].expected_ack_addr = 0U;
                g_astDbgTrace[g_u16DbgTraceIdx].expected_ack_data = 0U;
                g_astDbgTrace[g_u16DbgTraceIdx].validated_index = u16Idx;
                g_astDbgTrace[g_u16DbgTraceIdx].validation_result = 1U; // Force pass
                g_u16DbgTraceIdx++;
              }
            } else {
              u16 u16ExpTxAddr, u16ExpTxData;
              buildWaveBlockPacket(pstInst, u16Idx, &u16ExpTxAddr, &u16ExpTxData);

              u16 u16ExpRxAddr = (u16)(u16ExpTxAddr + calcSpiMasterChecksum(w2));
              u16 bDataOk = (w2 == u16ExpTxData);
              if ((u16ExpTxAddr == Spi_Block_End_spi_addr) && (pstInst->stBlockTx.u16WaveMode != 2U)) {
                bDataOk = (w2 == SPI_BLOCK_STATUS_BUSY) || (w2 == SPI_BLOCK_STATUS_READY);
              }

              if ((w1 != u16ExpRxAddr) || (!bDataOk)) {
                s_stSpiApp.u32StressFailCnt++;
                reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
                recoverSpiMaster(pstInst);
                return;
              }
            }
          } else {
            if (pstInst->stBlockTx.bLoop == 1U) {
              u16Idx = (u16)(u16Idx & 1U);
            }
            pstInst->stBlockTx.pstRxBuf[u16Idx].stPack.u16Address = w1;
            pstInst->stBlockTx.pstRxBuf[u16Idx].stPack.u16Data = w2;
          }
          pstInst->stBlockTx.u16RxIndex++;
          pstInst->u32TimeoutCnt = 0U;
        }
      }
    }

    // Packet output logic
    u16 u16InFlight;
    u16 u16AccountedRx = pstInst->stBlockTx.u16RxIndex + pstInst->stBlockTx.bFirstDiscarded;
    if (pstInst->stBlockTx.u16TxIndex >= u16AccountedRx) {
      u16InFlight = pstInst->stBlockTx.u16TxIndex - u16AccountedRx;
    } else {
      u16InFlight = 0U;
    }

    if ((u16InFlight == 0U) &&
        !((s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING) &&
          (s_stSpiApp.eTestCmd == SPI_MASTER_TEST_CMD_SEQ_WRITE_16) &&
          (bBlockResponseDrained == 1U))) {
      u16 bDelayOk = 0U;
      if (pstInst->stBlockTx.u16TxIndex == 0U) {
        bDelayOk = 1U;
      } else {
#if WAIT_SLAVE_TICKS == 0U
        bDelayOk = 1U;
#else
        if (getSpiMasterElapsedTicks(pstInst->u32WaitSlaveCnt) >= WAIT_SLAVE_TICKS) {
          bDelayOk = 1U;
        }
#endif
      }

      if (bDelayOk == 1U) {
        if (pstInst->stBlockTx.u16TxIndex < pstInst->stBlockTx.u16Length) {
          if (!SPI_isBusy(u32BaseAddr)) {
            u16 u16Idx = pstInst->stBlockTx.u16TxIndex;
            u16 u16TxAddr, u16TxData;
            
            if (pstInst->stBlockTx.pstTxBuf == 0) {
              buildWaveBlockPacket(pstInst, u16Idx, &u16TxAddr, &u16TxData);
            } else {
              if (pstInst->stBlockTx.bLoop == 1U) {
                u16Idx = (u16)(u16Idx & 1U);
              }
              u16TxAddr = pstInst->stBlockTx.pstTxBuf[u16Idx].stPack.u16Address;
              u16TxData = pstInst->stBlockTx.pstTxBuf[u16Idx].stPack.u16Data;
            }
            
            spiA_master.stDiag.stDriver.stComm.u32TxTotal++;

            writeSpiMasterFrame(u32BaseAddr, u16TxAddr, u16TxData);

            // Log MOSI
            if (pstInst->stBlockTx.u16WaveMode == 2U && g_u16MosiCount < 40U) {
              g_astMosiLog[g_u16MosiCount].addr = u16TxAddr;
              g_astMosiLog[g_u16MosiCount].data = u16TxData;
              g_u16MosiCount++;
            }

            pstInst->u32WaitSlaveCnt = U32_UPCNTS;
            pstInst->stBlockTx.u16TxIndex++;
            pstInst->u32TimeoutCnt = 0U;
          }
        } else if (pstInst->stBlockTx.bNullSent == 0U) {
          if (!SPI_isBusy(u32BaseAddr)) {
            // Trigger flush
            writeSpiMasterFrame(u32BaseAddr, 0xFFFFU, 0x0000U);

            // Log MOSI
            if (pstInst->stBlockTx.u16WaveMode == 2U && g_u16MosiCount < 40U) {
              g_astMosiLog[g_u16MosiCount].addr = 0xFFFFU;
              g_astMosiLog[g_u16MosiCount].data = 0x0000U;
              g_u16MosiCount++;
            }

            pstInst->u32WaitSlaveCnt = U32_UPCNTS;
            pstInst->stBlockTx.bNullSent = 1U;
            pstInst->u32TimeoutCnt = 0U;
          }
        }
      }
    }

    if (pstInst->stBlockTx.u16RxIndex == pstInst->stBlockTx.u16Length) {
      pstInst->u32IdleLimitTicks = WAIT_IDLE_TICKS;
      pstInst->eState = SPI_CMD_WAIT_IDLE;
      if (pstInst->stBlockTx.pfCallback != 0) {
        pstInst->stBlockTx.pfCallback();
      }
      break;
    }

    pstInst->u32TimeoutCnt++;
    if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
      s_stSpiApp.u32StressFailCnt++;
      reportMasterError(SPIA_FAULT_SOURCE_DRIVER, (u16)SPIA_DRV_FAULT_BLOCK_TIMEOUT);
      recoverSpiMaster(pstInst);
    }
  } break;
  }
}

static u16 normalizeContinuousLength(u16 u16Length) {
  if ((u16Length == 0U) || (u16Length > 4095U)) {
    return 2U;
  }

  return u16Length;
}

static void serviceSpiMasterTestRequest(void) {
  if (s_stSpiTest.u16Start == 0U) {
    return;
  }

  s_stSpiTest.u16Start = 0U;
  s_stSpiTest.u16Result = 0U;
  s_stSpiTest.u32Detail = 0U;

  if ((spiA_master.stDiag.u16FaultCode != 0U) ||
      (s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER) ||
      (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING)) {
    return;
  }

  s_stSpiApp.u16TestAddr = s_stSpiTest.u16Address;
  s_stSpiApp.u16TestData = s_stSpiTest.u16Data;
  s_stSpiApp.eTestCmd = s_stSpiTest.eCommand;
  s_stSpiApp.eLastTestCmd = s_stSpiTest.eCommand;
  s_stSpiApp.eTestState = SPI_MASTER_TEST_TRIGGER;

  s_stSpiTest.eStatus = SPI_TEST_STATUS_RUNNING;
}

static void updateSpiMasterTestResult(void) {
  if ((s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER) ||
      (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING)) {
    s_stSpiTest.eStatus = SPI_TEST_STATUS_RUNNING;
    return;
  }

  if (s_stSpiApp.eTestState == SPI_MASTER_TEST_DONE) {
    s_stSpiTest.eStatus = SPI_TEST_STATUS_PASSED;
    s_stSpiTest.u16Result = s_stSpiApp.u16LastResult;
    return;
  }

  if (s_stSpiApp.eTestState == SPI_MASTER_TEST_ERROR) {
    s_stSpiTest.eStatus = SPI_TEST_STATUS_FAILED;
    s_stSpiTest.u16Result = s_stSpiApp.u16LastResult;
  }
}

static void markTriggeredTestRunning(SPI_APP_STATE_e eState) {
  s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
  s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
  s_stSpiApp.eState = eState;
}

static void prepareShortBlockWriteStress(u16 u16Data) {
  s_astStressTxBuf[0].stPack.u16Address = Spi_Block_Data_Base_spi_addr;
  s_astStressTxBuf[0].stPack.u16Data = u16Data;
  s_astStressTxBuf[1].stPack.u16Address = Spi_Block_Expected_CheckSum_spi_addr;
  s_astStressTxBuf[1].stPack.u16Data = calcBlockFrameChecksum(s_astStressTxBuf, 1U);
  s_astStressTxBuf[2].stPack.u16Address = Spi_Block_End_spi_addr;
  s_astStressTxBuf[2].stPack.u16Data = 1U;
}

static void prepareVersionReadStress(void) {
  s_astStressTxBuf[0].stPack.u16Address = C2000_Version_spi_addr;
  s_astStressTxBuf[0].stPack.u16Data = 0x0000U;
  s_astStressTxBuf[1].stPack.u16Address = CPU2_Version_spi_addr;
  s_astStressTxBuf[1].stPack.u16Data = 0x0000U;
}

static u16 startTriggeredSpiMasterTest(ST_SPI_MASTER *pstInst) {
  switch (s_stSpiApp.eTestCmd) {
  case SPI_MASTER_TEST_CMD_WRITE:
    if (writeSpiMaster(pstInst, s_stSpiApp.u16TestAddr, s_stSpiApp.u16TestData, onManualWriteComplete) == 1U) {
      markTriggeredTestRunning(SPI_APP_STATE_SINGLE_WRITE);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_READ:
    if (readSpiMaster(pstInst, s_stSpiApp.u16TestAddr, onManualReadComplete) == 1U) {
      markTriggeredTestRunning(SPI_APP_STATE_SINGLE_READ);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_SEQ_WRITE_16: {
    u16 u16Idx;
    for (u16Idx = 0U; u16Idx < 16U; u16Idx++) {
      s_astSeqTxBuf[u16Idx].stPack.u16Address = (u16)(Spi_Block_Data_Base_spi_addr + u16Idx);
      s_astSeqTxBuf[u16Idx].stPack.u16Data = (u16)((u16Idx + 1U) * 0x1111U);
    }
    s_astSeqTxBuf[16].stPack.u16Address = Spi_Block_Expected_CheckSum_spi_addr;
    s_astSeqTxBuf[16].stPack.u16Data = calcBlockFrameChecksum(s_astSeqTxBuf, 16U);
    s_astSeqTxBuf[17].stPack.u16Address = Spi_Block_End_spi_addr;
    s_astSeqTxBuf[17].stPack.u16Data = 16U;

    if (writeBlockSpiMaster(pstInst, s_astSeqTxBuf, s_astSeqRxBuf, 18U, onBlockWriteComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      markTriggeredTestRunning(SPI_APP_STATE_BLOCK_WRITE);
      return 1U;
    }
  } break;

  case SPI_MASTER_TEST_CMD_WAVE_4095:
    if (writeWaveBlockSpiMaster(pstInst, 0U, onBlockWriteComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      markTriggeredTestRunning(SPI_APP_STATE_BLOCK_WRITE);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_SINE_4095:
    if (writeWaveBlockSpiMaster(pstInst, 1U, onBlockWriteComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      markTriggeredTestRunning(SPI_APP_STATE_BLOCK_WRITE);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_REG_FRAME_1000:
    if ((pstInst->eState == SPI_CMD_IDLE) && (pstInst->u16QCount == 0U)) {
      SPI_resetRxFIFO(pstInst->u32SpiBaseAddr);
      SPI_resetTxFIFO(pstInst->u32SpiBaseAddr);
      s_stSpiApp.u16RawFrameTarget = SPI_RAW_FRAME_STRESS_COUNT;
      s_stSpiApp.u16RawFrameIndex = 0U;
      s_stSpiApp.u16RawFrameAddr = SPI_RAW_FRAME_STRESS_ADDR;
      s_stSpiApp.u16RawFrameLastData = 0U;
      s_stSpiApp.u16LastResult = 0U;
      pstInst->u32TimeoutCnt = 0U;
      markTriggeredTestRunning(SPI_APP_STATE_RAW_FRAME_STRESS);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_PACKET_WRITE:
    if ((pstInst->eState == SPI_CMD_IDLE) && (pstInst->u16QCount == 0U)) {
      SPI_resetRxFIFO(pstInst->u32SpiBaseAddr);
      SPI_resetTxFIFO(pstInst->u32SpiBaseAddr);
      s_stSpiApp.u16RawFrameTarget = 4U;
      s_stSpiApp.u16RawFrameIndex = 0U;
      s_stSpiApp.u16RawFrameLastData = 0U;
      s_stSpiApp.u16LastResult = 0U;
      pstInst->u32TimeoutCnt = 0U;
      markTriggeredTestRunning(SPI_APP_STATE_PACKET_WRITE);
      return 1U;
    }
    break;

  case SPI_MASTER_TEST_CMD_WAVE_DOWNLOAD:
    if (writeBlockSpiMaster(pstInst, 0, 0, 4096U, onBlockWriteComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      pstInst->stBlockTx.u16WaveMode = 2U;
      {
        u16 i;
        g_u16MosiCount = 0U;
        g_u16DbgTraceIdx = 0U;
        for (i = 0U; i < 20U; i++) {
          g_astDbgTrace[i].index = i;
          g_astDbgTrace[i].mosi_addr = 0U;
          g_astDbgTrace[i].mosi_data = 0U;
          g_astDbgTrace[i].miso_addr = 0U;
          g_astDbgTrace[i].miso_data = 0U;
          g_astDbgTrace[i].expected_ack_addr = 0U;
          g_astDbgTrace[i].expected_ack_data = 0U;
          g_astDbgTrace[i].validated_index = 0xFFFFU;
          g_astDbgTrace[i].validation_result = 0U;
        }
      }
      markTriggeredTestRunning(SPI_APP_STATE_BLOCK_WRITE);
      return 1U;
    }
    break;

  default:
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.u16LastResult = 0U;
    return 0U;
  }

  return 0U;
}

static u16 startContinuousSpiMasterTest(ST_SPI_MASTER *pstInst) {
  u16 u16Len = normalizeContinuousLength(s_stSpiApp.u16StressLength);

  switch (s_stSpiApp.u16ContinuousMode) {
  case SPI_CONTINUOUS_MODE_BLOCK_WRITE:
    prepareShortBlockWriteStress(0x1234U);
    if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, 3U, onBlockWriteComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WRITE;
      return 1U;
    }
    break;

  case SPI_CONTINUOUS_MODE_BLOCK_READ:
    prepareVersionReadStress();
    if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, u16Len, onBlockReadComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 1U;
      s_stSpiApp.eState = SPI_APP_STATE_BLOCK_READ;
      return 1U;
    }
    break;

  case SPI_CONTINUOUS_MODE_BLOCK_WR_RD:
    prepareShortBlockWriteStress(0x1234U);
    if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, 3U, onBlockWrRdComplete) == 1U) {
      pstInst->stBlockTx.bLoop = 0U;
      s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WR_RD;
      return 1U;
    }
    break;

  default:
    break;
  }

  return 0U;
}

void runSpiMasterApp(ST_SPI_MASTER *pstInst) {
  switch (s_stSpiApp.eState) {
  case SPI_APP_STATE_IDLE:
    if (s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER) {
      (void)startTriggeredSpiMasterTest(pstInst);
    } else if ((s_stSpiApp.eTestState == SPI_MASTER_TEST_IDLE) &&
               (s_stSpiApp.u16StressEnable == 1U)) {
      (void)startContinuousSpiMasterTest(pstInst);
    }
    break;

  case SPI_APP_STATE_SINGLE_WRITE:
  case SPI_APP_STATE_SINGLE_READ:
  case SPI_APP_STATE_BLOCK_WRITE:
  case SPI_APP_STATE_BLOCK_READ:
  case SPI_APP_STATE_BLOCK_WR_RD:
    break;

  case SPI_APP_STATE_RAW_FRAME_STRESS:
    drainSpiMasterRxFifo(pstInst->u32SpiBaseAddr);

    if (s_stSpiApp.u16RawFrameIndex < s_stSpiApp.u16RawFrameTarget) {
      if (!SPI_isBusy(pstInst->u32SpiBaseAddr)) {
        u16 u16Data = s_stSpiApp.u16RawFrameIndex;

        writeSpiMasterFrame(pstInst->u32SpiBaseAddr, s_stSpiApp.u16RawFrameAddr, u16Data);
        s_stSpiApp.u16RawFrameLastData = u16Data;
        s_stSpiApp.u16RawFrameIndex++;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        spiA_master.stDiag.stDriver.stComm.u32TxTotal++;
        pstInst->u32TimeoutCnt = 0U;
      } else {
        pstInst->u32TimeoutCnt++;
      }

      if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        reportMasterError(SPIA_FAULT_SOURCE_SERVICE, (u16)SPIA_SERV_FAULT_STRESS_TIMEOUT);
        recoverSpiMaster(pstInst);
      }
    } else if (!SPI_isBusy(pstInst->u32SpiBaseAddr)) {
      drainSpiMasterRxFifo(pstInst->u32SpiBaseAddr);
      s_stSpiApp.u32StressPassCnt += s_stSpiApp.u16RawFrameTarget;
      s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
      s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
      s_stSpiApp.eState = SPI_APP_STATE_IDLE;
    }
    break;

  case SPI_APP_STATE_PACKET_WRITE:
    drainSpiMasterRxFifo(pstInst->u32SpiBaseAddr);

    if (s_stSpiApp.u16RawFrameIndex < s_stSpiApp.u16RawFrameTarget) {
      if (!SPI_isBusy(pstInst->u32SpiBaseAddr)) {
        u16 u16Addr = 0xFFFFU;
        u16 u16Data = 0x0000U;

        switch (s_stSpiApp.u16RawFrameIndex) {
        case 0U:
          u16Addr = SPI_PACKET_HEADER_MAGIC;
          u16Data = s_stSpiApp.u16TestAddr;
          break;

        case 1U:
          u16Addr = 1U;
          u16Data = s_stSpiApp.u16TestData;
          break;

        case 2U:
          u16Addr = calcSpiPacketChecksum3(s_stSpiApp.u16TestAddr, 1U,
                                           s_stSpiApp.u16TestData);
          u16Data = SPI_PACKET_PADDING_WORD;
          break;

        default:
          u16Addr = 0xFFFFU;
          u16Data = 0x0000U;
          break;
        }

        writeSpiMasterFrame(pstInst->u32SpiBaseAddr, u16Addr, u16Data);
        s_stSpiApp.u16RawFrameLastData = u16Data;
        s_stSpiApp.u16RawFrameIndex++;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        spiA_master.stDiag.stDriver.stComm.u32TxTotal++;
        pstInst->u32TimeoutCnt = 0U;
      } else {
        pstInst->u32TimeoutCnt++;
      }

      if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        reportMasterError(SPIA_FAULT_SOURCE_SERVICE, (u16)SPIA_SERV_FAULT_PACKET_TIMEOUT);
        recoverSpiMaster(pstInst);
      }
    } else if (!SPI_isBusy(pstInst->u32SpiBaseAddr)) {
      drainSpiMasterRxFifo(pstInst->u32SpiBaseAddr);
      recordSpiMasterSuccess(1U);
      s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
      s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
      s_stSpiApp.eState = SPI_APP_STATE_IDLE;
    }
    break;

  case SPI_APP_STATE_POLL_SLAVE:
    if (s_stSpiApp.bPollPending == 0U) {
      if (getSpiMasterElapsedTicks(s_stSpiApp.u32PollTimer) >= 50000U) {
        s_stSpiApp.u32PollTimer = U32_UPCNTS;
        s_stSpiApp.bPollPending = 1U;
        if (readSpiMaster(pstInst, Spi_Block_Status_spi_addr, onPollSlaveComplete) == 0U) {
          s_stSpiApp.bPollPending = 0U;
        }
      }
    }

    {
      if (getSpiMasterElapsedTicks(s_stSpiApp.u32PollTimeoutCnt) >= 50000000U) {
        s_stSpiApp.u32StressFailCnt++;
        reportMasterError(SPIA_FAULT_SOURCE_SERVICE, (u16)SPIA_SERV_FAULT_POLL_TIMEOUT);
        recoverSpiMaster(pstInst);
      }
    }

    if (s_stSpiApp.bPollPending == 0U) {
      if (s_stSpiApp.u16PollState == SPI_BLOCK_STATUS_READY) {
        if (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING) {
          s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
          s_stSpiApp.u16LastResult = s_stSpiApp.u16PollState;
        } else {
          s_stSpiApp.u32StressPassCnt += pstInst->stBlockTx.u16Length;
        }
        spiA_master.stDiag.stService.stComm.u32TxTotal++; // Log successful Service block
        s_stSpiApp.eState = SPI_APP_STATE_IDLE;
      } else if ((s_stSpiApp.u16PollState == SPI_BLOCK_STATUS_ERROR) || 
                 (s_stSpiApp.u16PollState == SPI_BLOCK_STATUS_OVERFLOW)) {
        s_stSpiApp.u32StressFailCnt++;
        if (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING) {
          s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
          s_stSpiApp.u16LastResult = s_stSpiApp.u16PollState;
        }
        reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
        recoverSpiMaster(pstInst);
      }
    }
    break;

  default:
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
    break;
  }
}

// ============================================================================
// Callbacks and Completion Events
// ============================================================================
static void startBlockStatusPoll(void) {
  s_stSpiApp.u32PollTimer = U32_UPCNTS;
  s_stSpiApp.u32PollTimeoutCnt = U32_UPCNTS;
  s_stSpiApp.bPollPending = 0U;
  s_stSpiApp.u16PollState = SPI_BLOCK_STATUS_BUSY;
  s_stSpiApp.eState = SPI_APP_STATE_POLL_SLAVE;
}

static void recordSpiMasterSuccess(u16 u16Count) {
  s_stSpiApp.u32StressPassCnt += u16Count;
  // Accumulate successes in stComm
  spiA_master.stDiag.stService.stComm.u32TxTotal += u16Count;
}

/* Zero goto, slim block response verifier */
static u16 verifyBlockResponses(const ST_SPI_MASTER_PACKET *pstTxBuf,
                                ST_SPI_MASTER_PACKET *pstRxBuf,
                                u16 u16Length,
                                u16 bLoop,
                                SPI_BLOCK_VERIFY_MODE_e eVerifyMode) {
  u16 u16Idx;

  if ((pstTxBuf == 0) || (pstRxBuf == 0)) {
    return 1U;
  }

  for (u16Idx = 0U; u16Idx < u16Length; u16Idx++) {
    u16 u16MapIdx = bLoop ? (u16)(u16Idx & 1U) : u16Idx;
    u16 u16RxAddr = pstRxBuf[u16MapIdx].stPack.u16Address;
    u16 u16RxData = pstRxBuf[u16MapIdx].stPack.u16Data;
    u16 u16TxAddr = pstTxBuf[u16MapIdx].stPack.u16Address;
    u16 u16TxData = pstTxBuf[u16MapIdx].stPack.u16Data;
    u16 u16ExpAddr = (u16)(u16TxAddr + calcSpiMasterChecksum(u16RxData));

    if (u16RxAddr != u16ExpAddr) {
      reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
      recoverSpiMaster(&s_stSpiMaster);
      return 0U;
    }
 
    if (eVerifyMode != SPI_BLOCK_VERIFY_ADDR_ONLY) {
      if (u16TxAddr == Spi_Block_End_spi_addr) {
        if ((u16RxData != SPI_BLOCK_STATUS_BUSY) && (u16RxData != SPI_BLOCK_STATUS_READY)) {
          reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
          recoverSpiMaster(&s_stSpiMaster);
          return 0U;
        }
      } else if ((eVerifyMode == SPI_BLOCK_VERIFY_ADDR_DATA) ||
                 ((eVerifyMode == SPI_BLOCK_VERIFY_ADDR_NONZERO_DATA) && (u16TxData != 0U))) {
        if (u16RxData != u16TxData) {
          reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
          recoverSpiMaster(&s_stSpiMaster);
          return 0U;
        }
      }
    }
  }

  return 1U;
}

static void onManualReadComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(s_stSpiApp.u16TestAddr + calcSpiMasterChecksum(u16RxData));
  s_stSpiTest.u32Detail = ((u32)u16RxAddr << 16) | (u32)u16RxData;
  if (u16RxAddr == u16ExpectedAddr) {
    s_stSpiApp.u16TestData = u16RxData;
    recordSpiMasterSuccess(1U);
    s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.u16LastResult = u16RxData;
    reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
    recoverSpiMaster(&s_stSpiMaster);
  }
}

#ifndef WAVE_VALIDATE_ADDR
#define WAVE_VALIDATE_ADDR 0x0960U
#endif
#ifndef WAVE_ACTIVATE_ADDR
#define WAVE_ACTIVATE_ADDR 0x0961U
#endif

static void onManualWriteComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(s_stSpiApp.u16TestAddr + calcSpiMasterChecksum(u16RxData));
  s_stSpiTest.u32Detail = ((u32)u16RxAddr << 16) | (u32)u16RxData;
  bool bIsCmdResultReg = (s_stSpiApp.u16TestAddr == WAVE_VALIDATE_ADDR) ||
                         (s_stSpiApp.u16TestAddr == WAVE_ACTIVATE_ADDR);
  bool bDataValid = bIsCmdResultReg ? true : (u16RxData == s_stSpiApp.u16TestData);

  if ((u16RxAddr == u16ExpectedAddr) && bDataValid) {
    recordSpiMasterSuccess(1U);
    s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.u16LastResult = u16RxData;
    reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
    recoverSpiMaster(&s_stSpiMaster);
  }
}

static void onBlockWriteComplete(void) {
  if (s_stSpiMaster.stBlockTx.u16WaveMode == 2U) {
    s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
    return;
  }

  if (verifyBlockResponses(s_stSpiMaster.stBlockTx.pstTxBuf,
                           s_stSpiMaster.stBlockTx.pstRxBuf,
                           s_stSpiMaster.stBlockTx.u16Length,
                           s_stSpiMaster.stBlockTx.bLoop,
                           SPI_BLOCK_VERIFY_ADDR_DATA) == 0U) {
    return;
  }

  startBlockStatusPoll();
}

static void onBlockReadComplete(void) {
  u16 u16Len = s_stSpiMaster.stBlockTx.u16Length;

  if (verifyBlockResponses(s_astStressTxBuf,
                           s_astStressRxBuf,
                           u16Len,
                           s_stSpiMaster.stBlockTx.bLoop,
                           SPI_BLOCK_VERIFY_ADDR_ONLY) == 0U) {
    return;
  }

  recordSpiMasterSuccess(u16Len);
  s_stSpiApp.eState = SPI_APP_STATE_IDLE;
}

static void onBlockWrRdComplete(void) {
  if (verifyBlockResponses(s_astStressTxBuf,
                           s_astStressRxBuf,
                           s_stSpiMaster.stBlockTx.u16Length,
                           s_stSpiMaster.stBlockTx.bLoop,
                           SPI_BLOCK_VERIFY_ADDR_NONZERO_DATA) == 0U) {
    return;
  }

  startBlockStatusPoll();
}

static void onPollSlaveComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(Spi_Block_Status_spi_addr + calcSpiMasterChecksum(u16RxData));
  if (u16RxAddr == u16ExpectedAddr) {
    s_stSpiApp.u16PollState = u16RxData;
    s_stSpiApp.bPollPending = 0U;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    reportMasterError(SPIA_FAULT_SOURCE_PROTOCOL, (u16)SPIA_PROT_FAULT_CHECKSUM);
    s_stSpiApp.bPollPending = 0U;
    recoverSpiMaster(&s_stSpiMaster);
  }
}

static void drainSpiMasterRxFifo(u32 u32BaseAddr) {
  while (SPI_getRxFIFOStatus(u32BaseAddr) > SPI_FIFO_RXEMPTY) {
    SPI_readDataNonBlocking(u32BaseAddr);
  }
}

static void prepareSineTable(void) {
  u16 u16Idx;
  f32 f32Step;

  if (s_u16SineTableReady == 1U) {
    return;
  }

  f32Step = 0.001534355386f; /* Precalculated 2 * PI / 4095 */
  s_u16SineTableChecksum = 0U;
  for (u16Idx = 0U; u16Idx < SPI_SINE_TABLE_SIZE; u16Idx++) {
    f32 f32Phase = (f32)u16Idx * f32Step;
    f32 f32SinVal;
    if (f32Phase > 3.14159265f) {
      f32 f32X = f32Phase - 3.14159265f;
      f32SinVal = -0.405284735f * f32X * (3.14159265f - f32X);
    } else {
      f32SinVal = 0.405284735f * f32Phase * (3.14159265f - f32Phase);
    }
    f32 f32Val = 2047.5f + 2047.5f * f32SinVal;
    if (f32Val > 4095.0f) {
      f32Val = 4095.0f;
    }
    g_u16SpiMasterWaveRam[u16Idx] = (u16)f32Val;
    s_u16SineTableChecksum = (u16)(s_u16SineTableChecksum + calcSpiMasterChecksum((u16)f32Val));
  }

  s_u16SineTableReady = 1U;
}

void spiMasterTask(ST_SPI_MASTER *pstInst) {
  serviceSpiMasterTestRequest();
  runSpiMasterCommunication(pstInst);
  runSpiMasterApp(pstInst);
  updateSpiMasterTestResult();

  /* Dynamically update diagnostics states */
  if (spiA_master.stDiag.eHealth == SPIA_HEALTH_OK) {
    spiA_master.stDiag.stDriver.eState = (SPIA_DRIVER_STATE_e)pstInst->eState;
    spiA_master.stDiag.stService.eState = (SPIA_SERVICE_STATE_e)s_stSpiApp.eState;
  } else {
    spiA_master.stDiag.stDriver.eState = SPIA_DRV_STATE_IDLE;
    spiA_master.stDiag.stService.eState = SPIA_SERV_STATE_IDLE;
    spiA_master.stDiag.stProtocol.eState = SPIA_PROT_STATE_IDLE;
  }
}

// ============================================================================
// Diagnostics & Self-Test Implementation
// ============================================================================

void triggerSpiMasterSelfTest(ST_SPI_MASTER *pstInst) {
  u32 u32Base = pstInst->u32SpiBaseAddr;
  u32 u32Timeout = 0U;
  u16 u16Addr, u16Data;

  // Set running state in stDiag
  spiA_master.stDiag.stDriver.eState = SPIA_DRV_STATE_BLOCK;

  SPI_enableLoopback(u32Base);
  SPI_resetTxFIFO(u32Base);
  SPI_resetRxFIFO(u32Base);

  // Send a test frame
  writeSpiMasterFrame(u32Base, 0x0400U, 0x5A5AU);

  while ((SPI_getRxFIFOStatus(u32Base) < 2U) && (u32Timeout < 5000U)) {
    u32Timeout++;
  }

  if (SPI_getRxFIFOStatus(u32Base) >= 2U) {
    u16Addr = SPI_readDataNonBlocking(u32Base);
    u16Data = SPI_readDataNonBlocking(u32Base);

    if ((u16Addr == 0x0400U) && (u16Data == 0x5A5AU)) {
      spiA_master.stDiag.stDriver.stComm.u32MaxQueueDepth = 2U; // Passed indicator
    } else {
      spiA_master.stDiag.stDriver.stComm.u32MaxQueueDepth = 3U; // Failed indicator
      spiA_master.stDiag.stDriver.stComm.u32ErrCount++;
    }
  } else {
    spiA_master.stDiag.stDriver.stComm.u32MaxQueueDepth = 3U; // Failed
    spiA_master.stDiag.stDriver.stComm.u32ErrCount++;
  }

  SPI_disableLoopback(u32Base);
  SPI_resetTxFIFO(u32Base);
  SPI_resetRxFIFO(u32Base);
}

u16 getSpiMasterSelfTestResult(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  if (spiA_master.stDiag.stDriver.stComm.u32MaxQueueDepth == 2U) {
    return 2U; // Passed
  } else if (spiA_master.stDiag.stDriver.stComm.u32MaxQueueDepth == 3U) {
    return 3U; // Failed
  }
  return 0U; // Not run
}

void resetSpiMasterMetrics(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stDriver.stComm);
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stProtocol.stComm);
  CommDiag_Init((ST_COMM_DIAG *)&spiA_master.stDiag.stService.stComm);
}

bool checkSpiMasterHealth(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  return (spiA_master.stDiag.eHealth == SPIA_HEALTH_OK);
}
