/*
 * spi_b_slave.c (Preview Sandbox)
 *
 * Refactored from test_spi_slave.c
 * Migrated to CPU1 for Host communication (SPI-B)
 * ASCII-ONLY formatted for MS950 compilation safety.
 *
 * M3 DMA path: DMA CH3 receives exactly 2 words (one legacy register frame)
 * per transfer.  DMA done is polled in pollReceiveFromSpi(); on each done the
 * active buffer alternates through the ping-pong manager and DMA is restarted
 * before parsing begins.
 */

#include "driverlib.h"
#include "device.h"
#include "inc/hw_spi.h"
#include "spi_slave.h"
#include "spi_fifo.h"
#include "spi_pingpong.h"
#include "common.h"
#include "shareram.h"

volatile uint16_t OUTPUT_ON;

/* -----------------------------------------------------------------------
 * DMA ping-pong abstraction (replaces the six raw globals from M3 baseline).
 * ----------------------------------------------------------------------- */
#pragma DATA_SECTION(s_rxPingPong, "spib_slave_state")
#pragma DATA_ALIGN(s_rxPingPong, 2)
static SpibDmaPingPong_t s_rxPingPong;

#pragma DATA_SECTION(s_fallbackFifo, "spib_slave_state")
static SPI_FIFO_t s_fallbackFifo;

/* Last received cmd/data words, kept for error reporting. */
static uint16_t s_u16LastRxCmd  = 0U;
static uint16_t s_u16LastRxData = 0U;

#pragma DATA_SECTION(gSpibRxDmaDoneCount, "spib_slave_state")
volatile uint32_t gSpibRxDmaDoneCount;
#pragma DATA_SECTION(gSpibRxParseOkCount, "spib_slave_state")
volatile uint32_t gSpibRxParseOkCount;
#pragma DATA_SECTION(gSpibRxParseFailCount, "spib_slave_state")
volatile uint32_t gSpibRxParseFailCount;
#pragma DATA_SECTION(gSpibRxDmaRestartCount, "spib_slave_state")
volatile uint32_t gSpibRxDmaRestartCount;
#pragma DATA_SECTION(gSpibRxErrorFlags, "spib_slave_state")
volatile uint16_t gSpibRxErrorFlags;

#define SPIB_WAVE_FORCE_TRIGGER_LIMIT   16U

/* Post-wave command capture: CH3 is re-armed onto this buffer while the
 * received wave block is being parsed, so control frames arriving during
 * the parse are not lost. */
#define SPIB_POST_WAVE_CMD_FRAMES       32U
#define SPIB_POST_WAVE_CMD_WORDS        (SPIB_POST_WAVE_CMD_FRAMES * 2U)

/* Wave block parse is sliced across polls so the background loop latency
 * stays bounded (4096 frames / 512 = 8 polls). */
#define SPIB_WAVE_PARSE_CHUNK_FRAMES    512U

#if WAVE_BURST_SAMPLE_COUNT != WAVE_SAMPLES_PER_PAGE
#error "Wave burst sample count must match the wave page size"
#endif

#pragma DATA_SECTION(g_u32WaveRawParseCount, "spib_slave_state")
volatile uint32_t g_u32WaveRawParseCount;

/* Wave block landing zone.  The block DMA captures WAVE_BURST_SAMPLE_COUNT
 * frames = 2 * 4096 = 8192 words starting at g_u16WaveRawRxBuffer, i.e. it
 * spans BOTH arrays below.  g_u16WaveRawRxBufferHi is never referenced by
 * name but claims RAMGS11 (which physically follows RAMGS10) so the linker
 * cannot place anything else in the DMA's path.  DO NOT REMOVE either array
 * or re-map their sections; initSPIslave() verifies the adjacency at boot. */
#pragma DATA_SECTION(g_u16WaveRawRxBuffer, "spib_rx_wave_raw_1")
volatile uint16_t g_u16WaveRawRxBuffer[4096];
#pragma DATA_SECTION(g_u16WaveRawRxBufferHi, "spib_rx_wave_raw_2")
volatile uint16_t g_u16WaveRawRxBufferHi[4096];

#pragma DATA_SECTION(g_u16PostWaveCmdBuffer, "spib_slave_state")
volatile uint16_t g_u16PostWaveCmdBuffer[SPIB_POST_WAVE_CMD_WORDS];

#pragma DATA_SECTION(g_u32PostWaveCmdWordsReceived, "spib_slave_state")
volatile uint32_t g_u32PostWaveCmdWordsReceived;
#pragma DATA_SECTION(g_u32PostWaveCmdFramesParsed, "spib_slave_state")
volatile uint32_t g_u32PostWaveCmdFramesParsed;
#pragma DATA_SECTION(g_u32PostWaveCmdOverflowCount, "spib_slave_state")
volatile uint32_t g_u32PostWaveCmdOverflowCount;
#pragma DATA_SECTION(g_u32PostWaveCmdPartialFrameCount, "spib_slave_state")
volatile uint32_t g_u32PostWaveCmdPartialFrameCount;
#pragma DATA_SECTION(g_u32PostWaveDstBeg, "spib_slave_state")
volatile uint32_t g_u32PostWaveDstBeg;
#pragma DATA_SECTION(g_u32PostWaveDstAct, "spib_slave_state")
volatile uint32_t g_u32PostWaveDstAct;

#pragma DATA_SECTION(g_u16WaveRawParsePending, "spib_slave_state")
volatile uint16_t g_u16WaveRawParsePending = 0U;

#pragma DATA_SECTION(s_bSpibRxInWaveMode, "spib_slave_state")
static bool s_bSpibRxInWaveMode = false;
/* Sliced wave block parse progress (g_u16WaveRawParsePending != 0). */
static uint16_t s_u16WaveParseIndex = 0U;
static uint16_t s_u16WaveParseTotal = 0U;
static bool     s_bWaveParseFailed = false;
/* Wave-block stall watchdog: DMA TRANSFER_COUNT progress tracking so an
 * aborted burst drains the partial block instead of locking the RX path. */
static uint16_t s_u16WaveLastXferCount = 0xFFFFU;
static uint32_t s_u32WaveStallTicks = 0U;
/* Previous RX cmd word, used only by the legacy overflow fallback. */
static uint16_t s_u16PrevRxCmd = 0xFFFFU;
/* WAVE_BURST_BEGIN preamble received: pre-arm the block transport on
 * this poll, before the announced stream arrives (zero-loss entry). */
static bool     s_bWaveBurstBegin = false;
static uint16_t s_u16WaveBurstFrames = 0U;
/* Block-wave MISO is intentionally ignored by the master.  Suppress the
 * per-sample ACKs while parsing DMA blocks so they cannot remain queued in
 * the SPI TX FIFO and corrupt the first control response after the burst. */
static bool     s_bWaveSuppressResponse = false;

/* B016: fault report registers + fault clear, per-burst parse-fail flag,
 * sliced wave parse, wave buffer Hi-half restored with boot-time guard.
 * B017: post-wave replay suppresses SPI responses (no TX-FIFO pollution) +
 * RX/TX FIFO flush on burst->RegFrame restore (FinishPostWave / DrainAndExit).
 * B01E: wave completion is derived from the received burst; 0x095A exposes
 * REG_READY/RX_DONE/ERROR for master-side status polling. */
#pragma DATA_SECTION(gSpibFwBuildTag, "spib_slave_state")
volatile uint16_t gSpibFwBuildTag = 0xB01EU;

/* ---- DIAGNOSTIC: slave-side post-burst timing (B01D) --------------------
 * Ticks (U32_UPCNTS, 50 MHz, wraps at SW_TIMER) captured through the
 * FinishPostWave recovery so the master/slave seam can be measured against
 * the master-side ticks. */
#pragma DATA_SECTION(g_u32DiagSlaveFinishEnterTick, "spib_slave_state")
volatile uint32_t g_u32DiagSlaveFinishEnterTick;
#pragma DATA_SECTION(g_u32DiagSlaveRecoverStartTick, "spib_slave_state")
volatile uint32_t g_u32DiagSlaveRecoverStartTick;
#pragma DATA_SECTION(g_u32DiagSlaveRecoverEndTick, "spib_slave_state")
volatile uint32_t g_u32DiagSlaveRecoverEndTick;
#pragma DATA_SECTION(g_u32DiagSlaveRegDmaArmedTick, "spib_slave_state")
volatile uint32_t g_u32DiagSlaveRegDmaArmedTick;
#pragma DATA_SECTION(g_u32DiagSlavePendingClearTick, "spib_slave_state")
volatile uint32_t g_u32DiagSlavePendingClearTick;
volatile uint32_t g_u32DebugLastTx;

/* ---- DIAGNOSTIC: RX/TX frame ring buffers (B018) ------------------------
 * Capture the last SPIB_DIAG_LOG_DEPTH frames the normal (non-wave) RX path
 * latched (INCLUDING idle 0xFFFF frames, which g_u32SpiSlaveLastRequest
 * hides) and the responses written via writeDirectSpiResponse.  After a
 * failed run, read these to see the exact word alignment around the post-
 * burst 0x0959 transaction.  *Idx points to the NEXT slot; newest entry is
 * (*Idx - 1) & (DEPTH-1), going backwards in time.  DEPTH must stay 2^n. */
#define SPIB_DIAG_LOG_DEPTH 16U
#pragma DATA_SECTION(g_u16SpibRxLogCmd, "spib_slave_state")
volatile uint16_t g_u16SpibRxLogCmd[SPIB_DIAG_LOG_DEPTH];
#pragma DATA_SECTION(g_u16SpibRxLogData, "spib_slave_state")
volatile uint16_t g_u16SpibRxLogData[SPIB_DIAG_LOG_DEPTH];
#pragma DATA_SECTION(g_u16SpibRxLogIdx, "spib_slave_state")
volatile uint16_t g_u16SpibRxLogIdx;
#pragma DATA_SECTION(g_u16SpibTxLogAddr, "spib_slave_state")
volatile uint16_t g_u16SpibTxLogAddr[SPIB_DIAG_LOG_DEPTH];
#pragma DATA_SECTION(g_u16SpibTxLogData, "spib_slave_state")
volatile uint16_t g_u16SpibTxLogData[SPIB_DIAG_LOG_DEPTH];
#pragma DATA_SECTION(g_u16SpibTxLogIdx, "spib_slave_state")
volatile uint16_t g_u16SpibTxLogIdx;

