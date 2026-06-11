/*
 * asr5k_spi_selftest_port.h
 *
 * Portability seam for the SPI self-test framework.
 *
 * 設計目標 (Design goals):
 *   1. 自測引擎 (asr5k_spi_selftest.c) 不直接觸碰 spiA_master / spiB_slave /
 *      g_waveDownload。所有外部相依集中在本檔。
 *   2. 之後 Master / Slave 拆成獨立韌體 (或獨立 CPU) 時:
 *        - MASTER-SIDE API   : 保持本機呼叫。
 *        - SLAVE-SIDE OBSERVE: 改為透過 SPI 讀取遠端診斷暫存器,
 *          只需改寫本檔 (或對應的 port .c),引擎與測試表零修改。
 *   3. 所有協議常數 (位址、page state 編碼) 單一出處,可由建置系統
 *      以 -D 覆寫,避免 magic number 散落。
 */

#ifndef ASR5K_SPI_SELFTEST_PORT_H_
#define ASR5K_SPI_SELFTEST_PORT_H_

#include <stdint.h>
#include "board.h"        /* device typedefs (float32_t 等) 必須先於  */
#include "driverlib.h"    /* 模組標頭,否則 shareram.h 會缺型別定義 */
#include "SPIA_Master/SPI_master.h"
#include "SPIB_Slave/spi_slave.h"
#include "SPIB_Slave/wave_download.h"   /* WAVE_*_ADDR register map +
                                           wave page metadata             */

/* ========================================================================
 * Wave-download register map
 * 位址巨集 (WAVE_PAGE_SELECT_ADDR / WAVE_DOWNLOAD_CTRL_ADDR /
 * WAVE_PAGE_STATUS_ADDR / WAVE_VALIDATE_ADDR / WAVE_ACTIVATE_ADDR)
 * 一律取自專案標頭 wave_download.h / cmd_id.h,本檔不提供 fallback,
 * 避免與協議定義不同步。
 * ======================================================================== */

#define WAVE_WINDOW_BASE_ADDR      0x3000U  /* Test6: sample write window    */
#define WAVE_WINDOW_LAST_ADDR      0x3FFFU
#define WAVE_PAGE_SAMPLE_COUNT     4096U

/* ------------------------------------------------------------------------
 * Wave page state encoding.
 * 若 slave 端 enum 變更,於建置選項以 -D 覆寫,不要改引擎。
 * ------------------------------------------------------------------------ */
#ifndef WAVE_PAGE_STATE_IDLE
#define WAVE_PAGE_STATE_IDLE               0U
#endif
#ifndef WAVE_PAGE_STATE_DOWNLOADING
#define WAVE_PAGE_STATE_DOWNLOADING        2U
#endif
#ifndef WAVE_PAGE_STATE_DOWNLOAD_COMPLETE
#define WAVE_PAGE_STATE_DOWNLOAD_COMPLETE  3U
#endif
#ifndef WAVE_PAGE_STATE_VALID
#define WAVE_PAGE_STATE_VALID              4U
#endif
#ifndef WAVE_PAGE_STATE_INVALID
#define WAVE_PAGE_STATE_INVALID            5U
#endif
#ifndef WAVE_PAGE_STATE_LOCKED
#define WAVE_PAGE_STATE_LOCKED             6U
#endif

/* ========================================================================
 * MASTER-SIDE API
 * Master/Slave 拆分後仍為本機呼叫,預期不變。
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
 * 拆分後改為遠端診斷讀取 (SPI register read / debug channel)。
 * 引擎只透過這些 accessor 取值,因此替換實作即可移植。
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
 * 拆分後: 這些 metadata 應由 slave 以唯讀診斷暫存器匯出,
 * 屆時以 SPI read 取代直接結構存取。
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
 * SPIB_DebugCaptureSelfTest() 目前專案中無實作,預設關閉以免連結失敗。
 * 若 slave 模組提供該函式,在建置選項加
 *   -DASR5K_SELFTEST_ENABLE_DEBUG_CAPTURE=1
 * 即可啟用,引擎不需修改。
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
