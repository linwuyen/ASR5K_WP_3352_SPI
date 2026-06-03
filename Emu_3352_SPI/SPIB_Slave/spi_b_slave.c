/*
 * spi_b_slave.c
 *
 * Refactored from test_spi_slave.c
 * Migrated to CPU1 for Host communication (SPI-B)
 * ASCII-ONLY formatted for MS950 compilation safety.
 */

#include "driverlib.h"
#include "device.h"
#include "spi_slave.h"
#include "common.h"
#include "shareram.h"  // after driverlib so float32_t is already defined

uint16_t OUTPUT_ON;

// ============================================================================
// 64-bit Dual-Track Experimental FSM & Global Monitor Definitions
// (0us Blocking Yield-Polling Core)
// ============================================================================
#ifdef SPI_ENABLE_U64_TEST
typedef enum {
    RX_STATE_IDLE = 0,
    RX_STATE_WAIT_U64_BODY
} E_RX_STATE;

static E_RX_STATE s_eRxState = RX_STATE_IDLE;
static uint16_t s_u16HighHeader = 0U;
static uint32_t s_u32RxTimeoutCnt = 0U;

// Slave RX timeout threshold (prevents permanent hang due to packet loss)
#define SLAVE_RX_TIMEOUT_LIMIT  100000U

// Inspect in CCS Expressions to verify 64-bit float is atomically saved into slave memory
volatile float g_f32SlaveRxTestVal = 0.0f;
#endif /* SPI_ENABLE_U64_TEST */

// Global debug variables
volatile uint32_t g_u32DebugLastTx = 0U;
static ST_WR_PARSER s_sSpiParser = DEFAULT_WR_PARSER;

#pragma DATA_SECTION(g_u16SpiBlockRam, "spib_block_ram")
volatile uint16_t g_u16SpiBlockRam[SIZE_OF_SPI_BLOCK_RAM];

#define SPI_BLOCK_STATUS_IDLE      0x0000U
#define SPI_BLOCK_STATUS_RECEIVING 0x0001U
#define SPI_BLOCK_STATUS_BUSY      0x0002U
#define SPI_BLOCK_STATUS_READY     0x0003U
#define SPI_BLOCK_STATUS_OVERFLOW  0x8000U
#define SPI_BLOCK_STATUS_ERROR     0x8001U

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

// Physical transmit split API: split 32-bit into two 16-bit words and write to FIFO
int16_t wr32bitsToSpi(HAL_U32PACK pHalPack) {
    g_u32DebugLastTx = pHalPack->u32All;

    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All >> 16));
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(pHalPack->u32All & 0xFFFF));

    return 4;
}

int16_t rd32bitsFromSpi(HAL_U32PACK pHalPack) {
    return 4;
}

#ifdef SPI_ENABLE_U64_TEST
static inline void parseU64PackDirect(uint16_t u16Word0, uint16_t u16Word1, uint16_t u16Word2, uint16_t u16Word3)
{
    uint16_t u16Header = u16Word0;  /* Word 1: addr | 0x8000 */
    uint16_t u16Low    = u16Word1;  /* Word 2: float low 16 bits */
    uint16_t u16High   = u16Word2;  /* Word 3: float high 16 bits */
    uint16_t u16Chk    = u16Word3;  /* Word 4: checksum */

    /* Atomic lossless reconstruction of float32 */
    uint32_t u32Raw    = ((uint32_t)u16High << 16) | u16Low;
    volatile float f32Val = *(float*)&u32Raw;

    /* If target address matches 64-bit test register 0x09F0U, write to slave observation variable */
    uint16_t u16AddrOnly = u16Header & 0x7FFFU;
    if (u16AddrOnly == 0x09F0U)
    {
        g_f32SlaveRxTestVal = f32Val;
    }

    /* Calculate Byte-sum checksum */
    uint16_t u16ChkCalc = (uint16_t)( (u32Raw        & 0xFFU)
                                    + ((u32Raw >>  8) & 0xFFU)
                                    + ((u32Raw >> 16) & 0xFFU)
                                    + ((u32Raw >> 24) & 0xFFU) );
    (void)u16Chk;

    /* Echo the received value back 100% identically */
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, (uint16_t)(u16Header & 0x7FFFU));
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16Low);
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16High);
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16ChkCalc);
}
#endif /* SPI_ENABLE_U64_TEST */

static inline uint16_t calcSpiByteChecksum(uint16_t u16Data)
{
    return (uint16_t)((u16Data & 0x00FFU) + (u16Data >> 8));
}