/* ---- DIAGNOSTIC: RX/DMA steady-state snapshot (B019) --------------------
 * Sampled at the top of pollReceiveFromSpi() every call.  After a failed run
 * (master has stopped clocking), these show whether the post-burst 0x0959 is
 * stuck in the RX FIFO (DMA trigger dead), the DMA run/armed state, and
 * whether the slave wrongly thinks it is still in wave/parse mode.
 *   g_u16DiagRxFifoLvl : SPI_getRxFIFOStatus (words waiting in RX FIFO)
 *   g_u16DiagDmaRun    : DMA channel RUNSTS (1 = armed/running, 0 = done)
 *   g_u16DiagDmaArmed  : s_bSpibRxDmaArmed
 *   g_u16DiagInWave    : s_bSpibRxInWaveMode
 *   g_u16DiagPending   : g_u16WaveRawParsePending
 *   g_u32DiagPollCount : pollReceiveFromSpi() call counter (proves it runs)
 *   g_u32DiagNormalFrames : times the normal RegFrame branch processed a frame */
#pragma DATA_SECTION(g_u16DiagRxFifoLvl, "spib_slave_state")
volatile uint16_t g_u16DiagRxFifoLvl;
#pragma DATA_SECTION(g_u16DiagDmaRun, "spib_slave_state")
volatile uint16_t g_u16DiagDmaRun;
#pragma DATA_SECTION(g_u16DiagDmaArmed, "spib_slave_state")
volatile uint16_t g_u16DiagDmaArmed;
#pragma DATA_SECTION(g_u16DiagInWave, "spib_slave_state")
volatile uint16_t g_u16DiagInWave;
#pragma DATA_SECTION(g_u16DiagPending, "spib_slave_state")
volatile uint16_t g_u16DiagPending;
#pragma DATA_SECTION(g_u32DiagPollCount, "spib_slave_state")
volatile uint32_t g_u32DiagPollCount;
#pragma DATA_SECTION(g_u32DiagNormalFrames, "spib_slave_state")
volatile uint32_t g_u32DiagNormalFrames;
/* RX FIFO word count seen at the post-burst RegFrame restore (B01A). >=2 means
 * the 0x0959 was resident and edge-trigger recovery had to force-capture it. */
#pragma DATA_SECTION(g_u16DiagFifoAtRestore, "spib_slave_state")
volatile uint16_t g_u16DiagFifoAtRestore;
/* Number of times the edge-trigger watchdog force-kicked a stuck resident
 * frame (B01A).  Non-zero proves the edge-trap was the failure mechanism. */
#pragma DATA_SECTION(g_u32DiagForceKickCount, "spib_slave_state")
volatile uint32_t g_u32DiagForceKickCount;

/* ---- DIAGNOSTIC: SPIB SPI peripheral register snapshot (B01B) -----------
 * The slave RX FIFO stays empty after the burst even though the DMA is armed
 * and the master is clocking -- so the SPI peripheral RX itself is refusing
 * data.  These raw registers reveal why:
 *   g_u32DiagSpiIntStatus : SPI_getInterruptStatus (RX_OVERRUN / RXFF_OVERFLOW)
 *   g_u16DiagSpiFfrx      : SPIFFRX  (bit15 RXFFOVF, bit13 RXFIFORESET,
 *                                     bits12:8 RXFFST, bit7 RXFFINT)
 *   g_u16DiagSpiCcr       : SPICCR   (bit7 SPISWRESET: 1=operational, 0=reset) */
#pragma DATA_SECTION(g_u32DiagSpiIntStatus, "spib_slave_state")
volatile uint32_t g_u32DiagSpiIntStatus;
#pragma DATA_SECTION(g_u16DiagSpiFfrx, "spib_slave_state")
volatile uint16_t g_u16DiagSpiFfrx;
#pragma DATA_SECTION(g_u16DiagSpiCcr, "spib_slave_state")
volatile uint16_t g_u16DiagSpiCcr;
/* SPI peripheral state frozen at the FIRST 2ms watchdog reset (B01C) = the
 * failure state before the reset heals it.  If the proactive recovery in
 * SPIB_RxWaveFinishPostWave works, the watchdog never fires and these stay 0. */
#pragma DATA_SECTION(g_u16DiagFfrxAtReset, "spib_slave_state")
volatile uint16_t g_u16DiagFfrxAtReset;
#pragma DATA_SECTION(g_u16DiagCcrAtReset, "spib_slave_state")
volatile uint16_t g_u16DiagCcrAtReset;
#pragma DATA_SECTION(g_u16DiagRxLvlAtReset, "spib_slave_state")
volatile uint16_t g_u16DiagRxLvlAtReset;
#pragma DATA_SECTION(g_u32DiagIntStatusAtReset, "spib_slave_state")
volatile uint32_t g_u32DiagIntStatusAtReset;
volatile uint32_t g_u32SpiSlaveLastRequest;

static void SPIB_RxRestartRegFrameDma(void);
static void SPIB_RxDma_Configure(uint16_t *pDst, uint16_t burstSize, uint16_t transferSize);
static void SPIB_RxDma_ConfigureRegFrame(uint16_t *pDst);
static void SPIB_RxDma_Start(void);
static void SPIB_RxDma_Stop(void);
static void SPIB_RxDma_ClearFlags(void);

#pragma DATA_SECTION(g_u16SpiBlockRam, "spib_block_ram")
volatile uint16_t g_u16SpiBlockRam[SIZE_OF_SPI_BLOCK_RAM];

#define SPI_BLOCK_STATUS_IDLE      0x0000U
#define SPI_BLOCK_STATUS_RECEIVING 0x0001U
#define SPI_BLOCK_STATUS_BUSY      0x0002U
#define SPI_BLOCK_STATUS_READY     0x0003U
#define SPI_BLOCK_STATUS_OVERFLOW  0x8000U
#define SPI_BLOCK_STATUS_ERROR     0x8001U

#define SPIB_RX_DMA_CH_BASE        DMA_CH3_BASE
#define SPIB_TX_DMA_CH_BASE        DMA_CH4_BASE
#define SPIB_RX_DMA_TIMEOUT_TICKS  T_2MS

/* ---- D02_2_1 5.2 TX path: TxPing/Pong -> DMA CH4 -> SPIB TX FIFO --------
 * The background loop keeps the idle buffer filled with the latest status
 * packet (SPIB_TxStatusRefresh).  On a master request the freshest buffer
 * is bound to CH4, which then feeds the TX FIFO as the master clocks idle
 * frames -- zero CPU involvement during the transfer. */
#pragma DATA_SECTION(s_u16TxStatusPing, "spib_slave_state")
static uint16_t s_u16TxStatusPing[SPIB_STATUS_PACKET_WORDS];
#pragma DATA_SECTION(s_u16TxStatusPong, "spib_slave_state")
static uint16_t s_u16TxStatusPong[SPIB_STATUS_PACKET_WORDS];
static uint16_t *s_pTxStatusFill = s_u16TxStatusPing;  /* CPU fills this   */
static uint16_t *s_pTxStatusDma  = s_u16TxStatusPong;  /* CH4 reads this   */
static bool     s_bTxStatusActive = false;
static uint32_t s_u32TxStatusArmTicks = 0U;
static uint16_t s_u16TxStatusSeq = 0U;

#pragma DATA_SECTION(gSpibTxStatusPacketCount, "spib_slave_state")
volatile uint32_t gSpibTxStatusPacketCount;
#pragma DATA_SECTION(gSpibTxStatusAbortCount, "spib_slave_state")
volatile uint32_t gSpibTxStatusAbortCount;
#pragma DATA_SECTION(gSpibTxSuppressedRespCount, "spib_slave_state")
volatile uint32_t gSpibTxSuppressedRespCount;

#pragma DATA_SECTION(spiB_slave, "spib_slave_state")
ST_SPI_SLAVE spiB_slave = {
    .fsm  = _INIT_SPI_AS_SLAVE,
    .stat = _NO_STAT_OF_SSS,
};


#define SW_SSS(x)         FG_SWTO(x, spiB_slave.fsm)
#define SET_SSS_STAT(x)   FG_SET(x, spiB_slave.stat)

#ifdef _FLASH
#pragma SET_CODE_SECTION(".TI.ramfunc")
#endif //_FLASH

static ST_WR_PARSER s_sSpiParser;
static bool s_bSpibRxDmaArmed = false;
static bool s_bSpibRxPartialPending = false;
static uint32_t s_u32SpibRxPartialTicks = 0U;

/* Edge-trigger watchdog debounce: consecutive polls a full frame has sat
 * resident in the RX FIFO with the DMA armed but not done.  The normal
 * mid-capture transient clears in < 1 poll; the edge-trap is permanent. */
#define SPIB_RX_STUCK_KICK_POLLS 8U
static uint16_t s_u16RxStuckPolls = 0U;
static uint32_t s_u32LastRxTicks = 0U;
static bool s_bSpiClean = false;

static uint32_t getElapsedTicks(uint32_t u32StartTicks)
{
    uint32_t u32Now = U32_UPCNTS;

    if (u32Now >= u32StartTicks)
    {
        return (u32Now - u32StartTicks);
    }

    return ((SW_TIMER - u32StartTicks) + u32Now);
}

// ============================================================================
// Central Slave Error Reporter
// ============================================================================
static void reportSlaveError(SPIB_FAULT_SOURCE_e eSource, uint16_t u16FaultVal)
{
    if (spiB_slave.stDiag.eHealth == SPIB_HEALTH_OK)
    {
        spiB_slave.stDiag.eHealth = SPIB_HEALTH_FAULT;
        spiB_slave.stDiag.eFaultSource = eSource;
        spiB_slave.stDiag.u16FaultCode = u16FaultVal;

        uint16_t step = 0U;
        uint16_t d0 = s_u16LastRxCmd;
        uint16_t d1 = s_u16LastRxData;

        if (eSource == SPIB_FAULT_SOURCE_DRIVER)
        {
            spiB_slave.stDiag.stDriver.eFault = (SPIB_DRIVER_FAULT_e)u16FaultVal;
            step = (uint16_t)spiB_slave.fsm;
            CommDiag_ReportError((ST_COMM_DIAG *)&spiB_slave.stDiag.stDriver.stComm, u16FaultVal, step, U32_UPCNTS, d0, d1, 0U, 0U);
        }
        else if (eSource == SPIB_FAULT_SOURCE_PROTOCOL)
        {
            spiB_slave.stDiag.stProtocol.eFault = (SPIB_PROTOCOL_FAULT_e)u16FaultVal;
            step = (uint16_t)spiB_slave.ePacketState;
            if (u16FaultVal == (uint16_t)SPIB_PROT_FAULT_BAD_LENGTH || u16FaultVal == (uint16_t)SPIB_PROT_FAULT_CHECKSUM)
            {
                d0 = spiB_slave.u16PacketCmd;
                d1 = spiB_slave.u16PacketLength;
            }
            CommDiag_ReportError((ST_COMM_DIAG *)&spiB_slave.stDiag.stProtocol.stComm, u16FaultVal, step, U32_UPCNTS, d0, d1, 0U, 0U);
        }
        else if (eSource == SPIB_FAULT_SOURCE_SERVICE)
        {
            spiB_slave.stDiag.stService.eFault = (SPIB_SERVICE_FAULT_e)u16FaultVal;
            step = spiB_slave.u16BlockWriteIndex;
            CommDiag_ReportError((ST_COMM_DIAG *)&spiB_slave.stDiag.stService.stComm, u16FaultVal, step, U32_UPCNTS, d0, d1, 0U, 0U);
        }
    }
}

