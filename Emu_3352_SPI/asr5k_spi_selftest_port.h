/*
 * asr5k_spi_selftest_port.h
 *
 * Portability seam for the SPI self-test framework.
 * ASCII-ONLY for MS950 compilation safety.
 *
 * Design goals:
 *   1. The self-test engine (asr5k_spi_selftest.c) never touches
 *      spiA_master / spiB_slave / g_waveDownload directly.  All external
 *      dependencies are concentrated in this file.
 *   2. When Master / Slave are later split into separate firmware images
 *      (or separate CPUs):
 *        - MASTER-SIDE API   : stays a local call.
 *        - SLAVE-SIDE OBSERVE: becomes a remote diagnostic-register read
 *          over SPI.  Only this file (or its port .c) changes; the engine
 *          and the test table need zero modification.
 *   3. Single source for all protocol constants (addresses, page-state
 *      encoding); the build system may override them with -D so magic
 *      numbers do not spread.
 */

#ifndef ASR5K_SPI_SELFTEST_PORT_H_
#define ASR5K_SPI_SELFTEST_PORT_H_

#include <stdint.h>
#include "board.h"        /* device typedefs (float32_t etc.) must come   */
#include "driverlib.h"    /* before module headers or shareram.h breaks   */
#include "SPIA_Master/SPI_master.h"
#include "SPIB_Slave/spi_slave.h"
#include "SPIB_Slave/wave_download.h"   /* WAVE_*_ADDR register map +
                                           wave page metadata             */

/* ========================================================================
 * Wave-download register map
 * Address macros (WAVE_PAGE_SELECT_ADDR / WAVE_DOWNLOAD_CTRL_ADDR /
 * WAVE_PAGE_STATUS_ADDR / WAVE_VALIDATE_ADDR / WAVE_ACTIVATE_ADDR) always
 * come from project headers wave_download.h / cmd_id.h.  This file adds
 * no fallback, to stay in sync with the protocol definition.
 * ======================================================================== */

#define WAVE_WINDOW_BASE_ADDR      0x3000U  /* Test6: sample write window    */
#define WAVE_WINDOW_LAST_ADDR      0x3FFFU
#define WAVE_PAGE_SAMPLE_COUNT     4096U

/* Wave page state encoding.
 * Use the ST_WAVE_PAGE_STATE enum from wave_download.h (included above):
 *   WAVE_PAGE_STATE_EMPTY = 0, WAVE_PAGE_STATE_DOWNLOADING = 1,
 *   WAVE_PAGE_STATE_DOWNLOAD_COMPLETE = 2, WAVE_PAGE_STATE_VALIDATING = 3,
 *   WAVE_PAGE_STATE_VALID = 4, WAVE_PAGE_STATE_INVALID = 5,
 *   WAVE_PAGE_STATE_LOCKED = 6
 * No fallback #define is provided here: the old fallback values
 * (DOWNLOADING=2, DOWNLOAD_COMPLETE=3) disagreed with the enum and the
 * #ifndef guards silently overrode the enum names, breaking comparisons.
 * The single source of truth is wave_download.h.
 */

/* ========================================================================
 * MASTER-SIDE API
 * Remains a local call after the Master/Slave split; expected unchanged.
 * ======================================================================== */

static inline uint16_t SelfTestPort_MasterStart(SPI_MASTER_TEST_CMD_e eCmd,
                                                uint16_t u16Addr,
                                                uint16_t u16Data)
{
    return startSPIAmasterTest(eCmd, u16Addr, u16Data);
}

static inline SPI_TEST_STATUS_e SelfTestPort_MasterStatus(void)
{
    return spiA_master.stTest.eStatus;
}

static inline uint16_t SelfTestPort_MasterResult16(void)
{
    return spiA_master.stTest.u16Result;
}

static inline uint32_t SelfTestPort_MasterDetail32(void)
{
    return spiA_master.stTest.u32Detail;
}

static inline uint16_t SelfTestPort_MasterFaultCode(void)
{
    return spiA_master.stDiag.u16FaultCode;
}

static inline uint16_t SelfTestPort_MasterFaultSource(void)
{
    return (uint16_t)spiA_master.stDiag.eFaultSource;
}

/* ========================================================================
 * SLAVE-SIDE OBSERVATION
 * After the split this becomes a remote diagnostic read (SPI register
 * read / debug channel).  The engine only reads through these accessors,
 * so swapping the implementation is enough to port it.
 * ======================================================================== */

