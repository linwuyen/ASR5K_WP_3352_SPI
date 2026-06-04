/*
 * SPI_master.c
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
#include <math.h>

// ============================================================================
// Private Macros & Configuration (Based on 50MHz hardware timer)
// ============================================================================
#define WAIT_SLAVE_TICKS 1000U    // 20us slave delay under 50MHz SW_TIMER
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

#pragma DATA_SECTION(g_u16SpiMasterWaveRam, "spia_master_wave_ram")
volatile u16 g_u16SpiMasterWaveRam[SPI_SINE_TABLE_SIZE];

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

// ============================================================================
// Private Helper Functions & Queue Operations
// ============================================================================
u16 calcSpiMasterChecksum(u16 u16Data) {
  return (u16)((u16Data >> 8) + (u16Data & 0x00FFU));
}

// Transaction enqueue implementation (Hungarian notation)
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

// Transaction dequeue implementation (Hungarian notation)
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

  s_stSpiDiag.u16ConsecutiveSuccess = 0U;
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
  s_stSpiApp.u16ContinuousMode = 3U;
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
ST_SPI_DIAG *getSpiDiagHandle(void) { return &s_stSpiDiag; }

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
  if ((u16Length == 0U) || (u16Length > 4096U)) {
    return 0U;
  }
  if ((pstTxBuf == 0) && (pstRxBuf == 0)) {
    /* Waveform generation mode */
  } else if ((pstTxBuf == 0) || (pstRxBuf == 0) || (u16Length > 4095U)) {
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
  if (writeBlockSpiMaster(pstInst, 0, 0, 4096U, pfCb) == 0U) {
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

      // Log TX Metric
      s_stSpiDiag.u32TotalTxCount++;

      SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr,
                               pstInst->stActiveTx.stTxPacket.stPack.u16Address);
      SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr,
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
        s_stSpiDiag.u16LastErrType = 1U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.u16StressStep;
        s_stSpiDiag.u16LastErrRxAddr = 0U;
        s_stSpiDiag.u16LastErrRxData = 0U;
        s_stSpiDiag.u16LastErrExpAddr = pstInst->stActiveTx.stTxPacket.stPack.u16Address;
        s_stSpiDiag.u16LastErrExpData = pstInst->stActiveTx.stTxPacket.stPack.u16Data;
        recoverSpiMaster(pstInst);
      }
    }
    break;

  case SPI_CMD_WAIT_SLAVE: {
    u32 u32Elapsed;
    u32 u32Now = U32_UPCNTS;

    if (u32Now >= pstInst->u32WaitSlaveCnt) {
      u32Elapsed = u32Now - pstInst->u32WaitSlaveCnt;
    } else {
      u32Elapsed = (SW_TIMER - pstInst->u32WaitSlaveCnt) + u32Now;
    }

    if (u32Elapsed >= WAIT_SLAVE_TICKS) {
      // Write Null transaction to clock out responses
      SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr, 0xFFFFU);
      SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr, 0x0000U);
      pstInst->u32TimeoutCnt = 0U;
      pstInst->eState = SPI_CMD_WAIT_DATA;
    }
  } break;

  case SPI_CMD_WAIT_DATA:
    if (SPI_getRxFIFOStatus(pstInst->u32SpiBaseAddr) >= 2U) {
      pstInst->u16RxAddrChecksum = SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);
      pstInst->u16FinalRxData = SPI_readDataNonBlocking(pstInst->u32SpiBaseAddr);

      s_stSpiDiag.u32TotalRxCount++;

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
        s_stSpiDiag.u16LastErrType = 2U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.u16StressStep;
        s_stSpiDiag.u16LastErrRxAddr = 0U;
        s_stSpiDiag.u16LastErrRxData = 0U;
        s_stSpiDiag.u16LastErrExpAddr = 0xFFFFU;
        s_stSpiDiag.u16LastErrExpData = 0x0000U;
        recoverSpiMaster(pstInst);
      }
    }
    break;

  case SPI_CMD_WAIT_IDLE: {
    u32 u32Elapsed;
    u32 u32Now = U32_UPCNTS;

    if (u32Now >= pstInst->u32WaitSlaveCnt) {
      u32Elapsed = u32Now - pstInst->u32WaitSlaveCnt;
    } else {
      u32Elapsed = (SW_TIMER - pstInst->u32WaitSlaveCnt) + u32Now;
    }

    if (u32Elapsed >= pstInst->u32IdleLimitTicks) {
      pstInst->u32IdleLimitTicks = WAIT_IDLE_TICKS;
      pstInst->eState = SPI_CMD_IDLE;
    }
  } break;

  case SPI_CMD_BLOCK_TX: {
    u32 u32BaseAddr = pstInst->u32SpiBaseAddr;

    // Read back data block
    while (SPI_getRxFIFOStatus(u32BaseAddr) >= 2U) {
      u16 w1 = SPI_readDataNonBlocking(u32BaseAddr);
      u16 w2 = SPI_readDataNonBlocking(u32BaseAddr);

      s_stSpiDiag.u32TotalRxCount++;

      if (pstInst->stBlockTx.bFirstDiscarded == 0U) {
        pstInst->stBlockTx.bFirstDiscarded = 1U;
        pstInst->u32TimeoutCnt = 0U;
      } else {
        if (pstInst->stBlockTx.u16RxIndex < pstInst->stBlockTx.u16Length) {
          u16 u16Idx = pstInst->stBlockTx.u16RxIndex;
          if (pstInst->stBlockTx.pstTxBuf == 0) {
            // Waveform generation verify
            u16 u16ExpTxAddr, u16ExpTxData;
            if (u16Idx < SPI_SINE_TABLE_SIZE) {
              u16ExpTxAddr = (u16)(Spi_Block_Data_Base_spi_addr + u16Idx);
              if (pstInst->stBlockTx.u16WaveMode == 1U) {
                u16ExpTxData = g_u16SpiMasterWaveRam[u16Idx];
              } else {
                u16ExpTxData = (u16)((u16Idx + 1U) * 16U);
              }
            } else {
              u16ExpTxAddr = Spi_Block_End_spi_addr;
              u16ExpTxData = 4095U;
            }
            
            u16 u16ExpRxAddr = (u16)(u16ExpTxAddr + calcSpiMasterChecksum(w2));
            u16 bDataOk = (w2 == u16ExpTxData);
            if (u16ExpTxAddr == Spi_Block_End_spi_addr) {
              bDataOk = (w2 == SPI_BLOCK_STATUS_BUSY) || (w2 == SPI_BLOCK_STATUS_READY);
            }
            
            if ((w1 != u16ExpRxAddr) || (!bDataOk)) {
              s_stSpiApp.u32StressFailCnt++;
              s_stSpiDiag.u16LastErrType = 3U;
              s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
              s_stSpiDiag.u16LastErrRxAddr = w1;
              s_stSpiDiag.u16LastErrRxData = w2;
              s_stSpiDiag.u16LastErrExpAddr = u16ExpRxAddr;
              s_stSpiDiag.u16LastErrExpData = u16ExpTxData;
              recoverSpiMaster(pstInst);
              return;
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
    u16 u16InFlight = (u16)(pstInst->stBlockTx.u16TxIndex - (pstInst->stBlockTx.u16RxIndex + pstInst->stBlockTx.bFirstDiscarded));

    if (u16InFlight == 0U) {
      u16 bDelayOk = 0U;
      if (pstInst->stBlockTx.u16TxIndex == 0U) {
        bDelayOk = 1U;
      } else {
        u32 u32Elapsed;
        u32 u32Now = U32_UPCNTS;
        if (u32Now >= pstInst->u32WaitSlaveCnt) {
          u32Elapsed = u32Now - pstInst->u32WaitSlaveCnt;
        } else {
          u32Elapsed = (SW_TIMER - pstInst->u32WaitSlaveCnt) + u32Now;
        }
        if (u32Elapsed >= WAIT_SLAVE_TICKS) {
          bDelayOk = 1U;
        }
      }

      if (bDelayOk == 1U) {
        if (pstInst->stBlockTx.u16TxIndex < pstInst->stBlockTx.u16Length) {
          if (!SPI_isBusy(u32BaseAddr)) {
            u16 u16Idx = pstInst->stBlockTx.u16TxIndex;
            u16 u16TxAddr, u16TxData;
            
            if (pstInst->stBlockTx.pstTxBuf == 0) {
              if (u16Idx < SPI_SINE_TABLE_SIZE) {
                u16TxAddr = (u16)(Spi_Block_Data_Base_spi_addr + u16Idx);
                if (pstInst->stBlockTx.u16WaveMode == 1U) {
                  u16TxData = g_u16SpiMasterWaveRam[u16Idx];
                } else {
                  u16TxData = (u16)((u16Idx + 1U) * 16U);
                }
              } else {
                u16TxAddr = Spi_Block_End_spi_addr;
                u16TxData = 4095U;
              }
            } else {
              if (pstInst->stBlockTx.bLoop == 1U) {
                u16Idx = (u16)(u16Idx & 1U);
              }
              u16TxAddr = pstInst->stBlockTx.pstTxBuf[u16Idx].stPack.u16Address;
              u16TxData = pstInst->stBlockTx.pstTxBuf[u16Idx].stPack.u16Data;
            }
            
            s_stSpiDiag.u32TotalTxCount++;

            SPI_writeDataNonBlocking(u32BaseAddr, u16TxAddr);
            SPI_writeDataNonBlocking(u32BaseAddr, u16TxData);
            pstInst->u32WaitSlaveCnt = U32_UPCNTS;
            pstInst->stBlockTx.u16TxIndex++;
            pstInst->u32TimeoutCnt = 0U;
          }
        } else if (pstInst->stBlockTx.bNullSent == 0U) {
          if (!SPI_isBusy(u32BaseAddr)) {
            // Trigger flush
            SPI_writeDataNonBlocking(u32BaseAddr, 0xFFFFU);
            SPI_writeDataNonBlocking(u32BaseAddr, 0x0000U);
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
      s_stSpiDiag.u16LastErrType = 5U;
      s_stSpiDiag.u16LastErrStep = s_stSpiApp.u16StressStep;
      s_stSpiDiag.u16LastErrRxAddr = 0U;
      s_stSpiDiag.u16LastErrRxData = 0U;
      s_stSpiDiag.u16LastErrExpAddr = 0xFFFFU;
      s_stSpiDiag.u16LastErrExpData = 0x0000U;
      recoverSpiMaster(pstInst);
    }
  } break;
  }
}

void runSpiMasterApp(ST_SPI_MASTER *pstInst) {
  switch (s_stSpiApp.eState) {
  case SPI_APP_STATE_IDLE:
    if (s_stSpiApp.eTestState == SPI_MASTER_TEST_TRIGGER) {
      switch (s_stSpiApp.eTestCmd) {
      case SPI_MASTER_TEST_CMD_WRITE:
        if (writeSpiMaster(pstInst, s_stSpiApp.u16TestAddr, s_stSpiApp.u16TestData, onManualWriteComplete) == 1U) {
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.eState = SPI_APP_STATE_SINGLE_WRITE;
        }
        break;

      case SPI_MASTER_TEST_CMD_READ:
        if (readSpiMaster(pstInst, s_stSpiApp.u16TestAddr, onManualReadComplete) == 1U) {
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.eState = SPI_APP_STATE_SINGLE_READ;
        }
        break;

      case SPI_MASTER_TEST_CMD_SEQ_WRITE_16: {
        u16 u16Idx;
        for (u16Idx = 0U; u16Idx < 16U; u16Idx++) {
          s_astSeqTxBuf[u16Idx].stPack.u16Address = (u16)(Spi_Block_Data_Base_spi_addr + u16Idx);
          s_astSeqTxBuf[u16Idx].stPack.u16Data = (u16)((u16Idx + 1U) * 0x1111U);
        }
        s_astSeqTxBuf[16].stPack.u16Address = Spi_Block_End_spi_addr;
        s_astSeqTxBuf[16].stPack.u16Data = 16U;

        if (writeBlockSpiMaster(pstInst, s_astSeqTxBuf, s_astSeqRxBuf, 17U, onBlockWriteComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 0U;
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WRITE;
        }
      } break;

      case SPI_MASTER_TEST_CMD_WAVE_4095:
        if (writeWaveBlockSpiMaster(pstInst, 0U, onBlockWriteComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 0U;
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WRITE;
        }
        break;

      case SPI_MASTER_TEST_CMD_SINE_4095:
        if (writeWaveBlockSpiMaster(pstInst, 1U, onBlockWriteComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 0U;
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WRITE;
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
          s_stSpiApp.eLastTestCmd = s_stSpiApp.eTestCmd;
          s_stSpiApp.eTestState = SPI_MASTER_TEST_RUNNING;
          s_stSpiApp.u16LastResult = 0U;
          s_stSpiApp.eState = SPI_APP_STATE_RAW_FRAME_STRESS;
          pstInst->u32TimeoutCnt = 0U;
        }
        break;

      default:
        s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
        s_stSpiApp.u16LastResult = 0U;
        break;
      }
    } else if ((s_stSpiApp.eTestState == SPI_MASTER_TEST_IDLE) &&
               (s_stSpiApp.u16StressEnable == 1U)) {
      u16 u16Len = s_stSpiApp.u16StressLength;
      if ((u16Len == 0U) || (u16Len > 4095U)) {
        u16Len = 2U;
      }

      if (s_stSpiApp.u16ContinuousMode == 3U) {
        s_astStressTxBuf[0].stPack.u16Address = Spi_Block_Data_Base_spi_addr;
        s_astStressTxBuf[0].stPack.u16Data = 0x1234U;
        s_astStressTxBuf[1].stPack.u16Address = Spi_Block_End_spi_addr;
        s_astStressTxBuf[1].stPack.u16Data = u16Len;

        if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, u16Len, onBlockWriteComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 1U;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WRITE;
        }
      } else if (s_stSpiApp.u16ContinuousMode == 4U) {
        s_astStressTxBuf[0].stPack.u16Address = C2000_Version_spi_addr;
        s_astStressTxBuf[0].stPack.u16Data = 0x0000U;
        s_astStressTxBuf[1].stPack.u16Address = CPU2_Version_spi_addr;
        s_astStressTxBuf[1].stPack.u16Data = 0x0000U;

        if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, u16Len, onBlockReadComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 1U;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_READ;
        }
      } else if (s_stSpiApp.u16ContinuousMode == 5U) {
        s_astStressTxBuf[0].stPack.u16Address = Spi_Block_Data_Base_spi_addr;
        s_astStressTxBuf[0].stPack.u16Data = 0x1234U;
        s_astStressTxBuf[1].stPack.u16Address = Spi_Block_End_spi_addr;
        s_astStressTxBuf[1].stPack.u16Data = u16Len;

        if (writeBlockSpiMaster(pstInst, s_astStressTxBuf, s_astStressRxBuf, u16Len, onBlockWrRdComplete) == 1U) {
          pstInst->stBlockTx.bLoop = 1U;
          s_stSpiApp.eState = SPI_APP_STATE_BLOCK_WR_RD;
        }
      }
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

        SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr, s_stSpiApp.u16RawFrameAddr);
        SPI_writeDataNonBlocking(pstInst->u32SpiBaseAddr, u16Data);
        s_stSpiApp.u16RawFrameLastData = u16Data;
        s_stSpiApp.u16RawFrameIndex++;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        s_stSpiDiag.u32TotalTxCount++;
        pstInst->u32TimeoutCnt = 0U;
      } else {
        pstInst->u32TimeoutCnt++;
      }

      if (pstInst->u32TimeoutCnt > SPI_TIMEOUT_LIMIT) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
        s_stSpiApp.u16LastResult = s_stSpiApp.u16RawFrameIndex;
        s_stSpiDiag.u16LastErrType = 7U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
        s_stSpiDiag.u16LastErrRxAddr = s_stSpiApp.u16RawFrameAddr;
        s_stSpiDiag.u16LastErrRxData = s_stSpiApp.u16RawFrameLastData;
        s_stSpiDiag.u16LastErrExpAddr = s_stSpiApp.u16RawFrameAddr;
        s_stSpiDiag.u16LastErrExpData = s_stSpiApp.u16RawFrameTarget;
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

  case SPI_APP_STATE_POLL_SLAVE:
    if (s_stSpiApp.bPollPending == 0U) {
      u32 u32Elapsed;
      u32 u32Now = U32_UPCNTS;

      if (u32Now >= s_stSpiApp.u32PollTimer) {
        u32Elapsed = u32Now - s_stSpiApp.u32PollTimer;
      } else {
        u32Elapsed = (SW_TIMER - s_stSpiApp.u32PollTimer) + u32Now;
      }

      if (u32Elapsed >= 50000U) {
        s_stSpiApp.u32PollTimer = u32Now;
        s_stSpiApp.bPollPending = 1U;
        if (readSpiMaster(pstInst, Spi_Block_Status_spi_addr, onPollSlaveComplete) == 0U) {
          s_stSpiApp.bPollPending = 0U;
        }
      }
    }

    {
      u32 u32ElapsedTimeout;
      u32 u32Now = U32_UPCNTS;

      if (u32Now >= s_stSpiApp.u32PollTimeoutCnt) {
        u32ElapsedTimeout = u32Now - s_stSpiApp.u32PollTimeoutCnt;
      } else {
        u32ElapsedTimeout = (SW_TIMER - s_stSpiApp.u32PollTimeoutCnt) + u32Now;
      }

      if (u32ElapsedTimeout >= 50000000U) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiDiag.u16LastErrType = 6U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
        s_stSpiDiag.u16LastErrRxAddr = Spi_Block_Status_spi_addr;
        s_stSpiDiag.u16LastErrRxData = s_stSpiApp.u16PollState;
        s_stSpiDiag.u16LastErrExpAddr = Spi_Block_Status_spi_addr;
        s_stSpiDiag.u16LastErrExpData = SPI_BLOCK_STATUS_READY;
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
        s_stSpiDiag.u16ConsecutiveSuccess++;
        if (s_stSpiDiag.u16ConsecutiveSuccess > s_stSpiDiag.u16MaxConsecutiveSuccess) {
          s_stSpiDiag.u16MaxConsecutiveSuccess = s_stSpiDiag.u16ConsecutiveSuccess;
        }
        s_stSpiApp.eState = SPI_APP_STATE_IDLE;
      } else if ((s_stSpiApp.u16PollState == SPI_BLOCK_STATUS_ERROR) || 
                 (s_stSpiApp.u16PollState == SPI_BLOCK_STATUS_OVERFLOW)) {
        s_stSpiApp.u32StressFailCnt++;
        if (s_stSpiApp.eTestState == SPI_MASTER_TEST_RUNNING) {
          s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
          s_stSpiApp.u16LastResult = s_stSpiApp.u16PollState;
        }
        s_stSpiDiag.u16LastErrType = 3U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
        s_stSpiDiag.u16LastErrRxAddr = Spi_Block_Status_spi_addr;
        s_stSpiDiag.u16LastErrRxData = s_stSpiApp.u16PollState;
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
static void onManualReadComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(s_stSpiApp.u16TestAddr + calcSpiMasterChecksum(u16RxData));
  if (u16RxAddr == u16ExpectedAddr) {
    s_stSpiApp.u16TestData = u16RxData;
    s_stSpiApp.u32StressPassCnt++;
    s_stSpiDiag.u16ConsecutiveSuccess++;
    if (s_stSpiDiag.u16ConsecutiveSuccess > s_stSpiDiag.u16MaxConsecutiveSuccess) {
      s_stSpiDiag.u16MaxConsecutiveSuccess = s_stSpiDiag.u16ConsecutiveSuccess;
    }
    s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiDiag.u16LastErrType = 3U;
    s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
    s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
    s_stSpiDiag.u16LastErrRxData = u16RxData;
    s_stSpiDiag.u16LastErrExpAddr = u16ExpectedAddr;
    s_stSpiDiag.u16LastErrExpData = 0U;
    recoverSpiMaster(&s_stSpiMaster);
  }
}

static void onManualWriteComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(s_stSpiApp.u16TestAddr + calcSpiMasterChecksum(u16RxData));
  if ((u16RxAddr == u16ExpectedAddr) && (u16RxData == s_stSpiApp.u16TestData)) {
    s_stSpiApp.u32StressPassCnt++;
    s_stSpiDiag.u16ConsecutiveSuccess++;
    if (s_stSpiDiag.u16ConsecutiveSuccess > s_stSpiDiag.u16MaxConsecutiveSuccess) {
      s_stSpiDiag.u16MaxConsecutiveSuccess = s_stSpiDiag.u16ConsecutiveSuccess;
    }
    s_stSpiApp.eTestState = SPI_MASTER_TEST_DONE;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiApp.eState = SPI_APP_STATE_IDLE;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    s_stSpiApp.eTestState = SPI_MASTER_TEST_ERROR;
    s_stSpiApp.u16LastResult = u16RxData;
    s_stSpiDiag.u16LastErrType = 3U;
    s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
    s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
    s_stSpiDiag.u16LastErrRxData = u16RxData;
    s_stSpiDiag.u16LastErrExpAddr = u16ExpectedAddr;
    s_stSpiDiag.u16LastErrExpData = s_stSpiApp.u16TestData;
    recoverSpiMaster(&s_stSpiMaster);
  }
}

static void onBlockWriteComplete(void) {
  u16 u16Idx;
  u16 u16Len = s_stSpiMaster.stBlockTx.u16Length;
  
  if (s_stSpiMaster.stBlockTx.pstTxBuf != 0) {
    for (u16Idx = 0U; u16Idx < u16Len; u16Idx++) {
      u16 u16MapIdx = u16Idx;
      if (s_stSpiMaster.stBlockTx.bLoop == 1U) {
        u16MapIdx = (u16)(u16MapIdx & 1U);
      }
      
      u16 u16RxAddr = s_stSpiMaster.stBlockTx.pstRxBuf[u16MapIdx].stPack.u16Address;
      u16 u16RxData = s_stSpiMaster.stBlockTx.pstRxBuf[u16MapIdx].stPack.u16Data;
      u16 u16ExpAddr = (u16)(s_stSpiMaster.stBlockTx.pstTxBuf[u16MapIdx].stPack.u16Address + calcSpiMasterChecksum(u16RxData));
      
      u16 bDataOk = (u16RxData == s_stSpiMaster.stBlockTx.pstTxBuf[u16MapIdx].stPack.u16Data);
      if (s_stSpiMaster.stBlockTx.pstTxBuf[u16MapIdx].stPack.u16Address == Spi_Block_End_spi_addr) {
        bDataOk = (u16RxData == SPI_BLOCK_STATUS_BUSY) || (u16RxData == SPI_BLOCK_STATUS_READY);
      }
      
      if ((u16RxAddr != u16ExpAddr) || (!bDataOk)) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiDiag.u16LastErrType = 3U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
        s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
        s_stSpiDiag.u16LastErrRxData = u16RxData;
        s_stSpiDiag.u16LastErrExpAddr = u16ExpAddr;
        s_stSpiDiag.u16LastErrExpData = s_stSpiMaster.stBlockTx.pstTxBuf[u16MapIdx].stPack.u16Data;
        recoverSpiMaster(&s_stSpiMaster);
        return;
      }
    }
  }
  
  s_stSpiApp.u32PollTimer = U32_UPCNTS;
  s_stSpiApp.u32PollTimeoutCnt = U32_UPCNTS;
  s_stSpiApp.bPollPending = 0U;
  s_stSpiApp.u16PollState = SPI_BLOCK_STATUS_BUSY;
  s_stSpiApp.eState = SPI_APP_STATE_POLL_SLAVE;
}

static void onBlockReadComplete(void) {
  u16 u16Idx;
  u16 u16Len = s_stSpiMaster.stBlockTx.u16Length;
  
  for (u16Idx = 0U; u16Idx < u16Len; u16Idx++) {
    u16 u16MapIdx = u16Idx;
    if (s_stSpiMaster.stBlockTx.bLoop == 1U) {
      u16MapIdx = (u16)(u16MapIdx & 1U);
    }
    
    u16 u16RxAddr = s_astStressRxBuf[u16MapIdx].stPack.u16Address;
    u16 u16RxData = s_astStressRxBuf[u16MapIdx].stPack.u16Data;
    u16 u16ExpAddr = (u16)(s_astStressTxBuf[u16MapIdx].stPack.u16Address + calcSpiMasterChecksum(u16RxData));
    
    if (u16RxAddr != u16ExpAddr) {
      s_stSpiApp.u32StressFailCnt++;
      s_stSpiDiag.u16LastErrType = 3U;
      s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
      s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
      s_stSpiDiag.u16LastErrRxData = u16RxData;
      s_stSpiDiag.u16LastErrExpAddr = u16ExpAddr;
      s_stSpiDiag.u16LastErrExpData = 0U;
      recoverSpiMaster(&s_stSpiMaster);
      return;
    }
  }
  
  s_stSpiApp.u32StressPassCnt += u16Len;
  s_stSpiDiag.u16ConsecutiveSuccess += u16Len;
  if (s_stSpiDiag.u16ConsecutiveSuccess > s_stSpiDiag.u16MaxConsecutiveSuccess) {
    s_stSpiDiag.u16MaxConsecutiveSuccess = s_stSpiDiag.u16ConsecutiveSuccess;
  }
  s_stSpiApp.eState = SPI_APP_STATE_IDLE;
}

static void onBlockWrRdComplete(void) {
  u16 u16Idx;
  u16 u16Len = s_stSpiMaster.stBlockTx.u16Length;
  
  for (u16Idx = 0U; u16Idx < u16Len; u16Idx++) {
    u16 u16MapIdx = u16Idx;
    if (s_stSpiMaster.stBlockTx.bLoop == 1U) {
      u16MapIdx = (u16)(u16MapIdx & 1U);
    }
    
    u16 u16RxAddr = s_astStressRxBuf[u16MapIdx].stPack.u16Address;
    u16 u16RxData = s_astStressRxBuf[u16MapIdx].stPack.u16Data;
    u16 u16ExpAddr = (u16)(s_astStressTxBuf[u16MapIdx].stPack.u16Address + calcSpiMasterChecksum(u16RxData));
    
    if (u16RxAddr != u16ExpAddr) {
      s_stSpiApp.u32StressFailCnt++;
      s_stSpiDiag.u16LastErrType = 3U;
      s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
      s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
      s_stSpiDiag.u16LastErrRxData = u16RxData;
      s_stSpiDiag.u16LastErrExpAddr = u16ExpAddr;
      s_stSpiDiag.u16LastErrExpData = 0U;
      recoverSpiMaster(&s_stSpiMaster);
      return;
    }
    
    if (s_astStressTxBuf[u16MapIdx].stPack.u16Data != 0U) {
      if (u16RxData != s_astStressTxBuf[u16MapIdx].stPack.u16Data) {
        s_stSpiApp.u32StressFailCnt++;
        s_stSpiDiag.u16LastErrType = 3U;
        s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
        s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
        s_stSpiDiag.u16LastErrRxData = u16RxData;
        s_stSpiDiag.u16LastErrExpAddr = u16ExpAddr;
        s_stSpiDiag.u16LastErrExpData = s_astStressTxBuf[u16MapIdx].stPack.u16Data;
        recoverSpiMaster(&s_stSpiMaster);
        return;
      }
    }
  }
  
  s_stSpiApp.u32PollTimer = U32_UPCNTS;
  s_stSpiApp.u32PollTimeoutCnt = U32_UPCNTS;
  s_stSpiApp.bPollPending = 0U;
  s_stSpiApp.u16PollState = SPI_BLOCK_STATUS_BUSY;
  s_stSpiApp.eState = SPI_APP_STATE_POLL_SLAVE;
}

static void onPollSlaveComplete(u16 u16RxAddr, u16 u16RxData) {
  u16 u16ExpectedAddr = (u16)(Spi_Block_Status_spi_addr + calcSpiMasterChecksum(u16RxData));
  if (u16RxAddr == u16ExpectedAddr) {
    s_stSpiApp.u16PollState = u16RxData;
    s_stSpiApp.bPollPending = 0U;
  } else {
    s_stSpiApp.u32StressFailCnt++;
    s_stSpiDiag.u16LastErrType = 3U;
    s_stSpiDiag.u16LastErrStep = s_stSpiApp.eState;
    s_stSpiDiag.u16LastErrRxAddr = u16RxAddr;
    s_stSpiDiag.u16LastErrRxData = u16RxData;
    s_stSpiDiag.u16LastErrExpAddr = u16ExpectedAddr;
    s_stSpiDiag.u16LastErrExpData = 0U;
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

  f32Step = 6.28318530f / (f32)SPI_SINE_TABLE_SIZE;
  for (u16Idx = 0U; u16Idx < SPI_SINE_TABLE_SIZE; u16Idx++) {
    f32 f32Phase = (f32)u16Idx * f32Step;
    f32 f32Val = 2047.5f + 2047.5f * sinf(f32Phase);
    if (f32Val > 4095.0f) {
      f32Val = 4095.0f;
    }
    g_u16SpiMasterWaveRam[u16Idx] = (u16)f32Val;
  }

  s_u16SineTableReady = 1U;
}

void spiMasterTask(ST_SPI_MASTER *pstInst) {
  runSpiMasterCommunication(pstInst);
  runSpiMasterApp(pstInst);
}

// ============================================================================
// Diagnostics & Self-Test Implementation
// ============================================================================

void triggerSpiMasterSelfTest(ST_SPI_MASTER *pstInst) {
  u32 u32Base = pstInst->u32SpiBaseAddr;
  u32 u32Timeout = 0U;
  u16 u16Addr, u16Data;

  s_stSpiDiag.u16SelfTestStatus = 1U; // Running

  SPI_enableLoopback(u32Base);
  SPI_resetTxFIFO(u32Base);
  SPI_resetRxFIFO(u32Base);

  // Send a deterministic test frame: Address 0x0400U, Data 0x5A5AU
  SPI_writeDataNonBlocking(u32Base, 0x0400U);
  SPI_writeDataNonBlocking(u32Base, 0x5A5AU);

  // Wait using a bounded loop to avoid blocking (max 5000 cycles)
  while ((SPI_getRxFIFOStatus(u32Base) < 2U) && (u32Timeout < 5000U)) {
    u32Timeout++;
  }

  if (SPI_getRxFIFOStatus(u32Base) >= 2U) {
    u16Addr = SPI_readDataNonBlocking(u32Base);
    u16Data = SPI_readDataNonBlocking(u32Base);

    if ((u16Addr == 0x0400U) && (u16Data == 0x5A5AU)) {
      s_stSpiDiag.u16SelfTestStatus = 2U; // Passed
    } else {
      s_stSpiDiag.u16SelfTestStatus = 3U; // Failed
      s_stSpiDiag.u16SelfTestErrorCount++;
    }
  } else {
    s_stSpiDiag.u16SelfTestStatus = 3U; // Failed due to loopback timeout
    s_stSpiDiag.u16SelfTestErrorCount++;
  }

  SPI_disableLoopback(u32Base);
  SPI_resetTxFIFO(u32Base);
  SPI_resetRxFIFO(u32Base);
}

u16 getSpiMasterSelfTestResult(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  return s_stSpiDiag.u16SelfTestStatus;
}

void resetSpiMasterMetrics(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  s_stSpiDiag.u32TotalTxCount = 0U;
  s_stSpiDiag.u32TotalRxCount = 0U;
  s_stSpiDiag.u16LastErrType = 0U;
  s_stSpiDiag.u16LastErrStep = 0U;
  s_stSpiDiag.u16LastErrRxAddr = 0U;
  s_stSpiDiag.u16LastErrRxData = 0U;
  s_stSpiDiag.u16LastErrExpAddr = 0U;
  s_stSpiDiag.u16LastErrExpData = 0U;
  s_stSpiDiag.u16ChecksumErrorCount = 0U;
  s_stSpiDiag.u16ConsecutiveSuccess = 0U;
  s_stSpiDiag.u16MaxConsecutiveSuccess = 0U;
  s_stSpiDiag.u16SelfTestStatus = 0U;
  s_stSpiDiag.u16SelfTestErrorCount = 0U;
}

bool checkSpiMasterHealth(ST_SPI_MASTER *pstInst) {
  (void)pstInst;
  // If there are too many errors, or self-test failed, report unhealthy
  if (s_stSpiDiag.u16SelfTestStatus == 3U) {
    return false;
  }
  if (s_stSpiDiag.u16ChecksumErrorCount > 10U) {
    return false;
  }
  return true;
}
