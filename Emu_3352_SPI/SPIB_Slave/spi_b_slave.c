/*
 * spi_b_slave.c
 *
 * Refactored from test_spi_slave.c
 * Migrated to CPU1 for Host communication (SPI-B)
 * ASCII-ONLY formatted for MS950 compilation safety.
 */

#include "driverlib.h"
#include "device.h"
#include "inc/hw_spi.h"
#include "spi_slave.h"
#include "common.h"
#include "shareram.h"  // after driverlib so float32_t is already defined

volatile uint16_t OUTPUT_ON;

#pragma DATA_SECTION(gSpibRxRegFrame, "spib_slave_state")
#pragma DATA_ALIGN(gSpibRxRegFrame, 2)
volatile uint16_t gSpibRxRegFrame[SPIB_RX_REG_WORDS];
volatile bool gSpibRxRegFrameReady;

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

// Global debug variables
volatile uint32_t g_u32DebugLastTx;
volatile uint32_t g_u32DebugLastValidResponse;
static ST_WR_PARSER s_sSpiParser;
static bool s_bSpibRxDmaArmed = false;
static uint32_t s_u32SpibRxDmaArmTicks = 0U;

#pragma DATA_SECTION(g_u16SpiBlockRam, "spib_block_ram")
volatile uint16_t g_u16SpiBlockRam[SIZE_OF_SPI_BLOCK_RAM];

#define SPI_BLOCK_STATUS_IDLE      0x0000U
#define SPI_BLOCK_STATUS_RECEIVING 0x0001U
#define SPI_BLOCK_STATUS_BUSY      0x0002U
#define SPI_BLOCK_STATUS_READY     0x0003U
#define SPI_BLOCK_STATUS_OVERFLOW  0x8000U
#define SPI_BLOCK_STATUS_ERROR     0x8001U

#define SPIB_RX_DMA_CH_BASE        DMA_CH3_BASE
#define SPIB_RX_DMA_TIMEOUT_TICKS  T_2MS

#pragma DATA_SECTION(spiB_slave, "spib_slave_state")
ST_SPI_SLAVE spiB_slave = {
    .fsm = _INIT_SPI_AS_SLAVE,
    .stat = _NO_STAT_OF_SSS,
    .u16Rpush = 0,
    .u16Rpop = 0,
    .u16Rcnt = 0,
};

#define SW_SSS(x)         FG_SWTO(x, spiB_slave.fsm)
#define SET_SSS_STAT(x)   FG_SET(x, spiB_slave.stat)

#ifdef _FLASH
#pragma SET_CODE_SECTION(".TI.ramfunc")
#endif //_FLASH

// ============================================================================
// Silence timeout variables
// ============================================================================
static uint32_t s_u32LastRxTicks = 0U;
static bool s_bSpiClean = false; // Initialize to false so we perform a reset 2ms after boot to clear startup noise

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
// Private Helper Initialization & Physical Layer Interface
// ============================================================================

void initNativeSpiSlave(uint32_t u32Base)
{
    // Must temporarily reset SPI before configuration
    SPI_disableModule(u32Base);

    // FIFO configuration
    SPI_enableFIFO(u32Base);
    SPI_setFIFOInterruptLevel(u32Base, SPI_FIFO_TX2, SPI_FIFO_RX2);
    SPI_clearInterruptStatus(u32Base, SPI_INT_RXFF | SPI_INT_TXFF);

    // Enable SPI
    SPI_enableModule(u32Base);

    SPI_resetTxFIFO(u32Base);
    SPI_resetRxFIFO(u32Base);
}

static void initSpibRxDma(void)
{
    uint16_t *pSrcAddr = (uint16_t *)(SPIB_SYSTEM_BASE + SPI_O_RXBUF);
    uint16_t *pDstAddr = (uint16_t *)&gSpibRxRegFrame[0];

    gSpibRxRegFrame[0] = 0U;
    gSpibRxRegFrame[1] = 0U;
    gSpibRxRegFrameReady = false;
    s_bSpibRxDmaArmed = false;

    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_disableInterrupt(SPIB_RX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);

    DMA_configAddresses(SPIB_RX_DMA_CH_BASE, pDstAddr, pSrcAddr);
    DMA_configBurst(SPIB_RX_DMA_CH_BASE, SPIB_RX_REG_WORDS, 0, 1);
    DMA_configTransfer(SPIB_RX_DMA_CH_BASE, 1U, 0, 0);
    DMA_configMode(SPIB_RX_DMA_CH_BASE, DMA_TRIGGER_SPIBRX,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);
    DMA_setEmulationMode(DMA_EMULATION_FREE_RUN);

    /* RXFF interrupt is used only as the DMA trigger source; no SPIB RX ISR is registered/enabled. */
    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
    SPI_enableInterrupt(SPIB_SYSTEM_BASE, SPI_INT_RXFF);

    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);
    s_bSpibRxDmaArmed = true;
    s_u32SpibRxDmaArmTicks = U32_UPCNTS;
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
    gSpibRxRegFrameReady = false;
    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);
    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
}