/* Master-commanded fault recovery (C2000_Fault_Clear_spi_addr).  Re-opens
 * the reportSlaveError() first-fault latch and clears latched error state
 * so the next fault is recorded again. */
static void clearSlaveFaults(void)
{
    spiB_slave.stDiag.eHealth = SPIB_HEALTH_OK;
    spiB_slave.stDiag.eFaultSource = SPIB_FAULT_SOURCE_NONE;
    spiB_slave.stDiag.u16FaultCode = 0U;
    spiB_slave.stDiag.stDriver.eFault = SPIB_DRV_FAULT_NONE;
    spiB_slave.stDiag.stProtocol.eFault = SPIB_PROT_FAULT_NONE;
    spiB_slave.stDiag.stService.eFault = SPIB_SERV_FAULT_NONE;
    gSpibRxErrorFlags = 0U;
    spiB_slave.u16PacketErrorCode = SPIB_PACKET_ERROR_NONE;
    spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_NONE;
    spiB_slave.stat &= ~_SSS_GET_ERROR;
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stDriver.stComm);
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stProtocol.stComm);
    CommDiag_ClearLatch((ST_COMM_DIAG *)&spiB_slave.stDiag.stService.stComm);
}

// ============================================================================
// Private Helper Initialization & Physical Layer Interface
// ============================================================================

void initNativeSpiSlave(uint32_t u32Base)
{
    SPI_disableModule(u32Base);
    SPI_enableFIFO(u32Base);
    SPI_setFIFOInterruptLevel(u32Base, SPI_FIFO_TX2, SPI_FIFO_RX2);
    SPI_clearInterruptStatus(u32Base, SPI_INT_RXFF | SPI_INT_TXFF);
    SPI_enableModule(u32Base);
    SPI_resetTxFIFO(u32Base);
    SPI_resetRxFIFO(u32Base);
}

static void initSpibRxDma(void)
{
    SpibPingPong_Init(&s_rxPingPong);
    uint16_t *pDstAddr = (uint16_t *)SpibPingPong_GetDmaDst(&s_rxPingPong);

    s_bSpibRxDmaArmed = false;
    s_bSpibRxPartialPending = false;

    s_bSpibRxInWaveMode = false;
    s_u16PrevRxCmd = 0xFFFFU;
    s_bWaveBurstBegin = false;
    s_u16WaveBurstFrames = 0U;
    s_bWaveSuppressResponse = false;
    s_u16WaveParseIndex = 0U;
    s_u16WaveParseTotal = 0U;
    s_bWaveParseFailed = false;
    g_u16WaveRawParsePending = 0U;

    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_disableInterrupt(SPIB_RX_DMA_CH_BASE);
    SPIB_RxDma_Stop();
    SPIB_RxDma_ClearFlags();

    /* M3: burst=2 (one SPI frame per trigger), transfer=1 burst.
     * DMA done after every single 2-word frame. */
    SPIB_RxDma_ConfigureRegFrame(pDstAddr);

    DMA_configMode(SPIB_RX_DMA_CH_BASE, DMA_TRIGGER_SPIBRX,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);
    DMA_setEmulationMode(DMA_EMULATION_FREE_RUN);
 
    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
    SPI_enableInterrupt(SPIB_SYSTEM_BASE, SPI_INT_RXFF);
 
    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    SPIB_RxDma_Start();
    s_bSpibRxDmaArmed = true;
}

bool SPIB_RxDmaIsDone(void)
{
    if (s_bSpibRxDmaArmed == false)
    {
        return false;
    }
    return (DMA_getRunStatusFlag(SPIB_RX_DMA_CH_BASE) == false);
}

void SPIB_RxDmaClearDone(void)
{
    SPIB_RxDma_ClearFlags();
    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
}

static void SPIB_RxDma_Configure(uint16_t *pDst, uint16_t burstSize, uint16_t transferSize)
{
    uint16_t *pSrcAddr = (uint16_t *)(SPIB_SYSTEM_BASE + SPI_O_RXBUF);
    int16_t transferStep = (transferSize > 1U) ? 1 : 0;
    DMA_configAddresses(SPIB_RX_DMA_CH_BASE, pDst, pSrcAddr);
    DMA_configBurst(SPIB_RX_DMA_CH_BASE, burstSize, 0, 1);
    DMA_configTransfer(SPIB_RX_DMA_CH_BASE, transferSize, 0, transferStep);
}

static void SPIB_RxDma_ConfigureRegFrame(uint16_t *pDst)
{
    SPIB_RxDma_Configure(pDst, SPIB_RX_REG_WORDS, 1U);
}

static void SPIB_RxDma_Start(void)
{
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);
}

static void SPIB_RxDma_Stop(void)
{
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
}

static void SPIB_RxDma_ClearFlags(void)
{
    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);
}

static void SPIB_RxRestartRegFrameDma(void)
{
    uint16_t *pDstAddr = (uint16_t *)SpibPingPong_GetDmaDst(&s_rxPingPong);

    gSpibRxDmaRestartCount++;

    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);

    SPIB_RxDma_ConfigureRegFrame(pDstAddr);

    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);

    s_bSpibRxDmaArmed = true;
    s_bSpibRxPartialPending = false;

    /* Edge-trigger recovery: the RX DMA request is an EDGE on RXFFST crossing
     * RXFFIL (see SPIB_RxWaveEnterBlockMode).  If a full frame is ALREADY
     * resident in the RX FIFO at re-arm time (e.g. the post-burst 0x0959 that
     * landed between the previous capture and this restart, or any back-to-back
     * frame), the level is already >= RX2 so no new edge ever occurs and the
     * DMA would never trigger -- the RX path goes silent until the next frame.
     * Force one trigger to capture the resident frame.  When the FIFO is empty
     * (the normal spaced case) this is a no-op. */
    if (SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE) >= SPI_FIFO_RX2)
    {
        DMA_forceTrigger(SPIB_RX_DMA_CH_BASE);
    }
}

void SPIB_RxDmaRestart(void)
{
    SPIB_RxRestartRegFrameDma();
}

/* ---- Wave block transport stall recovery (M5R Phase2) -------------------
 * If the master aborts mid-block, the block DMA never completes and the
 * RX path would lock up.  Drain the frames that did arrive and restore
 * the normal 2-word RegFrame path.
 * ------------------------------------------------------------------------ */
/* Arm the 2KB block DMA for the next wave block.
 * s_u16WaveFramesRemaining must be set by the caller. */
/* Returns true when every frame in the range was a wave sample or an idle
 * (0xFFFF) frame.  The per-burst result must come from this return value,
 * not from gSpibRxErrorFlags: that flag accumulates for the whole session,
 * and a stale historical bit would otherwise veto every later ACTIVATE. */
static bool SPIB_ParseWaveRawBuffer(const uint16_t *pRaw, uint16_t frameCount)
{
    uint16_t i;
    bool bAllOk = true;

    for (i = 0U; i < frameCount; i++)
    {
        uint16_t u16Cmd  = pRaw[i * 2U];
        uint16_t u16Data = pRaw[i * 2U + 1U];

        s_u16LastRxCmd  = u16Cmd;
        s_u16LastRxData = u16Data;

        if ((u16Cmd >= WAVE_DATA_WINDOW_BASE) && (u16Cmd <= WAVE_DATA_WINDOW_LIMIT))
        {
            WaveDownload_WriteSample((uint16_t)(u16Cmd - WAVE_DATA_WINDOW_BASE), u16Data);
        }
        else if (u16Cmd != 0xFFFFU)
        {
            gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL;
            bAllOk = false;
        }
    }

    return bAllOk;
}

static void SPIB_RxWaveEnterBlockMode(void)
{
    uint16_t u16NextBlock = s_u16WaveBurstFrames;
    uint16_t *pDst = (uint16_t *)g_u16WaveRawRxBuffer;

    s_bSpibRxInWaveMode = true;
    gSpibRxDmaRestartCount += u16NextBlock;

    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);

    SPIB_RxDma_Configure(pDst, 2U, u16NextBlock);

    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);
    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);

    /* The RX DMA request is edge-triggered by RXFFST crossing RXFFIL.  At a
     * block boundary the FIFO can remain above the watermark, so software
     * triggers consume the backlog until the condition becomes false.
     * Subsequent wire traffic can then generate a new hardware edge. */
    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE,
                             SPI_INT_RXFF | SPI_INT_RX_OVERRUN);

    {
        uint16_t u16Kick;
        for (u16Kick = 0U;
             u16Kick < SPIB_WAVE_FORCE_TRIGGER_LIMIT;
             u16Kick++)
        {
            if (SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE) < SPI_FIFO_RX2)
            {
                break;
            }
            DMA_forceTrigger(SPIB_RX_DMA_CH_BASE);
            /* let the forced burst move 2 words before re-checking */
            __asm(" RPT #40 || NOP");
        }
    }

    s_bSpibRxDmaArmed = true;
    s_bSpibRxPartialPending = false;
    s_u16WaveLastXferCount = 0xFFFFU;
    s_u32WaveStallTicks = U32_UPCNTS;
}

static void SPIB_RxWaveDrainAndExit(uint16_t u16FramesReceived)
{
    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
    SPIB_RxDmaClearDone();

    (void)SPIB_ParseWaveRawBuffer((const uint16_t *)g_u16WaveRawRxBuffer, u16FramesReceived);
    g_u32WaveRawParseCount++;
    gSpibRxDmaDoneCount += u16FramesReceived;
    gSpibRxParseOkCount += u16FramesReceived; /* Keep metrics happy */

    s_bSpibRxInWaveMode = false;
    (void)WaveDownload_FinalizeBurst(false);

    /* Restore RegFrame mode.  Any frame already resident in the RX FIFO is
     * captured by SPIB_RxRestartRegFrameDma's edge-trigger recovery (do NOT
     * flush here -- flushing would discard a frame the master already sent). */
    SPIB_RxRestartRegFrameDma();
    WaveDownload_SetRegReady();
}

