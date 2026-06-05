/*
 * spi_b_slave.c (Preview Sandbox)
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
#include "shareram.h"

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
volatile uint32_t g_u32SpiSlaveLastRequest;
static ST_WR_PARSER s_sSpiParser;
static bool s_bSpibRxDmaArmed = false;
static bool s_bSpibRxPartialPending = false;
static uint32_t s_u32SpibRxPartialTicks = 0U;

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
        uint16_t d0 = gSpibRxRegFrame[0];
        uint16_t d1 = gSpibRxRegFrame[1];

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
    uint16_t *pSrcAddr = (uint16_t *)(SPIB_SYSTEM_BASE + SPI_O_RXBUF);
    uint16_t *pDstAddr = (uint16_t *)&gSpibRxRegFrame[0];

    gSpibRxRegFrame[0] = 0U;
    gSpibRxRegFrame[1] = 0U;
    gSpibRxRegFrameReady = false;
    s_bSpibRxDmaArmed = false;
    s_bSpibRxPartialPending = false;

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

    SPI_clearInterruptStatus(SPIB_SYSTEM_BASE, SPI_INT_RXFF | SPI_INT_RX_OVERRUN);
    SPI_enableInterrupt(SPIB_SYSTEM_BASE, SPI_INT_RXFF);

    DMA_enableTrigger(SPIB_RX_DMA_CH_BASE);
    DMA_startChannel(SPIB_RX_DMA_CH_BASE);
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
    s_bSpibRxPartialPending = false;
}

void SPIB_RxDmaResetDebugCounters(void)
{
    gSpibRxDmaDoneCount = 0U;
    gSpibRxParseOkCount = 0U;
    gSpibRxParseFailCount = 0U;
    gSpibRxDmaRestartCount = 0U;
    gSpibRxErrorFlags = 0U;
}

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

/* Removed inline for code size saving */
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

    g_u32DebugLastTx = ((uint32_t)u16RespAddr << 16) | u16Data;
    g_u32DebugLastValidResponse = g_u32DebugLastTx;
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16RespAddr);
    SPI_writeDataNonBlocking(SPIB_SYSTEM_BASE, u16Data);
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
    uint16_t u16RespData;

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

    case Spi_Block_Status_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockStatus);
        break;

    case Spi_Block_Write_Index_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockWriteIndex);
        break;

    case Spi_Block_CheckSum_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockChecksum);
        break;

    case Spi_Block_Expected_CheckSum_spi_addr:
        writeDirectSpiResponse(u16Address, spiB_slave.u16BlockExpectedChecksum);
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

static bool SPIB_ParseLegacyRegFrame(uint16_t u16Cmd, uint16_t u16Data)
{
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

        // Increment success packet count in stComm
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

// Background Flash Commit Simulation (0us Non-blocking FSM)
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
        // Increment service success count in stComm
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

// ============================================================================
// Public API Implementations
// ============================================================================

void initSPIslave(void)
{
    uint16_t u16Idx;

    DMA_initController();
    initNativeSpiSlave(SPIB_SYSTEM_BASE);
    initSpibRxDma();

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

    /* Initialize diagnostics in stDiag */
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

    SET_SSS_STAT(_INIT_SSS_READY);
}

void pollReceiveFromSpi(void)
{
     uint32_t u32SpiIntStatus = SPI_getInterruptStatus(SPIB_SYSTEM_BASE);
     SPI_RxFIFOLevel eRxFifoLevel = SPI_getRxFIFOStatus(SPIB_SYSTEM_BASE);

     if ((u32SpiIntStatus & (SPI_INT_RX_OVERRUN | SPI_INT_RXFF_OVERFLOW)) != 0U)
     {
         gSpibRxErrorFlags |= SPIB_RX_ERR_RX_FIFO_OVERFLOW;
         spiB_slave.stat |= _SSS_GET_ERROR;
         reportSlaveError(SPIB_FAULT_SOURCE_DRIVER, (uint16_t)SPIB_DRV_FAULT_FIFO_OVERFLOW);
         SPIB_RxDmaClearDone();
         SPI_resetRxFIFO(SPIB_SYSTEM_BASE);
         SPIB_RxDmaRestart();
         return;
     }

     if (DMA_getOverflowFlag(SPIB_RX_DMA_CH_BASE))
     {
         gSpibRxErrorFlags |= SPIB_RX_ERR_DMA_ERROR;
         spiB_slave.stat |= _SSS_GET_ERROR;
         reportSlaveError(SPIB_FAULT_SOURCE_DRIVER, (uint16_t)SPIB_DRV_FAULT_DMA_ERROR);
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

     if (SPIB_RxDmaIsDone())
     {
         uint16_t u16Cmd;
         uint16_t u16Data;
         bool bParseOk;

         gSpibRxRegFrameReady = true;
         gSpibRxDmaDoneCount++;
         spiB_slave.stDiag.stDriver.stComm.u32RxTotal++;

         u16Cmd = gSpibRxRegFrame[0];
         u16Data = gSpibRxRegFrame[1];
         if (u16Cmd != 0xFFFFU)
         {
             g_u32SpiSlaveLastRequest =
                 ((uint32_t)u16Cmd << 16) | (uint32_t)u16Data;
         }

         bParseOk = SPIB_ParseRegFrame(u16Cmd, u16Data);
         if (bParseOk)
         {
             gSpibRxParseOkCount++;
         }
         else
         {
             gSpibRxParseFailCount++;
             gSpibRxErrorFlags |= SPIB_RX_ERR_FRAME_PARSE_FAIL;
             reportSlaveError(SPIB_FAULT_SOURCE_PROTOCOL, (uint16_t)SPIB_PROT_FAULT_FRAME_PARSE_FAIL);
         }

         SPIB_RxDmaClearDone();
         SPIB_RxDmaRestart();
     }

     uint32_t u32Elapsed = getElapsedTicks(s_u32LastRxTicks);

     if (u32Elapsed > T_2MS)
     {
         if (!s_bSpiClean)
         {
             spiB_slave.u32ResetCount++;
             spiB_slave.u16LastRcntBeforeReset = spiB_slave.u16Rcnt;
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
        /* Map FSM states to Driver State */
        if (spiB_slave.fsm == _INIT_SPI_AS_SLAVE) {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_INIT;
        } else if (spiB_slave.fsm == _POP_RXD_FROM_SPI) {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_POP_RXD;
        } else {
            spiB_slave.stDiag.stDriver.eState = SPIB_DRV_STATE_WAIT_TIMEOUT;
        }

        /* Map packet states to Protocol State */
        spiB_slave.stDiag.stProtocol.eState = (SPIB_PROTOCOL_STATE_e)spiB_slave.ePacketState;

        /* Map block status to Service State */
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

        while(0 < spiB_slave.u16Rcnt) {
            if(spiB_slave.u16Rpop != spiB_slave.u16Rpush) {
                s_sSpiParser.pRdata = &spiB_slave.u32RxD[spiB_slave.u16Rpop];
                s_sSpiParser.wrfunc = wr32bitsToSpi;
                parseRemoteCommand(&s_sSpiParser);
            }

            spiB_slave.u16Rpop++;
            if(SIZE_OF_SSS_BUFFER == spiB_slave.u16Rpop) spiB_slave.u16Rpop = 0;
            spiB_slave.u16Rcnt--;
        }
        s_sSpiParser.wrfunc = wr32bitsToSpi;

        handleBackgroundFlashCommit();
        break;

    case _WAIT_FOR_SPI_TIMEOUT:
        break;

    default:
        break;
    }

    updateSpibModuleState();
}