void SPIB_RxDmaRestart(void)
{
    uint16_t *pSrcAddr = (uint16_t *)(SPIB_SYSTEM_BASE + SPI_O_RXBUF);
    uint16_t *pDstAddr = (uint16_t *)&gSpibRxRegFrame[0];

    gSpibRxDmaRestartCount++;
    gSpibRxRegFrameReady = false;
    DMA_disableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_stopChannel(SPIB_RX_DMA_CH_BASE);
    DMA_clearTriggerFlag(SPIB_RX_DMA_CH_BASE);
    DMA_clearErrorFlag(SPIB_RX_DMA_CH_BASE);

    DMA_configAddresses(SPIB_RX_DMA_CH_BASE, pDstAddr, pSrcAddr);
    DMA_configBurst(SPIB_RX_DMA_CH_BASE, SPIB_RX_REG_WORDS, 0, 1);
    DMA_configTransfer(SPIB_RX_DMA_CH_BASE, 1U, 0, 0);

    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);
    s_bSpibRxDmaArmed = true;
    s_u32SpibRxDmaArmTicks = U32_UPCNTS;
}

void SPIB_RxDmaResetDebugCounters(void)
{
    gSpibRxDmaDoneCount = 0U;
    gSpibRxParseOkCount = 0U;
    gSpibRxParseFailCount = 0U;
    gSpibRxDmaRestartCount = 0U;
    gSpibRxErrorFlags = 0U;
}

// Physical transmit split API: split 32-bit into two 16-bit words and write to FIFO
int16_t wr32bitsToSpi(HAL_U32PACK pHalPack) {
    g_u32DebugLastTx = pHalPack->u32All;
    if (pHalPack->u16Address != 0xFFFFU)
    {
        g_u32DebugLastValidResponse = pHalPack->u32All;
    }

    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All >> 16));
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All & 0xFFFF));

    return 4;
}

int16_t rd32bitsFromSpi(HAL_U32PACK pHalPack) {
    return 4;
}

static inline uint16_t calcSpiByteChecksum(uint16_t u16Data)
{
    return (uint16_t)((u16Data & 0x00FFU) + (u16Data >> 8));
}

static inline void writeDirectSpiResponse(uint16_t u16Address, uint16_t u16Data)
{
    uint16_t u16RespAddr = (uint16_t)(u16Address + calcSpiByteChecksum(u16Data));

    g_u32DebugLastTx = ((uint32_t)u16RespAddr << 16) | u16Data;
    g_u32DebugLastValidResponse = g_u32DebugLastTx;
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16RespAddr);
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16Data);
}

static inline uint16_t tryHandleBlockPath(uint16_t u16Address, uint16_t u16Data)
{
    if ((u16Address >= Spi_Block_Data_Base_spi_addr) &&
        (u16Address <= Spi_Block_Data_Last_spi_addr))
    {
        uint16_t u16Index = (uint16_t)(u16Address - Spi_Block_Data_Base_spi_addr);

        /* Busy Guard: reject any new block write if background flash commit is in progress */
        if (spiB_slave.eFlashState != FLASH_COMMIT_IDLE)
        {
            spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
            writeDirectSpiResponse(u16Address, spiB_slave.u16BlockStatus);
            return 1U;
        }

        if (u16Index < SIZE_OF_SPI_BLOCK_RAM)
        {
            if (u16Index == 0U)
            {
                spiB_slave.u16BlockReady = 0U;
                spiB_slave.u16BlockChecksum = 0U;
                spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_RECEIVING;
            }

            g_u16SpiBlockRam[u16Index] = u16Data;
            spiB_slave.u16BlockWriteIndex = (uint16_t)(u16Index + 1U);
            spiB_slave.u16BlockChecksum =
                (uint16_t)(spiB_slave.u16BlockChecksum + calcSpiByteChecksum(u16Data));
            spiB_slave.u32BlockRxCount++;
            writeDirectSpiResponse(u16Address, u16Data);
        }
        else
        {
            spiB_slave.u32BlockOverflowCount++;
            spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_OVERFLOW;
            writeDirectSpiResponse(u16Address, spiB_slave.u16BlockStatus);
        }
        return 1U;
    }

    if (u16Address == Spi_Block_End_spi_addr)
    {
        spiB_slave.u16BlockExpectedLen = (u16Data == 0U) ?
            spiB_slave.u16BlockWriteIndex : u16Data;
        
        if (spiB_slave.u16BlockStatus != SPI_BLOCK_STATUS_OVERFLOW)
        {
            /* Fix: Only set pending flag and status, let background FSM manage transition */
            spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_BUSY;
            spiB_slave.u16FlashCommitPending = 1U;
        }

        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockStatus);
        return 1U;
    }

    return 0U;
}