/* Final stage of a completed wave burst: account for the control frames
 * captured into g_u16PostWaveCmdBuffer during the sliced parse, replay
 * them through the normal parser, then restore the RegFrame DMA path. */
static void SPIB_RxWaveFinishPostWave(void)
{
    uint32_t u32WordsReceived = 0U;
    uint32_t u32FramesReceived;
    uint32_t i;
    bool bTransportOk = true;

    g_u32DiagSlaveFinishEnterTick = U32_UPCNTS;   /* B01D */

    if (s_bWaveParseFailed)
    {
        reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL,
                         (uint16_t)SPIB_PROT_FAULT_FRAME_PARSE_FAIL);
    }

    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
    g_u32PostWaveDstAct = HWREG(SPIB_RX_DMA_CH_BASE + DMA_O_DST_ADDR_ACTIVE);

    if (g_u32PostWaveDstAct >= g_u32PostWaveDstBeg)
    {
        u32WordsReceived = g_u32PostWaveDstAct - g_u32PostWaveDstBeg;
    }

    g_u32PostWaveCmdWordsReceived = u32WordsReceived;

    if (u32WordsReceived > SPIB_POST_WAVE_CMD_WORDS)
    {
        u32WordsReceived = SPIB_POST_WAVE_CMD_WORDS;
        g_u32PostWaveCmdOverflowCount++;
        gSpibRxErrorFlags |= SPIB_RX_ERR_POST_WAVE_OVERFLOW;
        bTransportOk = false;
    }

    if ((u32WordsReceived % 2U) != 0U)
    {
        g_u32PostWaveCmdPartialFrameCount++;
        gSpibRxErrorFlags |= SPIB_RX_ERR_POST_WAVE_PARTIAL_FRAME;
        bTransportOk = false;
    }

    u32FramesReceived = u32WordsReceived / 2U;
    g_u32PostWaveCmdFramesParsed += u32FramesReceived;

    /* These frames are REPLAYED after the fact: the master already clocked
     * them during the burst and read transport filler, so it is not waiting
     * for a response now.  Emitting writeDirectSpiResponse() here would push
     * stale echo words (e.g. for spilled tail samples 0x3FFE/0x3FFF) into the
     * TX FIFO.  Suppress responses so the replay only updates slave state;
     * the master will read completion later through 0x095A status polling. */
    s_bWaveSuppressResponse = true;
    for (i = 0U; i < u32FramesReceived; i++)
    {
        uint16_t cmd  = g_u16PostWaveCmdBuffer[i * 2U];
        uint16_t data = g_u16PostWaveCmdBuffer[(i * 2U) + 1U];

        if (s_bWaveParseFailed && (cmd == (uint16_t)WAVE_ACTIVATE_ADDR))
        {
            /* Skip ACTIVATE when this burst's parse failed */
            continue;
        }

        (void)SPIB_ParseRegFrame(cmd, data);
    }
    s_bWaveSuppressResponse = false;

    g_u32WaveRawParseCount++;
    (void)WaveDownload_FinalizeBurst(
        bTransportOk && (s_bWaveParseFailed == false));

    /* DIAG: RX FIFO level seen right after the burst, before recovery. */
    g_u16DiagFifoAtRestore = (uint16_t)SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE);

    /* Proactive full SPI module recovery (B01C).  ROOT CAUSE: a long continuous
     * burst leaves the SPIB SPI RX de-framed / overflow-locked, so it can drop
     * the first post-burst register transaction.  A
     * FIFO reset alone does NOT clear this -- proven by builds B017..B019, which
     * flushed/force-triggered the FIFO and still failed.  Only an SPISWRESET
     * (disable/enable) restores RX framing.  The existing 2ms watchdog already
     * performs exactly this recovery and is the reason the SPI looks healthy by
     * the time we halt (u32ResetCount==1).  Do the same recovery proactively
     * here while pending remains set, so RX is fully re-framed before status
     * polling begins.  Mirrors the watchdog body. */
    g_u32DiagSlaveRecoverStartTick = U32_UPCNTS;   /* B01D */
    if (s_bTxStatusActive == false)
    {
        SPI_disableModule(SPIB_SYSTEM_BASE);
        SPI_enableModule(SPIB_SYSTEM_BASE);
        SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
        SPI_resetTxFIFO(SPIB_SYSTEM_BASE);
        SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RX_OVERRUN);
    }
    g_u32DiagSlaveRecoverEndTick = U32_UPCNTS;   /* B01D */
    /* Mark RX activity so the 2ms watchdog does not also fire a redundant
     * reset right after this proactive one. */
    s_u32LastRxTicks = U32_UPCNTS;
    s_bSpiClean = true;

    /* Restore normal RegFrame[2] DMA (with edge-trigger recovery for a frame
     * already resident in the FIFO). */
    SPIB_RxRestartRegFrameDma();
    g_u32DiagSlaveRegDmaArmedTick = U32_UPCNTS;   /* B01D */
    WaveDownload_SetRegReady();

    g_u16WaveRawParsePending = 0U;
    g_u32DiagSlavePendingClearTick = U32_UPCNTS;   /* B01D */
}

/* One bounded slice of the deferred wave block parse.  Runs instead of the
 * regular poll body while g_u16WaveRawParsePending is set. */
static void SPIB_RxWaveParseChunk(void)
{
    uint16_t u16Remain = (uint16_t)(s_u16WaveParseTotal - s_u16WaveParseIndex);
    uint16_t u16Chunk = (u16Remain > SPIB_WAVE_PARSE_CHUNK_FRAMES) ?
        SPIB_WAVE_PARSE_CHUNK_FRAMES : u16Remain;

    if (u16Chunk > 0U)
    {
        const uint16_t *pRaw = (const uint16_t *)g_u16WaveRawRxBuffer +
                               ((uint32_t)s_u16WaveParseIndex * 2U);

        s_bWaveSuppressResponse = true;
        if (SPIB_ParseWaveRawBuffer(pRaw, u16Chunk) == false)
        {
            s_bWaveParseFailed = true;
        }
        s_bWaveSuppressResponse = false;

        s_u16WaveParseIndex = (uint16_t)(s_u16WaveParseIndex + u16Chunk);
        gSpibRxParseOkCount += u16Chunk;
    }

    if (s_u16WaveParseIndex >= s_u16WaveParseTotal)
    {
        SPIB_RxWaveFinishPostWave();
    }
}

static int16_t wr32bitsToSpi(HAL_U32PACK pHalPack) {
    g_u32DebugLastTx = pHalPack->u32All;

    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All >> 16));
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All & 0xFFFF));

    return 4;
}

static uint16_t calcSpiByteChecksum(uint16_t u16Data)
{
    return (uint16_t)((u16Data & 0x00FFU) + (u16Data >> 8));
}

static uint16_t calcSpiPacketChecksum(uint16_t u16Sum, uint16_t u16Word)
{
    return (uint16_t)(u16Sum + u16Word);
}

static void resetBlockReceiver(void)
{
    spiB_slave.u16BlockReady = 0U;
    spiB_slave.u16BlockWriteIndex = 0U;
    spiB_slave.u16BlockExpectedLen = 0U;
    spiB_slave.u16BlockChecksum = 0U;
    spiB_slave.u16BlockExpectedChecksum = 0U;
    spiB_slave.u16BlockExpectedChecksumValid = 0U;
    spiB_slave.u16BlockExpectedIndex = 0U;
    spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_NONE;
    spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_RECEIVING;
}

static void failBlockReceiver(uint16_t u16ErrorCode)
{
    spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_ERROR;
    spiB_slave.u16BlockErrorCode = u16ErrorCode;
    spiB_slave.u16FlashCommitPending = 0U;
    reportSlaveError(SPIB_FAULT_SOURCE_SERVICE, u16ErrorCode);
}

static uint16_t applyBlockDataWord(uint16_t u16Index, uint16_t u16Data)
{
    if (spiB_slave.eFlashState != FLASH_COMMIT_IDLE)
    {
        spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
        return spiB_slave.u16BlockStatus;
    }

    if (u16Index >= SIZE_OF_SPI_BLOCK_RAM)
    {
        spiB_slave.u32BlockOverflowCount++;
        spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_OVERFLOW;
        spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_OVERFLOW;
        reportSlaveError(SPIB_FAULT_SOURCE_SERVICE, (uint16_t)SPI_BLOCK_ERROR_OVERFLOW);
        return spiB_slave.u16BlockStatus;
    }

    if (u16Index == 0U)
    {
        resetBlockReceiver();
    }

    if ((spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_ERROR) ||
        (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_OVERFLOW))
    {
        return spiB_slave.u16BlockStatus;
    }

    if ((spiB_slave.u16BlockStatus != SPI_BLOCK_STATUS_RECEIVING) ||
        (u16Index != spiB_slave.u16BlockExpectedIndex))
    {
        failBlockReceiver(SPI_BLOCK_ERROR_OUT_OF_SEQUENCE);
        return spiB_slave.u16BlockStatus;
    }

    g_u16SpiBlockRam[u16Index] = u16Data;
    spiB_slave.u16BlockWriteIndex = (uint16_t)(u16Index + 1U);
    spiB_slave.u16BlockExpectedIndex = spiB_slave.u16BlockWriteIndex;
    spiB_slave.u16BlockChecksum =
        (uint16_t)(spiB_slave.u16BlockChecksum + calcSpiByteChecksum(u16Data));
    spiB_slave.u32BlockRxCount++;

    return u16Data;
}

static uint16_t setBlockExpectedChecksum(uint16_t u16Checksum)
{
    if (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_RECEIVING)
    {
        spiB_slave.u16BlockExpectedChecksum = u16Checksum;
        spiB_slave.u16BlockExpectedChecksumValid = 1U;
        return u16Checksum;
    }

    failBlockReceiver(SPI_BLOCK_ERROR_OUT_OF_SEQUENCE);
    return spiB_slave.u16BlockStatus;
}

static uint16_t finalizeBlockReceiver(uint16_t u16ExpectedLen)
{
    spiB_slave.u16BlockExpectedLen = (u16ExpectedLen == 0U) ?
        spiB_slave.u16BlockWriteIndex : u16ExpectedLen;

    if (spiB_slave.u16BlockStatus != SPI_BLOCK_STATUS_OVERFLOW)
    {
        if (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_ERROR)
        {
            /* Keep original error */
        }
        else if (spiB_slave.u16BlockExpectedLen != spiB_slave.u16BlockWriteIndex)
        {
            failBlockReceiver(SPI_BLOCK_ERROR_LENGTH_MISMATCH);
        }
        else if (spiB_slave.u16BlockExpectedChecksumValid == 0U)
        {
            failBlockReceiver(SPI_BLOCK_ERROR_CHECKSUM_MISSING);
        }
        else if (spiB_slave.u16BlockExpectedChecksum != spiB_slave.u16BlockChecksum)
        {
            failBlockReceiver(SPI_BLOCK_ERROR_CHECKSUM_MISMATCH);
        }
        else
        {
            spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
            spiB_slave.u16FlashCommitPending = 1U;
            spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_NONE;
        }
    }

    return spiB_slave.u16BlockStatus;
}