static inline void writeDirectSpiResponse(uint16_t u16Address, uint16_t u16Data)
{
    uint16_t u16RespAddr = (uint16_t)(u16Address + calcSpiByteChecksum(u16Data));

    g_u32DebugLastTx = ((uint32_t)u16RespAddr << 16) | u16Data;
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
    spiB_slave.u32U64TimeoutCount = 0U;
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
     uint16_t u16LoopCnt = 0U;

#ifdef SPI_ENABLE_U64_TEST
     // Slave 64-bit status timeout monitor
     if (s_eRxState == RX_STATE_WAIT_U64_BODY)
     {
         s_u32RxTimeoutCnt++;
         if (s_u32RxTimeoutCnt > SLAVE_RX_TIMEOUT_LIMIT)
         {
             spiB_slave.u32U64TimeoutCount++;
             spiB_slave.u16LastRcntBeforeReset = spiB_slave.u16Rcnt;
             // Perform full hardware SPI reset on timeout to restore alignment
             SPI_disableModule(SPIB_SYSTEM_BASE);
             SPI_enableModule(SPIB_SYSTEM_BASE);
             SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
             SPI_resetTxFIFO(SPIB_SYSTEM_BASE);
             pushNullIntoTxD(&s_sSpiParser);
             s_eRxState = RX_STATE_IDLE;
             s_u32RxTimeoutCnt = 0U;
             spiB_slave.stat |= _SSS_GET_ERROR;
             return;
         }
     }
#endif /* SPI_ENABLE_U64_TEST */

     // State 1: IDLE - detect header and dispatch to appropriate path
     // Bounded loop: Max hardware FIFO depth is 16 words (8 packets of 32-bit/2-word)
     for (u16LoopCnt = 0U; u16LoopCnt < 8U; u16LoopCnt++)
     {
         if (SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE) < SPI_FIFO_RX2)
         {
             break;
         }

#ifdef SPI_ENABLE_U64_TEST
         if (s_eRxState != RX_STATE_IDLE)
         {
             break;
         }
#endif

         // Update activity ticks
         s_u32LastRxTicks = U32_UPCNTS;
         s_bSpiClean = false;

#ifdef SPI_ENABLE_U64_TEST
         if (s_eRxState == RX_STATE_IDLE)
         {
             /* Read Word 1 for high-frequency filtering */
             s_u16HighHeader = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);

             if ((s_u16HighHeader & 0x8000U) && ((s_u16HighHeader & 0x7FFFU) == 0x09F0U))
             {
                 /* Detected 64-bit characteristic flag (bit15=1 and address=0x09F0): transition state, exit immediately, 0us CPU blocking overhead */
                 s_eRxState = RX_STATE_WAIT_U64_BODY;
                 s_u32RxTimeoutCnt = 0U;
                 return;
             }

             /* Production 32-bit standard packet processing path */
             uint16_t u16Low = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);

             if (tryHandleFastPath(s_u16HighHeader, u16Low) == 1U)
             {
                 continue;
             }

             spiB_slave.u32FallbackPathCount++;
             if (SIZE_OF_SSS_BUFFER > spiB_slave.u16Rcnt)
             {
                 spiB_slave.u32RxD[spiB_slave.u16Rpush].u32All = ((uint32_t)s_u16HighHeader << 16) | u16Low;
                 spiB_slave.u16Rpush++;
                 if (SIZE_OF_SSS_BUFFER == spiB_slave.u16Rpush)  spiB_slave.u16Rpush = 0;
                 spiB_slave.u16Rcnt++;
                 if (spiB_slave.u16Rcnt > spiB_slave.u32MaxRcnt)
                 {
                     spiB_slave.u32MaxRcnt = spiB_slave.u16Rcnt;
                 }
             }
             else
             {
                 (void)SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
                 spiB_slave.stat |= _SSS_GET_ERROR;
             }
         }
#else
         /* Production-only 32-bit native path (no branching, maximum performance) */
         if (SIZE_OF_SSS_BUFFER > spiB_slave.u16Rcnt)
         {
             uint16_t u16High = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
             uint16_t u16Low = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);

             if (tryHandleFastPath(u16High, u16Low) == 1U)
             {
                 continue;
             }

             spiB_slave.u32FallbackPathCount++;
             spiB_slave.u32RxD[spiB_slave.u16Rpush].u32All = ((uint32_t)u16High << 16) | u16Low;

             spiB_slave.u16Rpush++;
             if (SIZE_OF_SSS_BUFFER == spiB_slave.u16Rpush)  spiB_slave.u16Rpush = 0;
             spiB_slave.u16Rcnt++;
             if (spiB_slave.u16Rcnt > spiB_slave.u32MaxRcnt)
             {
                 spiB_slave.u32MaxRcnt = spiB_slave.u16Rcnt;
             }
         }
         else
         {
             (void)SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
             (void)SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
             spiB_slave.stat |= _SSS_GET_ERROR;
         }
#endif /* SPI_ENABLE_U64_TEST */
     }

#ifdef SPI_ENABLE_U64_TEST
     // State 2: Non-blocking yield waiting for remaining 3 words of 64-bit packet
     if (s_eRxState == RX_STATE_WAIT_U64_BODY)
     {
         if (SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE) >= 3U)
         {
             // Update activity ticks
             s_u32LastRxTicks = U32_UPCNTS;
             s_bSpiClean = false;

             uint16_t u16Low   = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
             uint16_t u16Word2 = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);
             uint16_t u16Word3 = SPI_readDataNonBlocking(SPIB_SYSTEM_BASE);

             /* All 3 words received, parse directly in background without polluting 32-bit software ring buffer */
             parseU64PackDirect(s_u16HighHeader, u16Low, u16Word2, u16Word3);

             // Return to IDLE state
             s_eRxState = RX_STATE_IDLE;
             s_u32RxTimeoutCnt = 0U;
         }
     }
#endif /* SPI_ENABLE_U64_TEST */

     // Silence timeout auto-recovery
     uint32_t u32Now = U32_UPCNTS;
     uint32_t u32Elapsed;
     if (u32Now >= s_u32LastRxTicks)
     {
         u32Elapsed = u32Now - s_u32LastRxTicks;
     }
     else
     {
         u32Elapsed = (SW_TIMER - s_u32LastRxTicks) + u32Now;
     }

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

             // Clear error flag during silence reset recovery
             spiB_slave.stat &= ~_SSS_GET_ERROR;

#ifdef SPI_ENABLE_U64_TEST
             s_eRxState = RX_STATE_IDLE;
             s_u32RxTimeoutCnt = 0U;
#endif
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