static inline uint16_t tryHandleFastPath(uint16_t u16Address, uint16_t u16Data)
{
    uint16_t u16RespData;

    /* Filter out Null words, do nothing but report handled to prevent fallback parsing */
    if (u16Address == 0xFFFFU)
    {
        return 1U;
    }

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

    case CPU2_Version_spi_addr:
        writeDirectSpiResponse(u16Address, DSP_FW_Version_Code_CPU2);
        break;

    case Startup_State_spi_addr:
        writeDirectSpiResponse(u16Address, startupFlags);
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

bool SPIB_ParseRegFrame(uint16_t u16Cmd, uint16_t u16Data)
{
    s_u32LastRxTicks = U32_UPCNTS;
    s_bSpiClean = false;

    if (tryHandleFastPath(u16Cmd, u16Data) == 1U)
    {
        return true;
    }

    spiB_slave.u32FallbackPathCount++;
    if (SIZE_OF_SSS_BUFFER > spiB_slave.u16Rcnt)
    {
        spiB_slave.u32RxD[spiB_slave.u16Rpush].u32All =
            ((uint32_t)u16Cmd << 16) | u16Data;

        spiB_slave.u16Rpush++;
        if (SIZE_OF_SSS_BUFFER == spiB_slave.u16Rpush)
        {
            spiB_slave.u16Rpush = 0U;
        }

        spiB_slave.u16Rcnt++;
        if (spiB_slave.u16Rcnt > spiB_slave.u32MaxRcnt)
        {
            spiB_slave.u32MaxRcnt = spiB_slave.u16Rcnt;
        }

        return true;
    }
    else
    {
        spiB_slave.stat |= _SSS_GET_ERROR;
        return false;
    }
}

// ============================================================================
// Background Flash Commit Simulation (0us Non-blocking FSM)
// ============================================================================
static inline void handleBackgroundFlashCommit(void)
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
        /* Simulate background non-blocking write: +10% progress per call */
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
        break;

    case FLASH_COMMIT_ERROR:
        spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_ERROR;
        spiB_slave.u16FlashCommitPending = 0U;
        spiB_slave.eFlashState = FLASH_COMMIT_IDLE;
        break;

    default:
        spiB_slave.eFlashState = FLASH_COMMIT_IDLE;
        break;
    }
}

// ============================================================================
// Public API Implementations
// ============================================================================

void initSPIslave(void)
{
    uint16_t u16Idx;

    initNativeSpiSlave(SPIB_SYSTEM_BASE);
    initSpibRxDma();

    /* Initialize block RAM to zero to prevent startup garbage noise */
    for (u16Idx = 0U; u16Idx < SIZE_OF_SPI_BLOCK_RAM; u16Idx++)
    {
        g_u16SpiBlockRam[u16Idx] = 0U;
    }

    s_sSpiParser = (ST_WR_PARSER) {
      .wrfunc = wr32bitsToSpi,
      .rdfunc = rd32bitsFromSpi,
      .pRdata = (HAL_U32PACK)&spiB_slave.u32RxD[0],
    };

    pushNullIntoTxD(&s_sSpiParser);

    /* Diagnostic fields initialization */
    spiB_slave.u32ResetCount = 0U;
    spiB_slave.u32MaxRcnt = 0U;
    spiB_slave.u16LastRcntBeforeReset = 0U;
    spiB_slave.u32FastPathCount = 0U;
    spiB_slave.u32FallbackPathCount = 0U;
    spiB_slave.u32BlockRxCount = 0U;
    spiB_slave.u32BlockOverflowCount = 0U;
    spiB_slave.u16BlockReady = 0U;
    spiB_slave.u16BlockWriteIndex = 0U;
    spiB_slave.u16BlockExpectedLen = 0U;
    spiB_slave.u16BlockChecksum = 0U;
    spiB_slave.u16BlockStatus = SPI_BLOCK_STATUS_IDLE;

    /* Background commit variables initialization */
    spiB_slave.u16FlashCommitPending = 0U;
    spiB_slave.u16BlockProgress = 0U;
    spiB_slave.eFlashState = FLASH_COMMIT_IDLE;

    SET_SSS_STAT(_INIT_SSS_READY);
}