static void writeDirectSpiResponse(uint16_t u16Address, uint16_t u16Data)
{
    uint16_t u16RespAddr = (uint16_t)(u16Address + calcSpiByteChecksum(u16Data));

    if (s_bWaveSuppressResponse)
    {
        return;
    }

    /* CH4 owns the TX FIFO while a status packet streams out; CPU echo
     * words would interleave into the packet and corrupt it. */
    if (s_bTxStatusActive)
    {
        gSpibTxSuppressedRespCount++;
        return;
    }

    g_u32DebugLastTx = ((uint32_t)u16RespAddr << 16) | u16Data;

    /* DIAG (B018): log every response actually pushed to the TX FIFO. */
    g_u16SpibTxLogAddr[g_u16SpibTxLogIdx] = u16RespAddr;
    g_u16SpibTxLogData[g_u16SpibTxLogIdx] = u16Data;
    g_u16SpibTxLogIdx =
        (uint16_t)((g_u16SpibTxLogIdx + 1U) & (SPIB_DIAG_LOG_DEPTH - 1U));

    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16RespAddr);
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16Data);
}

/* =========================================================================
 * Status packet TX over DMA CH4 (D02_2_1 5.2)
 * ========================================================================= */

static void SPIB_TxStatusFillPacket(uint16_t *pBuf)
{
    uint16_t u16Sum = 0U;
    uint16_t i;

    pBuf[0]  = (uint16_t)SPIB_STATUS_PACKET_MAGIC;
    pBuf[1]  = gSpibFwBuildTag;
    pBuf[2]  = (uint16_t)(((uint16_t)spiB_slave.stDiag.eHealth << 12) |
                          ((uint16_t)spiB_slave.stDiag.eFaultSource << 8) |
                          (spiB_slave.stDiag.u16FaultCode & 0x00FFU));
    pBuf[3]  = gSpibRxErrorFlags;
    pBuf[4]  = spiB_slave.u16BlockStatus;
    pBuf[5]  = g_waveDownload.u16SelectedPage;
    pBuf[6]  = (g_waveDownload.u16SelectedPage == WAVE_PAGE_INVALID) ?
               0xFFFFU :
               g_waveDownload.u16PageState[g_waveDownload.u16SelectedPage];
    pBuf[7]  = OUTPUT_ON;
    pBuf[8]  = (uint16_t)(gSpibRxDmaDoneCount & 0xFFFFU);
    pBuf[9]  = (uint16_t)(gSpibRxDmaDoneCount >> 16);
    pBuf[10] = (uint16_t)(gSpibRxParseOkCount & 0xFFFFU);
    pBuf[11] = (uint16_t)(gSpibRxParseOkCount >> 16);
    pBuf[12] = (uint16_t)(gSpibRxParseFailCount & 0xFFFFU);
    pBuf[13] = (uint16_t)(gSpibRxParseFailCount >> 16);
    s_u16TxStatusSeq++;
    pBuf[14] = s_u16TxStatusSeq;

    for (i = 0U; i < (uint16_t)(SPIB_STATUS_PACKET_WORDS - 1U); i++)
    {
        u16Sum = (uint16_t)(u16Sum + pBuf[i]);
    }
    pBuf[SPIB_STATUS_PACKET_WORDS - 1U] = u16Sum;
}

/* Background refresh: fill the idle buffer, then promote it to DMA source.
 * Skipped while CH4 owns a buffer so an in-flight packet is never touched. */
void SPIB_TxStatusRefresh(void)
{
    uint16_t *pSwap;

    if (s_bTxStatusActive)
    {
        return;
    }

    SPIB_TxStatusFillPacket(s_pTxStatusFill);

    pSwap = s_pTxStatusDma;
    s_pTxStatusDma = s_pTxStatusFill;
    s_pTxStatusFill = pSwap;
}

static void SPIB_TxStatusInit(void)
{
    s_pTxStatusFill = s_u16TxStatusPing;
    s_pTxStatusDma  = s_u16TxStatusPong;
    s_bTxStatusActive = false;
    s_u16TxStatusSeq = 0U;
    gSpibTxStatusPacketCount = 0U;
    gSpibTxStatusAbortCount = 0U;
    gSpibTxSuppressedRespCount = 0U;

    DMA_disableTrigger(SPIB_TX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_TX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_TX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_TX_DMA_CH_BASE);

    SPIB_TxStatusFillPacket(s_u16TxStatusPing);
    SPIB_TxStatusFillPacket(s_u16TxStatusPong);
}

/* Bind the freshest packet buffer to CH4 and hand it the TX FIFO.  The
 * request ack (2 CPU words) is already in the FIFO; the packet streams out
 * behind it as the master clocks idle frames. */
static void SPIB_TxStatusArm(void)
{
    if (s_bTxStatusActive)
    {
        return;
    }

    DMA_disableTrigger(SPIB_TX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_TX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_TX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_TX_DMA_CH_BASE);

    DMA_configAddresses(SPIB_TX_DMA_CH_BASE,
                        (uint16_t *)(SPIB_SYSTEM_BASE + SPI_O_TXBUF),
                        s_pTxStatusDma);
    DMA_configBurst(SPIB_TX_DMA_CH_BASE, SPIB_RX_REG_WORDS, 1, 0);
    DMA_configTransfer(SPIB_TX_DMA_CH_BASE,
                       SPIB_STATUS_PACKET_WORDS / SPIB_RX_REG_WORDS, 1, 0);
    DMA_configMode(SPIB_TX_DMA_CH_BASE, DMA_TRIGGER_SPIBTX,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);

    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_TXFF);
    DMA_enableTrigger(SPIB_TX_DMA_CH_BASE);
    DMA_startChannel(SPIB_TX_DMA_CH_BASE);

    s_bTxStatusActive = true;
    s_u32TxStatusArmTicks = U32_UPCNTS;

    /* The TX DMA request is edge-triggered by TXFFST crossing TXFFIL from
     * above (mirror of the RXFF behavior learned in block mode).  Right
     * after arming the FIFO holds only the 2-word ack (already <= TXFFIL),
     * so no edge will ever come: software-kick bursts until the level is
     * above the watermark, then master clocking generates real edges. */
    {
        uint16_t u16Kick;
        for (u16Kick = 0U;
             u16Kick < SPIB_WAVE_FORCE_TRIGGER_LIMIT;
             u16Kick++)
        {
            if (DMA_getRunStatusFlag(SPIB_TX_DMA_CH_BASE) == false)
            {
                break;
            }
            if (SPI_getTxFIFOStatus(SPIB_SYSTEM_BASE) > SPI_FIFO_TX2)
            {
                break;
            }
            DMA_forceTrigger(SPIB_TX_DMA_CH_BASE);
            /* let the forced burst move 2 words before re-checking */
            __asm(" RPT #40 || NOP");
        }
    }
}

static void SPIB_TxStatusAbort(void)
{
    DMA_disableTrigger(SPIB_TX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_TX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_TX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_TX_DMA_CH_BASE);
    SPI_resetTxFIFO(SPIB_SYSTEM_BASE);

    if (s_bTxStatusActive)
    {
        s_bTxStatusActive = false;
        gSpibTxStatusAbortCount++;
        gSpibRxErrorFlags |= SPIB_RX_ERR_TX_STATUS_TIMEOUT;
    }
}

/* Poll CH4 completion; recover the TX FIFO for CPU echo responses.  A
 * master that stops clocking mid-packet is timed out like the RX paths. */
static void SPIB_TxStatusService(void)
{
    if (s_bTxStatusActive == false)
    {
        return;
    }

    if (DMA_getRunStatusFlag(SPIB_TX_DMA_CH_BASE) == false)
    {
        DMA_disableTrigger(SPIB_TX_DMA_CH_BASE);
        DMA_clearTriggerFlag(SPIB_TX_DMA_CH_BASE);
        s_bTxStatusActive = false;
        gSpibTxStatusPacketCount++;
        return;
    }

    if (getElapsedTicks(s_u32TxStatusArmTicks) > SPIB_RX_DMA_TIMEOUT_TICKS)
    {
        SPIB_TxStatusAbort();
    }
}

static uint16_t tryHandleBlockPath(uint16_t u16Address, uint16_t u16Data)
{
    if ((u16Address >= Spi_Block_Data_Base_spi_addr) &&
        (u16Address <= Spi_Block_Data_Last_spi_addr))
    {
        uint16_t u16Index = (uint16_t)(u16Address - Spi_Block_Data_Base_spi_addr);
        uint16_t u16RespData;

        u16RespData = applyBlockDataWord(u16Index, u16Data);
        writeDirectSpiResponse(u16Address, u16RespData);
        return 1U;
    }

    if (u16Address == Spi_Block_Expected_CheckSum_spi_addr)
    {
        writeDirectSpiResponse(u16Address, setBlockExpectedChecksum(u16Data));
        return 1U;
    }

    if (u16Address == Spi_Block_End_spi_addr)
    {
        writeDirectSpiResponse(u16Address, finalizeBlockReceiver(u16Data));
        return 1U;
    }

    return 0U;
}

