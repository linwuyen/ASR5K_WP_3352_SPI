#include "driverlib.h"
#include "device.h"
#include "wave_download.h"
#include "spi_slave.h"

#pragma DATA_SECTION(g_waveDownload, "spib_slave_state")
ST_WAVE_DOWNLOAD g_waveDownload;

#ifndef ASR5K_HAS_EMIF1_SDRAM
#pragma DATA_SECTION(g_u16FakeSdram0, "fake_sdram_page0")
uint16_t g_u16FakeSdram0[WAVE_SAMPLES_PER_PAGE];
#pragma DATA_SECTION(g_u16FakeSdram1, "fake_sdram_page1")
uint16_t g_u16FakeSdram1[WAVE_SAMPLES_PER_PAGE];
#pragma DATA_SECTION(g_u16FakeSdram2, "fake_sdram_page2")
uint16_t g_u16FakeSdram2[WAVE_SAMPLES_PER_PAGE];

static uint16_t * const g_pFakeSdramPages[3] = {
    g_u16FakeSdram0, g_u16FakeSdram1, g_u16FakeSdram2
};
#endif

void WaveDownload_Init(void)
{
    uint16_t i;
    g_waveDownload.u16SelectedPage = WAVE_PAGE_INVALID;
    g_waveDownload.u16ActivePage = WAVE_PAGE_INVALID;
    
    for (i = 0U; i < WAVE_MAX_PAGES; i++)
    {
        g_waveDownload.u16PageState[i] = (uint16_t)WAVE_PAGE_STATE_EMPTY;
        g_waveDownload.u16SampleCount[i] = 0U;
        g_waveDownload.u16LastAddress[i] = 0U;
        g_waveDownload.bAddressContinuous[i] = true;
        g_waveDownload.bDownloadComplete[i] = false;
    }
    
    g_waveDownload.stDiag.u32WriteCount = 0UL;
    g_waveDownload.stDiag.u32VerifyFailCount = 0UL;
    g_waveDownload.stDiag.u16LastVerifyExpected = 0U;
    g_waveDownload.stDiag.u16LastVerifyObserved = 0U;
}

void WaveDownload_SetPage(uint16_t u16Page)
{
    if (u16Page < WAVE_MAX_PAGES)
    {
        g_waveDownload.u16SelectedPage = u16Page;
        if (g_waveDownload.u16PageState[u16Page] == (uint16_t)WAVE_PAGE_STATE_EMPTY)
        {
            g_waveDownload.u16PageState[u16Page] = (uint16_t)WAVE_PAGE_STATE_DOWNLOADING;
            g_waveDownload.u16SampleCount[u16Page] = 0U;
            g_waveDownload.u16LastAddress[u16Page] = 0U;
            g_waveDownload.bAddressContinuous[u16Page] = true;
            g_waveDownload.bDownloadComplete[u16Page] = false;
        }
    }
    else
    {
        g_waveDownload.u16SelectedPage = WAVE_PAGE_INVALID;
    }
}

uint16_t WaveDownload_GetPage(void)
{
    return g_waveDownload.u16SelectedPage;
}

/* Storage Access Abstraction */
static void storage_write_sample(uint16_t u16PageId, uint16_t u16Offset, uint16_t u16Sample)
{
#ifdef ASR5K_HAS_EMIF1_SDRAM
    uint16_t *page = (uint16_t *)(WAVE_SDRAM_BASE_ADDR + (uint32_t)u16PageId * 8192UL);
    page[u16Offset] = u16Sample;
#else
    if (u16PageId < 3U)
    {
        g_pFakeSdramPages[u16PageId][u16Offset] = u16Sample;
    }
#endif
}

uint16_t WaveDownload_ReadSample(uint16_t u16PageId, uint16_t u16Offset)
{
    if (u16PageId >= WAVE_MAX_PAGES || u16Offset >= WAVE_SAMPLES_PER_PAGE)
    {
        return 0xFFFFU;
    }
#ifdef ASR5K_HAS_EMIF1_SDRAM
    uint16_t *page = (uint16_t *)(WAVE_SDRAM_BASE_ADDR + (uint32_t)u16PageId * 8192UL);
    return page[u16Offset];
#else
    if (u16PageId < 3U)
    {
        return g_pFakeSdramPages[u16PageId][u16Offset];
    }
    return 0xFFFFU;
#endif
}

uint16_t WaveDownload_WriteSample(uint16_t u16Offset, uint16_t u16Sample)
{
    uint16_t u16PageId = g_waveDownload.u16SelectedPage;
    uint16_t u16Address = WAVE_DATA_WINDOW_BASE + u16Offset;
    
    if (u16PageId >= WAVE_MAX_PAGES || u16Offset >= WAVE_SAMPLES_PER_PAGE)
    {
        return 0xFFFFU;
    }
    
    /* Check address continuity */
    if (g_waveDownload.u16SampleCount[u16PageId] > 0U)
    {
        if (u16Address != g_waveDownload.u16LastAddress[u16PageId] + 1U)
        {
            g_waveDownload.bAddressContinuous[u16PageId] = false;
        }
    }
    else
    {
        if (u16Address != WAVE_DATA_WINDOW_BASE)
        {
            g_waveDownload.bAddressContinuous[u16PageId] = false;
        }
    }
    
    g_waveDownload.u16LastAddress[u16PageId] = u16Address;
    g_waveDownload.u16SampleCount[u16PageId]++;
    
    /* Write via storage abstraction */
    storage_write_sample(u16PageId, u16Offset, u16Sample);
    g_waveDownload.stDiag.u32WriteCount++;
    
#ifdef DIAG_SELFTEST
    /* Perform read-back echo verification under DIAG_SELFTEST compile option */
    uint16_t u16Read = WaveDownload_ReadSample(u16PageId, u16Offset);
    if (u16Read != u16Sample)
    {
        g_waveDownload.stDiag.u32VerifyFailCount++;
        g_waveDownload.stDiag.u16LastVerifyExpected = u16Sample;
        g_waveDownload.stDiag.u16LastVerifyObserved = u16Read;
        return 0xFFFFU;
    }
#endif
    
    return u16Sample;
}