void pollReceiveFromSpi(void)
{
     uint32_t u32SpiIntStatus = SPI_getInterruptStatus(SPIB_SYSTEM_BASE);

     if ((u32SpiIntStatus & (SPI_INT_RX_OVERRUN | SPI_INT_RXFF_OVERFLOW)) != 0U)
     {
         gSpibRxErrorFlags |= SPIB_RX_ERR_RX_FIFO_OVERFLOW;
         spiB_slave.stat |= _SSS_GET_ERROR;
         SPIB_RxDmaClearDone();
         SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
         SPIB_RxDmaRestart();
         return;
     }

     if (DMA_getOverflowFlag(SPIB_RX_DMA_CH_BASE))
     {
         gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_ERROR;
         spiB_slave.stat |= _SSS_GET_ERROR;
         SPIB_RxDmaClearDone();
         SPIB_RxDmaRestart();
         return;
     }

     if ((SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE) > SPI_FIFO_RXEMPTY) &&
         (getElapsedTicks(s_u32SpibRxDmaArmTicks) > SPIB_RX_DMA_TIMEOUT_TICKS))
     {
         gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_DONE_TIMEOUT;
         spiB_slave.stat |= _SSS_GET_ERROR;
         SPIB_RxDmaClearDone();
         SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
         SPIB_RxDmaRestart();
         return;
     }

     if (SPIB_RxDmaIsDone())
     {
         uint16_t u16Cmd;
         uint16_t u16Data;
         bool bParseOk;

         gSpibRxRegFrameReady = true;
         gSpibRxDmaDoneCount++;

         u16Cmd = gSpibRxRegFrame[0];
         u16Data = gSpibRxRegFrame[1];

         bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);
         if (bParseOk)
         {
             gSpibRxParseOkCount++;
         }
         else
         {
             gSpibRxParseFailCount++;
             gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL;
         }

         SPIB_RxDmaClearDone();
         SPIB_RxDmaRestart();
     }

     // Silence timeout auto-recovery
     uint32_t u32Elapsed = getElapsedTicks(s_u32LastRxTicks);

     if (u32Elapsed > T_2MS)
     {
         if (!s_bSpiClean)
         {
             spiB_slave.u32ResetCount++;
             spiB_slave.u16LastRcntBeforeReset = spiB_slave.u16Rcnt;
             // Perform full hardware SPI reset on timeout to restore alignment
             SPI_disableModule(SPIB_SYSTEM_BASE);
             SPI_enableModule(SPIB_SYSTEM_BASE);
             SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
             SPI_resetTxFIFO(SPIB_SYSTEM_BASE);
             SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RX_OVERRUN);
             pushNullIntoTxD(&s_sSpiParser);
             SPIB_RxDmaRestart();

             // Clear error flag during silence reset recovery
             spiB_slave.stat &= ~_SSS_GET_ERROR;

             s_bSpiClean = true;
         }
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

        // Process all pending packets in the ring buffer in a single main loop pass
        while(0 < spiB_slave.u16Rcnt) {
            if(spiB_slave.u16Rpop != spiB_slave.u16Rpush) {
                s_sSpiParser.pRdata = &spiB_slave.u32RxD[spiB_slave.u16Rpop];

                // Block pipeline mode expects every received packet to produce one queued response.
                s_sSpiParser.wrfunc = wr32bitsToSpi;

                parseRemoteCommand(&s_sSpiParser);
            }

            spiB_slave.u16Rpop++;
            if(SIZE_OF_SSS_BUFFER == spiB_slave.u16Rpop) spiB_slave.u16Rpop = 0;

            spiB_slave.u16Rcnt--;
        }
        s_sSpiParser.wrfunc = wr32bitsToSpi;

        /* Execute non-blocking background Flash commit steps */
        handleBackgroundFlashCommit();
        break;

    case _WAIT_FOR_SPI_TIMEOUT:
        break;

    default:
        break;
    }
}

#ifdef _FLASH
#pragma SET_CODE_SECTION()
#endif //_FLASH