static uint16_t tryHandleFastPath(uint16_t u16Address, uint16_t u16Data)
{
    uint16_t u16RespData = 0U;

    if (u16Address == 0xFFFFU)
    {
        return 1U;
    }

    if (u16Address == (uint16_t)WAVE_BURST_BEGIN_ADDR)
    {
        /* Burst preamble: u16Data = sample count of the continuous
         * stream that follows.  Flag the pre-arm; pollReceiveFromSpi
         * arms the block DMA right after this frame is handled.
         * Frames to capture = guard frames + samples + trailing flush. */
        s_bWaveBurstBegin = false;
        s_u16WaveBurstFrames = 0U;
        if ((OUTPUT_ON == 0U) &&
            (WaveDownload_BeginBurst(u16Data) == u16Data))
        {
            u16RespData = u16Data;
            s_bWaveBurstBegin = true;
            s_u16WaveBurstFrames = u16Data;
        }
        else
        {
            u16RespData = 0xFFFFU;
        }
        writeDirectSpiResponse(u16Address, u16RespData);
        spiB_slave.u32FastPathCount++;
        return 1U;
    }

    /* Route to Wave Download Service if it handles this address */
    if (WaveDownload_HandleWrite(u16Address, u16Data, &u16RespData, OUTPUT_ON))
    {
        writeDirectSpiResponse(u16Address, u16RespData);
        spiB_slave.u32FastPathCount++;
        return 1U;
    }

    /* DIAG_COMPAT_ONLY: Fallback to legacy block path only when Wave Download is inactive */
    if (tryHandleBlockPath(u16Address, u16Data) == 1U)
    {
        spiB_slave.u32FastPathCount++;
        return 1U;
    }

    switch (u16Address)
    {
    case C2000_Version_spi_addr:
        writeDirectSpiResponse(u16Address, DSP_FW_Version_Code_CPU1);
        break;

    case Machine_Status_spi_addr:
        /*
         * The current test firmware has no production machine-status source.
         * Preserve the legacy echo behavior explicitly for link verification.
         */
        writeDirectSpiResponse(u16Address, u16Data);
        break;

    case CPU2_Version_spi_addr:
        writeDirectSpiResponse(u16Address, DSP_FW_Version_Code_CPU2);
        break;

    case Startup_State_spi_addr:
        writeDirectSpiResponse(u16Address, startupFlags);
        break;

    case C2000_Alarm_Status_spi_addr:
        /* [15:12] health, [11:8] fault source, [7:0] fault code */
        writeDirectSpiResponse(u16Address,
            (uint16_t)(((uint16_t)spiB_slave.stDiag.eHealth << 12) |
                       ((uint16_t)spiB_slave.stDiag.eFaultSource << 8) |
                       (spiB_slave.stDiag.u16FaultCode & 0x00FFU)));
        break;

    case C2000_Alarm_Status_2_spi_addr:
        writeDirectSpiResponse(u16Address, gSpibRxErrorFlags);
        break;

    case C2000_Fault_Clear_spi_addr:
        if (u16Data == 1U)
        {
            clearSlaveFaults();
        }
        writeDirectSpiResponse(u16Address, (uint16_t)spiB_slave.stDiag.eHealth);
        break;

    case Spi_Status_Packet_Req_spi_addr:
        /* Ack via the CPU path first, then hand the TX FIFO to CH4 for
         * one status packet (TxPing/Pong -> DMA CH4 -> SPIB TX FIFO). */
        writeDirectSpiResponse(u16Address, (uint16_t)SPIB_STATUS_PACKET_WORDS);
        SPIB_TxStatusArm();
        break;

    case Spi_Block_Status_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockStatus);
        break;

    case Spi_Block_Write_Index_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockWriteIndex);
        break;

    case Spi_Block_CheckSum_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockChecksum);
        break;

    /* Spi_Block_Expected_CheckSum_spi_addr is handled by tryHandleBlockPath
     * above and can never reach this switch. */

    case Spi_Block_Progress_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockProgress);
        break;

    case Output_ON_OFF_spi_addr:
        OUTPUT_ON = u16Data & 0x01U;
        writeDirectSpiResponse(u16Address, OUTPUT_ON);
        break;

    default:
        if (((u16Address >> 8) & 0xFFU) == 0x04U)
        {
            u16RespData = u16Data;
            writeDirectSpiResponse(u16Address, u16RespData);
        }
        else
        {
            return 0U;
        }
        break;
    }

    spiB_slave.u32FastPathCount++;
    return 1U;
}

static bool SPIB_ParseLegacyRegFrame(uint16_t u16Cmd, uint16_t u16Data)
{
    if (tryHandleFastPath(u16Cmd, u16Data) == 1U)
    {
        return true;
    }

    spiB_slave.u32FallbackPathCount++;
    {
        SPI_FRAME_t frame;
        frame.cmd  = u16Cmd;
        frame.data = u16Data;
        if (SPI_FIFO_Push(&s_fallbackFifo, &frame))
        {
            return true;
        }
        spiB_slave.stat |= _SSS_GET_ERROR;
        reportSlaveError(SPIB_FAULT_SOURCE_SERVICE, (uint16_t)1U);
        return false;
    }
}

static void resetPacketParser(void)
{
    spiB_slave.ePacketState = SPIB_PACKET_STATE_IDLE;
    spiB_slave.u16PacketCmd = 0U;
    spiB_slave.u16PacketLength = 0U;
    spiB_slave.u16PacketIndex = 0U;
    spiB_slave.u16PacketChecksum = 0U;
    spiB_slave.u16PacketFirstPayload = 0U;
}

static bool failPacketParser(uint16_t u16ErrorCode)
{
    spiB_slave.u16PacketErrorCode = u16ErrorCode;
    reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL, u16ErrorCode);
    resetPacketParser();
    return false;
}

static bool feedPacketWord(uint16_t u16Word, bool *pbPacketDone)
{
    *pbPacketDone = false;

    switch (spiB_slave.ePacketState)
    {
    case SPIB_PACKET_STATE_IDLE:
        if (u16Word != SPIB_PACKET_HEADER_MAGIC)
        {
            return failPacketParser(SPIB_PACKET_ERROR_UNSUPPORTED_CMD);
        }

        spiB_slave.u16PacketChecksum =
            calcSpiPacketChecksum(0U, SPIB_PACKET_HEADER_MAGIC);
        spiB_slave.u16PacketErrorCode = SPIB_PACKET_ERROR_NONE;
        spiB_slave.ePacketState = SPIB_PACKET_STATE_CMD;
        break;

    case SPIB_PACKET_STATE_CMD:
        spiB_slave.u16PacketCmd = u16Word;
        spiB_slave.u16PacketChecksum =
            calcSpiPacketChecksum(spiB_slave.u16PacketChecksum, u16Word);
        spiB_slave.ePacketState = SPIB_PACKET_STATE_LENGTH;
        break;

    case SPIB_PACKET_STATE_LENGTH:
        spiB_slave.u16PacketLength = u16Word;
        spiB_slave.u16PacketIndex = 0U;
        spiB_slave.u16PacketChecksum =
            calcSpiPacketChecksum(spiB_slave.u16PacketChecksum, u16Word);

        if (spiB_slave.u16PacketCmd == Spi_Block_Data_Base_spi_addr)
        {
            if ((u16Word == 0U) || (u16Word > SPIB_PACKET_MAX_PAYLOAD_WORDS))
            {
                return failPacketParser(SPIB_PACKET_ERROR_BAD_LENGTH);
            }

            if (spiB_slave.eFlashState != FLASH_COMMIT_IDLE)
            {
                spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
                return failPacketParser(SPIB_PACKET_ERROR_BLOCK_BUSY);
            }
        }
        else if (u16Word > 1U)
        {
            return failPacketParser(SPIB_PACKET_ERROR_BAD_LENGTH);
        }

        spiB_slave.ePacketState = (u16Word == 0U) ?
            SPIB_PACKET_STATE_CHECKSUM : SPIB_PACKET_STATE_PAYLOAD;
        break;

    case SPIB_PACKET_STATE_PAYLOAD:
        spiB_slave.u16PacketChecksum =
            calcSpiPacketChecksum(spiB_slave.u16PacketChecksum, u16Word);

        if (spiB_slave.u16PacketCmd == Spi_Block_Data_Base_spi_addr)
        {
            (void)applyBlockDataWord(spiB_slave.u16PacketIndex, u16Word);
            if ((spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_OVERFLOW) ||
                (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_ERROR) ||
                (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_BUSY))
            {
                return failPacketParser(SPIB_PACKET_ERROR_UNSUPPORTED_CMD);
            }
        }
        else
        {
            if (spiB_slave.u16PacketIndex == 0U)
            {
                spiB_slave.u16PacketFirstPayload = u16Word;
            }
        }

        spiB_slave.u16PacketIndex++;
        if (spiB_slave.u16PacketIndex >= spiB_slave.u16PacketLength)
        {
            spiB_slave.ePacketState = SPIB_PACKET_STATE_CHECKSUM;
        }
        break;

    case SPIB_PACKET_STATE_CHECKSUM:
        spiB_slave.u16PacketLastChecksum = u16Word;
        if (u16Word != spiB_slave.u16PacketChecksum)
        {
            if (spiB_slave.u16PacketCmd == Spi_Block_Data_Base_spi_addr)
            {
                failBlockReceiver(SPI_BLOCK_ERROR_CHECKSUM_MISMATCH);
            }
            return failPacketParser(SPIB_PACKET_ERROR_CHECKSUM);
        }

        if (spiB_slave.u16PacketCmd == Spi_Block_Data_Base_spi_addr)
        {
            uint16_t u16Status;
            (void)setBlockExpectedChecksum(spiB_slave.u16BlockChecksum);
            u16Status = finalizeBlockReceiver(spiB_slave.u16PacketLength);
            writeDirectSpiResponse(spiB_slave.u16PacketCmd, u16Status);
        }
        else
        {
            uint16_t u16Data = (spiB_slave.u16PacketLength == 0U) ?
                0U : spiB_slave.u16PacketFirstPayload;
            if (SPIB_ParseLegacyRegFrame(spiB_slave.u16PacketCmd, u16Data) == false)
            {
                return failPacketParser(SPIB_PACKET_ERROR_UNSUPPORTED_CMD);
            }
        }

        spiB_slave.stDiag.stProtocol.stComm.u32RxTotal++;
        resetPacketParser();
        *pbPacketDone = true;
        break;

    default:
        return failPacketParser(SPIB_PACKET_ERROR_UNSUPPORTED_CMD);
    }

    return true;
}

bool SPIB_ParseRegFrame(uint16_t u16Cmd, uint16_t u16Data)
{
    bool bPacketDone = false;

    s_u32LastRxTicks = U32_UPCNTS;
    s_bSpiClean = false;

    if ((spiB_slave.ePacketState != SPIB_PACKET_STATE_IDLE) ||
        (u16Cmd == SPIB_PACKET_HEADER_MAGIC))
    {
        if (feedPacketWord(u16Cmd, &bPacketDone) == false)
        {
            return false;
        }

        if (bPacketDone)
        {
            if (u16Data == SPIB_PACKET_PADDING_WORD)
            {
                return true;
            }

            spiB_slave.u16PacketErrorCode = SPIB_PACKET_ERROR_PADDING;
            reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL, (uint16_t)SPIB_PROT_FAULT_PADDING);
            return false;
        }

        if (feedPacketWord(u16Data, &bPacketDone) == false)
        {
            return false;
        }

        return true;
    }

    return SPIB_ParseLegacyRegFrame(u16Cmd, u16Data);
}