/* ---- DMA / parser counters -------------------------------------------- */
static inline uint32_t SelfTestPort_SlaveDmaDoneCount(void)    { return gSpibRxDmaDoneCount; }
static inline uint32_t SelfTestPort_SlaveParseOkCount(void)    { return gSpibRxParseOkCount; }
static inline uint32_t SelfTestPort_SlaveParseFailCount(void)  { return gSpibRxParseFailCount; }
static inline uint32_t SelfTestPort_SlaveDmaRestartCount(void) { return gSpibRxDmaRestartCount; }
static inline uint16_t SelfTestPort_SlaveErrorFlags(void)      { return gSpibRxErrorFlags; }

static inline uint16_t SelfTestPort_SlaveFaultCode(void)
{
    return spiB_slave.stDiag.u16FaultCode;
}

static inline uint16_t SelfTestPort_SlaveFaultSource(void)
{
    return (uint16_t)spiB_slave.stDiag.eFaultSource;
}

/* ---- Output relay / power-stage state ---------------------------------- */
static inline uint16_t SelfTestPort_OutputOn(void)
{
    return (uint16_t)OUTPUT_ON;
}

/* ---- Block transfer result (Test4) ------------------------------------- */
static inline uint16_t SelfTestPort_BlockWriteIndex(void)      { return spiB_slave.u16BlockWriteIndex; }
static inline uint16_t SelfTestPort_BlockExpectedLen(void)     { return spiB_slave.u16BlockExpectedLen; }
static inline uint16_t SelfTestPort_BlockChecksum(void)        { return spiB_slave.u16BlockChecksum; }
static inline uint16_t SelfTestPort_BlockExpectedChecksum(void){ return spiB_slave.u16BlockExpectedChecksum; }
static inline uint16_t SelfTestPort_BlockErrorCode(void)       { return spiB_slave.u16BlockErrorCode; }
static inline uint16_t SelfTestPort_BlockProgress(void)        { return spiB_slave.u16BlockProgress; }
static inline uint16_t SelfTestPort_BlockStatus(void)          { return spiB_slave.u16BlockStatus; }

/* ---- Wave download metadata (Test5~9) ----------------------------------
 * After the split the slave should export this metadata via read-only
 * diagnostic registers; SPI reads then replace direct struct access.
 * ------------------------------------------------------------------------ */
static inline uint16_t SelfTestPort_WaveSelectedPage(void)
{
    return g_waveDownload.u16SelectedPage;
}

static inline uint16_t SelfTestPort_WaveSampleCount(uint16_t u16Page)
{
    return g_waveDownload.u16SampleCount[u16Page];
}

static inline uint16_t SelfTestPort_WaveAddressContinuous(uint16_t u16Page)
{
    return (g_waveDownload.bAddressContinuous[u16Page] != 0U) ? 1U : 0U;
}

static inline uint16_t SelfTestPort_WaveLastAddress(uint16_t u16Page)
{
    return g_waveDownload.u16LastAddress[u16Page];
}

static inline uint16_t SelfTestPort_WaveDownloadComplete(uint16_t u16Page)
{
    return (g_waveDownload.bDownloadComplete[u16Page] != 0U) ? 1U : 0U;
}

static inline uint16_t SelfTestPort_WavePageState(uint16_t u16Page)
{
    return g_waveDownload.u16PageState[u16Page];
}

static inline uint16_t SelfTestPort_WaveActivePage(void)
{
    return g_waveDownload.u16ActivePage;
}

static inline uint32_t SelfTestPort_WaveWriteCount(void)
{
    return g_waveDownload.stDiag.u32WriteCount;
}

static inline uint16_t SelfTestPort_WaveReadSample(uint16_t u16Page,
                                                   uint16_t u16Index)
{
    return WaveDownload_ReadSample(u16Page, u16Index);
}

/* ---- Flash commit path (must stay idle during RAM-page tests) ---------- */
static inline uint16_t SelfTestPort_FlashPathIdle(void)
{
    return ((spiB_slave.u16FlashCommitPending == 0U) &&
            (spiB_slave.eFlashState == FLASH_COMMIT_IDLE)) ? 1U : 0U;
}

/* ---- Debug capture hook (optional) --------------------------------------
 * SPIB_DebugCaptureSelfTest() has no implementation in this project, so it
 * is disabled by default to avoid link failures.  If the slave module
 * provides it, enable with build option
 *   -DASR5K_SELFTEST_ENABLE_DEBUG_CAPTURE=1
 * with no engine change required.
 * ------------------------------------------------------------------------ */
#ifndef ASR5K_SELFTEST_ENABLE_DEBUG_CAPTURE
#define ASR5K_SELFTEST_ENABLE_DEBUG_CAPTURE 0
#endif

static inline void SelfTestPort_DebugCapture(void)
{
#if ASR5K_SELFTEST_ENABLE_DEBUG_CAPTURE
    SPIB_DebugCaptureSelfTest();
#endif
}

#endif /* ASR5K_SPI_SELFTEST_PORT_H_ */