uint16_t WaveDownload_CompleteDownload(uint16_t u16Data)
{
    uint16_t u16PageId = g_waveDownload.u16SelectedPage;
    if (u16PageId >= WAVE_MAX_PAGES || u16Data != 1U)
    {
        return 0xFFFFU;
    }
    
    g_waveDownload.bDownloadComplete[u16PageId] = true;
    g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_DOWNLOAD_COMPLETE;
    return 1U;
}

uint16_t WaveDownload_ValidatePage(void)
{
    uint16_t u16PageId = g_waveDownload.u16SelectedPage;
    if (u16PageId >= WAVE_MAX_PAGES)
    {
        return 0xFFFFU;
    }
    
    /* WAVE_VALIDATE minimum checks:
       - selected page valid
       - page_id within 0x0000~0x00FF
       - sample_count == 4096
       - address_continuous == true
       - last_address == 0x3FFF
       - download_complete == true
       - output_off == true (OUTPUT_ON == 0)
    */
    bool bSelectedPageValid = (u16PageId != WAVE_PAGE_INVALID);
    bool bPageRangeOk = (u16PageId < WAVE_MAX_PAGES);
    bool bCountOk = (g_waveDownload.u16SampleCount[u16PageId] == WAVE_SAMPLES_PER_PAGE);
    bool bContinuousOk = g_waveDownload.bAddressContinuous[u16PageId];
    bool bLastAddrOk = (g_waveDownload.u16LastAddress[u16PageId] == WAVE_DATA_WINDOW_LIMIT);
    bool bCompleteOk = g_waveDownload.bDownloadComplete[u16PageId];
    bool bOutputOff = (OUTPUT_ON == 0U);
    
    g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_VALIDATING;
    
    if (bSelectedPageValid && bPageRangeOk && bCountOk && bContinuousOk &&
        bLastAddrOk && bCompleteOk && bOutputOff)
    {
        g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_VALID;
        /* D10 per-sample checksum is not implemented in M5. */
    }
    else
    {
        g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_INVALID;
    }
    
    return g_waveDownload.u16PageState[u16PageId];
}

uint16_t WaveDownload_ActivatePage(void)
{
    uint16_t u16PageId = g_waveDownload.u16SelectedPage;
    if (u16PageId >= WAVE_MAX_PAGES)
    {
        return 0xFFFFU;
    }
    
    /* WAVE_ACTIVATE preconditions:
       - selected page valid
       - page state == VALID (WAVE_PAGE_STATE_VALID)
       - output_off == true
    */
    bool bSelectedPageValid = (u16PageId != WAVE_PAGE_INVALID);
    bool bStateValid = (g_waveDownload.u16PageState[u16PageId] == (uint16_t)WAVE_PAGE_STATE_VALID);
    bool bOutputOff = (OUTPUT_ON == 0U);
    
    if (bSelectedPageValid && bStateValid && bOutputOff)
    {
        g_waveDownload.u16ActivePage = u16PageId;
        g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_LOCKED;
        return (uint16_t)WAVE_PAGE_STATE_LOCKED;
    }
    
    /* If activation fails, keep previous active page unchanged. */
    return 0xFFFFU;
}

uint16_t WaveDownload_GetPageState(uint16_t u16Page)
{
    if (u16Page < WAVE_MAX_PAGES)
    {
        return g_waveDownload.u16PageState[u16Page];
    }
    return (uint16_t)WAVE_PAGE_STATE_INVALID;
}

bool WaveDownload_HandleWrite(uint16_t u16Address, uint16_t u16Data, uint16_t *pu16Response)
{
    if (u16Address == WAVE_PAGE_SELECT_ADDR)
    {
        WaveDownload_SetPage(u16Data);
        *pu16Response = g_waveDownload.u16SelectedPage;
        return true;
    }
    
    if (u16Address == WAVE_DOWNLOAD_CTRL_ADDR)
    {
        *pu16Response = WaveDownload_CompleteDownload(u16Data);
        return true;
    }
    
    if (u16Address == WAVE_VALIDATE_ADDR)
    {
        *pu16Response = WaveDownload_ValidatePage();
        return true;
    }
    
    if (u16Address == WAVE_ACTIVATE_ADDR)
    {
        *pu16Response = WaveDownload_ActivatePage();
        return true;
    }
    
    if (u16Address == WAVE_PAGE_STATUS_ADDR)
    {
        *pu16Response = (uint16_t)WaveDownload_GetPageState(g_waveDownload.u16SelectedPage);
        return true;
    }
    
    if (u16Address >= WAVE_DATA_WINDOW_BASE && u16Address <= WAVE_DATA_WINDOW_LIMIT)
    {
        if (g_waveDownload.u16SelectedPage != WAVE_PAGE_INVALID)
        {
            uint16_t u16Offset = u16Address - WAVE_DATA_WINDOW_BASE;
            *pu16Response = WaveDownload_WriteSample(u16Offset, u16Data);
            return true;
        }
    }
    
    return false;
}