static void handleBackgroundFlashCommit(void)
{
    switch (spiB_slave.eFlashState)
    {
    case FLASH_COMMIT_IDLE:
        if (spiB_slave.u16FlashCommitPending == 1U)
        {
            spiB_slave.eFlashState = FLASH_COMMIT_BUSY;
            spiB_slave.u16BlockProgress = 0U;
            spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
        }
        break;

    case FLASH_COMMIT_BUSY:
        if (spiB_slave.u16BlockProgress < 100U)
        {
            spiB_slave.u16BlockProgress = (uint16_t)(spiB_slave.u16BlockProgress + 10U);
        }

        if (spiB_slave.u16BlockProgress >= 100U)
        {
            spiB_slave.u16BlockProgress = 100U;
            spiB_slave.eFlashState = FLASH_COMMIT_DONE;
        }
        break;

    case FLASH_COMMIT_DONE:
        spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_READY;
        spiB_slave.u16BlockReady = 1U;
        spiB_slave.u16FlashCommitPending = 0U;
        spiB_slave.eFlashState = FLASH_COMMIT_IDLE;
        spiB_slave.stDiag.stService.stComm.u32TxTotal++;
        break;

    case FLASH_COMMIT_ERROR:
        spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_ERROR;
        spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_FLASH_COMMIT;
        spiB_slave.u16FlashCommitPending = 0U;
        spiB_slave.eFlashState = FLASH_COMMIT_IDLE;
        reportSlaveError(SPIB_FAULT_SOURCE_SERVICE, (uint16_t)SPI_BLOCK_ERROR_FLASH_COMMIT);
        break;

    default:
        spiB_slave.eFlashState = FLASH_COMMIT_IDLE;
        break;
    }
}

void initSPIslave(void)
{
    uint16_t u16Idx;

    DMA_initController();
    initNativeSpiSlave(SPIB_SYSTEM_BASE);
    SPI_FIFO_Init(&s_fallbackFifo);
    initSpibRxDma();
    WaveDownload_Init();
    SPIB_TxStatusInit();

    for (u16Idx = 0U; u16Idx < SIZE_OF_SPI_BLOCK_RAM; u16Idx++)
    {
        g_u16SpiBlockRam[u16Idx] = 0U;
    }

    for (u16Idx = 0U; u16Idx < 4096U; u16Idx++)
    {
        g_u16WaveRawRxBuffer[u16Idx] = 0U;
        g_u16WaveRawRxBufferHi[u16Idx] = 0U;
    }

    s_sSpiParser = (ST_WR_PARSER) {
      .wrfunc = wr32bitsToSpi,
      .rdfunc = 0,
      .pRdata = (HAL_U32PACK)0,
    };

    pushNullIntoTxD(&s_sSpiParser);

    spiB_slave.stDiag.eHealth = SPIB_HEALTH_OK;
    spiB_slave.stDiag.eFaultSource = SPIB_FAULT_SOURCE_NONE;
    spiB_slave.stDiag.u16FaultCode = 0U;

    spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_INIT;
    spiB_slave.stDiag.stDriver.eFault = SPIB_DRV_FAULT_NONE;
    CommDiag_Init((ST_COMM_DIAG *)&spiB_slave.stDiag.stDriver.stComm);

    spiB_slave.stDiag.stProtocol.eState = SPIB_PROT_STATE_IDLE;
    spiB_slave.stDiag.stProtocol.eFault = SPIB_PROT_FAULT_NONE;
    CommDiag_Init((ST_COMM_DIAG *)&spiB_slave.stDiag.stProtocol.stComm);

    spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_IDLE;
    spiB_slave.stDiag.stService.eFault = SPIB_SERV_FAULT_NONE;
    CommDiag_Init((ST_COMM_DIAG *)&spiB_slave.stDiag.stService.stComm);

    spiB_slave.u32ResetCount = 0U;
    spiB_slave.u32FastPathCount = 0U;
    spiB_slave.u32FallbackPathCount = 0U;
    spiB_slave.u32BlockRxCount = 0U;
    spiB_slave.u32BlockOverflowCount = 0U;
    spiB_slave.u16BlockReady = 0U;
    spiB_slave.u16BlockWriteIndex = 0U;
    spiB_slave.u16BlockExpectedLen = 0U;
    spiB_slave.u16BlockChecksum = 0U;
    spiB_slave.u16BlockExpectedChecksum = 0U;
    spiB_slave.u16BlockExpectedChecksumValid = 0U;
    spiB_slave.u16BlockExpectedIndex = 0U;
    spiB_slave.u16BlockErrorCode = SPI_BLOCK_ERROR_NONE;
    spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_IDLE;

    spiB_slave.u16FlashCommitPending = 0U;
    spiB_slave.u16BlockProgress = 0U;
    spiB_slave.eFlashState = FLASH_COMMIT_IDLE;

    resetPacketParser();
    spiB_slave.u16PacketLastChecksum = 0U;
    spiB_slave.u16PacketErrorCode = SPIB_PACKET_ERROR_NONE;

    /* Avoid one spurious idle-reset right after boot (LastRxTicks == 0). */
    s_u32LastRxTicks = U32_UPCNTS;
    s_bSpiClean = true;

    /* The wave block DMA spans both raw buffers (8192 words).  Fault out
     * loudly if the linker ever stops placing them back-to-back
     * (RAMGS10 -> RAMGS11); silent corruption would follow otherwise. */
    if (((uint32_t)(uintptr_t)g_u16WaveRawRxBufferHi) !=
        ((uint32_t)(uintptr_t)g_u16WaveRawRxBuffer + 4096UL))
    {
        reportSlaveError(SPIB_FAULT_SOURCE_DRIVER,
                         (uint16_t)SPIB_DRV_FAULT_DMA_ERROR);
    }

    SET_SSS_STAT(_INIT_SSS_READY);
}

void pollReceiveFromSpi(void)
{
    uint32_t u32SpiIntStatus;
    SPI_RxFIFOLevel eRxFifoLevel;
    bool bDmaDone;

    /* DIAG (B019): steady-state snapshot of the RX/DMA condition. */
    g_u32DiagPollCount++;
    g_u16DiagRxFifoLvl = (uint16_t)SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE);
    g_u16DiagDmaRun =
        (uint16_t)(DMA_getRunStatusFlag(SPIB_RX_DMA_CH_BASE) ? 1U : 0U);
    g_u16DiagDmaArmed = (uint16_t)(s_bSpibRxDmaArmed ? 1U : 0U);
    g_u16DiagInWave = (uint16_t)(s_bSpibRxInWaveMode ? 1U : 0U);
    g_u16DiagPending = g_u16WaveRawParsePending;
    /* DIAG (B01B): raw SPI peripheral RX state -- is the FIFO in reset / has it
     * latched an overflow that is now silently discarding incoming frames? */
    g_u32DiagSpiIntStatus = SPI_getInterruptStatus(SPIB_SYSTEM_BASE);
    g_u16DiagSpiFfrx = HWREGH(SPIB_SYSTEM_BASE + SPI_O_FFRX);
    g_u16DiagSpiCcr = HWREGH(SPIB_SYSTEM_BASE + SPI_O_CCR);

    /* A completed wave block is parsed in bounded slices; CH3 is meanwhile
     * armed on the post-wave command buffer, so the regular poll body (which
     * interprets CH3 state as RegFrame traffic) must not run until done. */
    if (g_u16WaveRawParsePending != 0U)
    {
        SPIB_RxWaveParseChunk();
        return;
    }

    /* CH4 status packet completion / stall timeout */
    SPIB_TxStatusService();

    u32SpiIntStatus = SPI_getInterruptStatus(SPIB_SYSTEM_BASE);
    eRxFifoLevel = SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE);
    bDmaDone = SPIB_RxDmaIsDone();

    if (bDmaDone)
    {
        spiB_slave.stDiag.stDriver.stComm.u32RxTotal++;

        if (s_bSpibRxInWaveMode)
        {
            /* Block capture complete: re-arm CH3 onto the post-wave command
             * buffer so control frames arriving during the parse are kept,
             * then hand the 4096-frame parse to the sliced path (next polls
             * run SPIB_RxWaveParseChunk until SPIB_RxWaveFinishPostWave). */
            DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
            SPIB_RxDmaClearDone();

            g_u32PostWaveDstBeg = (uint32_t)(uintptr_t)g_u16PostWaveCmdBuffer;
            SPIB_RxDma_Configure((uint16_t *)g_u16PostWaveCmdBuffer,
                                 SPIB_RX_REG_WORDS, SPIB_POST_WAVE_CMD_FRAMES);
            DMA_startChannel(SPIB_RX_DMA_CH_BASE);

            s_bSpibRxInWaveMode = false;

            /* Keep per-frame counter semantics so the self-test
             * invariants (parse_ok == dma_done, dma_restart >= dma_done)
             * stay valid in block mode. */
            gSpibRxDmaDoneCount += s_u16WaveBurstFrames;

            s_u16WaveParseIndex = 0U;
            s_u16WaveParseTotal = s_u16WaveBurstFrames;
            s_bWaveParseFailed = false;
            g_u16WaveRawParsePending = 1U;
        }
        else
        {
            volatile uint16_t *pFrame;
            uint16_t u16Cmd;
            uint16_t u16Data;
            bool bParseOk;

            gSpibRxDmaDoneCount++;

            pFrame  = SpibPingPong_SwapAndGetFrame(&s_rxPingPong);
            u16Cmd  = pFrame[0];
            u16Data = pFrame[1];

            s_u16LastRxCmd  = u16Cmd;
            s_u16LastRxData = u16Data;

            /* DIAG (B018): log every latched frame, idle 0xFFFF included. */
            g_u32DiagNormalFrames++;
            g_u16SpibRxLogCmd[g_u16SpibRxLogIdx]  = u16Cmd;
            g_u16SpibRxLogData[g_u16SpibRxLogIdx] = u16Data;
            g_u16SpibRxLogIdx =
                (uint16_t)((g_u16SpibRxLogIdx + 1U) & (SPIB_DIAG_LOG_DEPTH - 1U));

            if (u16Cmd != 0xFFFFU)
            {
                g_u32SpiSlaveLastRequest =
                    ((uint32_t)u16Cmd << 16) | (uint32_t)u16Data;
            }

            SPIB_RxDmaClearDone();

            bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);

            if (bParseOk)
            {
                gSpibRxParseOkCount++;
            }
            else
            {
                gSpibRxParseFailCount++;
                gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL;
                reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL,
                                 (uint16_t)SPIB_PROT_FAULT_FRAME_PARSE_FAIL);
            }

            if (s_bWaveBurstBegin)
            {
                /* Preamble announced the stream: pre-arm block transport
                 * before the first sample arrives (zero-loss entry). */
                s_bWaveBurstBegin = false;
                SPIB_RxWaveEnterBlockMode();
            }
            else
            {
                SPIB_RxRestartRegFrameDma();
            }

            s_u16PrevRxCmd = u16Cmd;
        }

        return;
    }

    /* Edge-trigger watchdog (B01A): the RX DMA request is an EDGE on RXFFST
     * crossing RXFFIL.  If a complete frame is resident in the RX FIFO while the
     * DMA is armed but has not signalled done, no rising edge ever occurred
     * (the FIFO was already >= RX2 when the channel was armed) and the frame
     * would sit forever -- this is exactly how the post-burst 0x0959 got lost.
     *
     * Debounced so it cannot misfire during the sub-poll window where a frame
     * is legitimately mid-capture (FIFO briefly >= RX2 while the DMA is running
     * after a natural edge): that condition clears within one poll, whereas the
     * edge-trap is permanent and accumulates every poll.  Only a persistently
     * stuck frame reaches the threshold; then force one trigger and the next
     * poll processes it via the bDmaDone path above. */
    if ((s_bSpibRxInWaveMode == false) &&
        (s_bSpibRxDmaArmed != false) &&
        (eRxFifoLevel >= SPI_FIFO_RX2))
    {
        s_u16RxStuckPolls++;
        if (s_u16RxStuckPolls >= SPIB_RX_STUCK_KICK_POLLS)
        {
            s_u16RxStuckPolls = 0U;
            g_u32DiagForceKickCount++;
            DMA_forceTrigger(SPIB_RX_DMA_CH_BASE);
            return;
        }
    }
    else
    {
        s_u16RxStuckPolls = 0U;
    }

    /* Wave block in flight: block completion and stall recovery own the RX
     * path until normal two-word DMA mode is restored. */
    if (s_bSpibRxInWaveMode)
    {
        uint16_t u16XferLeft;

        if ((u32SpiIntStatus & (SPI_INT_RX_OVERRUN | SPI_INT_RXFF_OVERFLOW)) != 0U)
        {
            /* Expected while parsing overlaps the stream; words may be
             * lost (sample count shows it), but the block reception
             * keeps running. */
            gSpibRxErrorFlags |= SPIB_RX_ERR_RX_FIFO_OVERFLOW;
            SPI_clearInterruptStatus(SPIB_SYSTEM_BASE,
                                     SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
        }

        if (DMA_getOverflowFlag(SPIB_RX_DMA_CH_BASE))
        {
            gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_ERROR;
            DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);
        }

        /* TRANSFER_COUNT is stale until the first burst of a new channel.
         * DST_ADDR_ACTIVE advances by 2 words per received frame, so
         * (active - begin) / 2 is the received frame count. */
        {
            uint32_t u32DstBeg =
                HWREG(SPIB_RX_DMA_CH_BASE + DMA_O_DST_BEG_ADDR_ACTIVE);
            uint32_t u32DstNow =
                HWREG(SPIB_RX_DMA_CH_BASE + DMA_O_DST_ADDR_ACTIVE);

            /* Before the first burst, ACTIVE can still describe the previous
             * transfer. A begin-address mismatch means no frame has reached
             * the newly armed buffer. */
            if (u32DstBeg != (uint32_t)(uintptr_t)g_u16WaveRawRxBuffer)
            {
                u16XferLeft = 0U;
            }
            else
            {
                u16XferLeft = (uint16_t)(u32DstNow - u32DstBeg);   /* words */
            }
        }

        if (u16XferLeft != s_u16WaveLastXferCount)
        {
            s_u16WaveLastXferCount = u16XferLeft;
            s_u32WaveStallTicks = U32_UPCNTS;
        }
        else if (getElapsedTicks(s_u32WaveStallTicks) >
                 SPIB_RX_DMA_TIMEOUT_TICKS)
        {
            uint16_t u16Frames = (uint16_t)(u16XferLeft / 2U);

            if (u16Frames > s_u16WaveBurstFrames)
            {
                u16Frames = s_u16WaveBurstFrames;
            }
            SPIB_RxWaveDrainAndExit(u16Frames);
        }
        return;
    }

    if ((u32SpiIntStatus & (SPI_INT_RX_OVERRUN | SPI_INT_RXFF_OVERFLOW)) != 0U)
    {
        gSpibRxErrorFlags |= SPIB_RX_ERR_RX_FIFO_OVERFLOW;

        if ((OUTPUT_ON == 0U) &&
            (g_waveDownload.u16SelectedPage != WAVE_PAGE_INVALID) &&
                 (s_u16PrevRxCmd >= (uint16_t)WAVE_DATA_WINDOW_BASE) &&
                 (s_u16PrevRxCmd <  (uint16_t)WAVE_DATA_WINDOW_LIMIT))
        {
            /* Compatibility fallback for a legacy sender that omits
             * WAVE_BURST_BEGIN. This path is best-effort because resetting
             * an already-overflowed FIFO cannot guarantee zero loss. */
            SPIB_RxDmaClearDone();
            SPI_resetRxFIFO(SPIB_SYSTEM_BASE);

            s_u16WaveBurstFrames =
                (uint16_t)(WAVE_SAMPLES_PER_PAGE -
                           (uint16_t)(s_u16PrevRxCmd - WAVE_DATA_WINDOW_BASE) - 1U);
            SPIB_RxWaveEnterBlockMode();
        }
        else
        {
            spiB_slave.stat |= _SSS_GET_ERROR;

            reportSlaveError(SPIB_FAULT_SOURCE_DRIVER,
                             (uint16_t)SPIB_DRV_FAULT_FIFO_OVERFLOW);

            SPIB_RxDmaClearDone();
            SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
            SPIB_RxDmaRestart();
        }
        return;
    }

    if (DMA_getOverflowFlag(SPIB_RX_DMA_CH_BASE))
    {
        gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_ERROR;
        spiB_slave.stat |= _SSS_GET_ERROR;

        reportSlaveError(SPIB_FAULT_SOURCE_DRIVER,
                         (uint16_t)SPIB_DRV_FAULT_DMA_ERROR);

        SPIB_RxDmaClearDone();
        SPIB_RxDmaRestart();
        return;
    }

    if ((eRxFifoLevel > SPI_FIFO_RXEMPTY) &&
        (eRxFifoLevel < SPI_FIFO_RX2))
    {
        if (s_bSpibRxPartialPending == false)
        {
            s_bSpibRxPartialPending = true;
            s_u32SpibRxPartialTicks = U32_UPCNTS;
        }
        else if (getElapsedTicks(s_u32SpibRxPartialTicks) >
                 SPIB_RX_DMA_TIMEOUT_TICKS)
        {
            gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_DONE_TIMEOUT;
            spiB_slave.stat |= _SSS_GET_ERROR;

            reportSlaveError(SPIB_FAULT_SOURCE_DRIVER,
                             (uint16_t)SPIB_DRV_FAULT_DMA_TIMEOUT);

            SPIB_RxDmaClearDone();
            SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
            SPIB_RxDmaRestart();
            return;
        }
    }
    else
    {
        s_bSpibRxPartialPending = false;
    }

    if (getElapsedTicks(s_u32LastRxTicks) > T_2MS)
    {
        if (!s_bSpiClean)
        {
            spiB_slave.u32ResetCount++;

            /* DIAG (B01C): freeze the SPI peripheral state at the moment the
             * watchdog decides to reset -- this is the failure state, before
             * the reset heals it.  Captured only on the FIRST watchdog reset so
             * it is not overwritten by later idle resets. */
            if (spiB_slave.u32ResetCount == 1U)
            {
                g_u16DiagFfrxAtReset = HWREGH(SPIB_SYSTEM_BASE + SPI_O_FFRX);
                g_u16DiagCcrAtReset = HWREGH(SPIB_SYSTEM_BASE + SPI_O_CCR);
                g_u16DiagRxLvlAtReset =
                    (uint16_t)SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE);
                g_u32DiagIntStatusAtReset =
                    SPI_getInterruptStatus(SPIB_SYSTEM_BASE);
            }

            if (s_bTxStatusActive)
            {
                SPIB_TxStatusAbort();
            }

            SPI_disableModule(SPIB_SYSTEM_BASE);
            SPI_enableModule(SPIB_SYSTEM_BASE);
            SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
            SPI_resetTxFIFO(SPIB_SYSTEM_BASE);
            SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RX_OVERRUN);

            pushNullIntoTxD(&s_sSpiParser);

            SPIB_RxDmaRestart();

            spiB_slave.stat &= ~_SSS_GET_ERROR;
            s_bSpiClean = true;
        }
    }
}

static void updateSpibModuleState(void)
{
    if (spiB_slave.stDiag.eHealth == SPIB_HEALTH_OK)
    {
        if (spiB_slave.fsm == _INIT_SPI_AS_SLAVE) {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_INIT;
        } else if (spiB_slave.fsm == _POP_RXD_FROM_SPI) {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_POP_RXD;
        } else {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_WAIT_TIMEOUT;
        }

        spiB_slave.stDiag.stProtocol.eState = (SPIB_PROTOCOL_STATE_e)spiB_slave.ePacketState;

        if (spiB_slave.eFlashState == FLASH_COMMIT_BUSY) {
            spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_COMMITTING;
        } else if (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_BUSY) {
            spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_BUSY;
        } else if (spiB_slave.u16BlockStatus == SPI_BLOCK_STATUS_RECEIVING) {
            spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_RECEIVING;
        } else {
            spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_IDLE;
        }
    }
    else
    {
        spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_INIT;
        spiB_slave.stDiag.stProtocol.eState = SPIB_PROT_STATE_IDLE;
        spiB_slave.stDiag.stService.eState = SPIB_SERV_STATE_IDLE;
    }
}

void runSPIBslave(void)
{
    switch(spiB_slave.fsm) {
    case _INIT_SPI_AS_SLAVE:
        initSPIslave();
        SW_SSS(_POP_RXD_FROM_SPI);
        break;

    case _POP_RXD_FROM_SPI:
        pollReceiveFromSpi();

        {
            SPI_FRAME_t frame;
            U32_PACK adapter;
            while (SPI_FIFO_Pop(&s_fallbackFifo, &frame))
            {
                adapter.u16Address = frame.cmd;
                adapter.u16Data    = frame.data;
                s_sSpiParser.pRdata = &adapter;
                s_sSpiParser.wrfunc = wr32bitsToSpi;
                parseRemoteCommand(&s_sSpiParser);
            }
        }
        s_sSpiParser.wrfunc = wr32bitsToSpi;

        handleBackgroundFlashCommit();

        /* D02_2_1 5.2: background loop keeps the idle TX ping-pong buffer
         * filled with the latest status so a request streams fresh data. */
        SPIB_TxStatusRefresh();
        break;

    default:
        break;
    }

    updateSpibModuleState();
}
