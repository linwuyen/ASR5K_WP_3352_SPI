#include "driverlib.h"
#include "device.h"
#include "wave_download.h"
/* spi_slave.h is intentionally NOT included here; OUTPUT_ON is passed by
 * callers via the u16OutputOn parameter to avoid coupling this service to
 * the driver layer. */

#pragma DATA_SECTION(g_waveDownload, "spib_slave_state")
ST_WAVE_DOWNLOAD g_waveDownload;

#ifndef ASR5K_HAS_EMIF1_SDRAM
#pragma DATA_SECTION(g_u16FakeSdram0, "fake_sdram_page0")
uint16_t g_u16FakeSdram0[WAVE_SAMPLES_PER_PAGE];
#pragma DATA_SECTION(g_u16FakeSdram1, "fake_sdram_page1")
uint16_t g_u16FakeSdram1[WAVE_SAMPLES_PER_PAGE];
#pragma DATA_SECTION(g_u16FakeSdram2, "fake_sdram_page2")
uint16_t g_u16FakeSdram2[WAVE_SAMPLES_PER_PAGE];

static uint16_t * const g_pFakeSdramPages[WAVE_FAKE_SDRAM_PAGES] = {
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

    for (i = 0U; i < WAVE_FAKE_SDRAM_PAGES; i++)
    {
        g_waveDownload.u32PageChecksum[i] = 0UL;
    }

    g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_NONE;

    g_waveDownload.stDiag.u32WriteCount = 0UL;
    g_waveDownload.stDiag.u32VerifyFailCount = 0UL;
    g_waveDownload.stDiag.u16LastVerifyExpected = 0U;
    g_waveDownload.stDiag.u16LastVerifyObserved = 0U;
    g_waveDownload.stDiag.u32ChecksumErrorCount = 0UL;
}

void WaveDownload_SetPage(uint16_t u16Page)
{
    if (u16Page < WAVE_MAX_PAGES)
    {
        g_waveDownload.u16SelectedPage = u16Page;
        /* Re-selecting any non-LOCKED page restarts its download:
         * stale metadata from a previous partial/invalid download
         * (e.g. Test6/Test8 leftovers) must not leak into the new
         * download, or the final validation fails on sample count
         * and address continuity. LOCKED (active) pages are kept. */
        if (g_waveDownload.u16PageState[u16Page] != (uint16_t)WAVE_PAGE_STATE_LOCKED)
        {
            g_waveDownload.u16PageState[u16Page] = (uint16_t)WAVE_PAGE_STATE_DOWNLOADING;
            g_waveDownload.u16SampleCount[u16Page] = 0U;
            g_waveDownload.u16LastAddress[u16Page] = 0U;
            g_waveDownload.bAddressContinuous[u16Page] = true;
            g_waveDownload.bDownloadComplete[u16Page] = false;
            /* Fresh download: drop the previous storage-consistency checksum
             * so leftover sums cannot pass a later validation (fake pages only). */
            if (u16Page < WAVE_FAKE_SDRAM_PAGES)
            {
                g_waveDownload.u32PageChecksum[u16Page] = 0UL;
            }
        }
    }
    else
    {
        g_waveDownload.u16SelectedPage = WAVE_PAGE_INVALID;
    }
}

/* Storage Access Abstraction */
static void storage_write_sample(uint16_t u16PageId, uint16_t u16Offset, uint16_t u16Sample)
{
#ifdef ASR5K_HAS_EMIF1_SDRAM
    uint16_t *page = (uint16_t *)(WAVE_SDRAM_BASE_ADDR + (uint32_t)u16PageId * 8192UL);
    page[u16Offset] = u16Sample;
#else
    if (u16PageId < WAVE_FAKE_SDRAM_PAGES)
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
    if (u16PageId < WAVE_FAKE_SDRAM_PAGES)
    {
        return g_pFakeSdramPages[u16PageId][u16Offset];
    }
    return 0xFFFFU;
#endif
}

/* M5 sandbox storage-consistency checksum: per-sample byte fold (D10 section 9
 * formula).  Max value 0x00FF + 0x00FF = 0x01FE, so a 4096-sample page sums to
 * at most 4096 * 510 = 2,088,960, which fits a uint32_t accumulator without
 * overflow.  This validates write->storage->readback only; it is NOT the full
 * D10 per-sample transport checksum (the Legacy 2-word frame carries no
 * independent checksum word). */
static inline uint16_t wave_sample_checksum(uint16_t u16Sample)
{
    return (uint16_t)((u16Sample >> 8) + (u16Sample & 0x00FFU));
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

    /* Sample is accepted and stored: fold it into this page's storage-
     * consistency checksum.  Only fake-backed pages have a checksum slot;
     * re-checked from readback in ValidatePage. */
    if (u16PageId < WAVE_FAKE_SDRAM_PAGES)
    {
        g_waveDownload.u32PageChecksum[u16PageId] += wave_sample_checksum(u16Sample);
    }

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

uint16_t WaveDownload_ValidatePage(uint16_t u16OutputOn)
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
       - output_off == true (u16OutputOn == 0)
    */
    bool bSelectedPageValid = (u16PageId != WAVE_PAGE_INVALID);
    bool bPageRangeOk = (u16PageId < WAVE_MAX_PAGES);
    bool bCountOk = (g_waveDownload.u16SampleCount[u16PageId] == WAVE_SAMPLES_PER_PAGE);
    bool bContinuousOk = g_waveDownload.bAddressContinuous[u16PageId];
    bool bLastAddrOk = (g_waveDownload.u16LastAddress[u16PageId] == WAVE_DATA_WINDOW_LIMIT);
    bool bCompleteOk = g_waveDownload.bDownloadComplete[u16PageId];
    bool bOutputOff = (u16OutputOn == 0U);
    
    g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_NONE;
    g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_VALIDATING;

    if (bSelectedPageValid && bPageRangeOk && bCountOk && bContinuousOk &&
        bLastAddrOk && bCompleteOk && bOutputOff)
    {
        /* M5 sandbox per-page storage-consistency checksum (single pass, 4096
         * samples).  This is NOT the D10 full per-sample checksum; the D10 full
         * per-sample checksum area is deferred to the EMIF1/formal-board phase.
         * Runs only after the preconditions above pass, so offsets
         * 0..WAVE_SAMPLES_PER_PAGE-1 were each written exactly once.  Re-read
         * every sample from storage and re-fold it; the sum is order-
         * independent, so it must equal the write-time page checksum unless
         * storage corrupted a word.  Runs in the background poll at Output OFF
         * (no ISR, no DMA, no blocking delay). */
        bool bChecksumOk;

        if (u16PageId < WAVE_FAKE_SDRAM_PAGES)
        {
            uint32_t u32ReadbackAcc = 0UL;
            uint16_t u16Off;
            for (u16Off = 0U; u16Off < WAVE_SAMPLES_PER_PAGE; u16Off++)
            {
                u32ReadbackAcc +=
                    wave_sample_checksum(WaveDownload_ReadSample(u16PageId, u16Off));
            }
            bChecksumOk = (u32ReadbackAcc == g_waveDownload.u32PageChecksum[u16PageId]);
        }
        else
        {
            /* Page >= WAVE_FAKE_SDRAM_PAGES has no fake SDRAM backing in the
             * sandbox, so storage consistency cannot be established and the
             * page must never be accepted as VALID. */
            bChecksumOk = false;
        }

        if (bChecksumOk)
        {
            g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_VALID;
        }
        else
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_CHECKSUM;
            g_waveDownload.stDiag.u32ChecksumErrorCount++;
            g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_INVALID;
        }
    }
    else
    {
        /* Record the first failing precondition (D10 section 17 codes) so a
         * rejected page reports why; control flow is unchanged. */
        if (!bSelectedPageValid || !bPageRangeOk)
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_INVALID_PAGE_ID;
        }
        else if (!bOutputOff)
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_OUTPUT_ON;
        }
        else if (!bCompleteOk)
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_DOWNLOAD_COMPLETE;
        }
        else if (!bCountOk)
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_SAMPLE_COUNT;
        }
        else if (!bContinuousOk)
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_ADDRESS_DISCONTINUITY;
        }
        else /* !bLastAddrOk */
        {
            g_waveDownload.u16LastValidateError = WAVE_VALIDATE_ERR_ADDRESS_OUT_OF_RANGE;
        }

        g_waveDownload.u16PageState[u16PageId] = (uint16_t)WAVE_PAGE_STATE_INVALID;
    }

    return g_waveDownload.u16PageState[u16PageId];
}

uint16_t WaveDownload_ActivatePage(uint16_t u16OutputOn)
{
    uint16_t u16PageId = g_waveDownload.u16SelectedPage;
    if (u16PageId >= WAVE_MAX_PAGES)
    {
        return 0xFFFFU;
    }

    /* WAVE_ACTIVATE preconditions:
       - selected page valid
       - page state == VALID (WAVE_PAGE_STATE_VALID)
       - output_off == true (u16OutputOn == 0)
    */
    bool bSelectedPageValid = (u16PageId != WAVE_PAGE_INVALID);
    bool bStateValid = (g_waveDownload.u16PageState[u16PageId] == (uint16_t)WAVE_PAGE_STATE_VALID);
    bool bOutputOff = (u16OutputOn == 0U);
    
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

bool WaveDownload_HandleWrite(uint16_t u16Address, uint16_t u16Data,
                              uint16_t *pu16Response, uint16_t u16OutputOn)
{
    bool bWaveMutation =
        (u16Address == WAVE_PAGE_SELECT_ADDR) ||
        (u16Address == WAVE_DOWNLOAD_CTRL_ADDR) ||
        (u16Address == WAVE_VALIDATE_ADDR) ||
        (u16Address == WAVE_ACTIVATE_ADDR) ||
        ((u16Address >= WAVE_DATA_WINDOW_BASE) &&
         (u16Address <= WAVE_DATA_WINDOW_LIMIT));

    if ((u16OutputOn != 0U) && bWaveMutation)
    {
        *pu16Response = 0xFFFFU;
        return true;
    }

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
        *pu16Response = WaveDownload_ValidatePage(u16OutputOn);
        return true;
    }

    if (u16Address == WAVE_ACTIVATE_ADDR)
    {
        *pu16Response = WaveDownload_ActivatePage(u16OutputOn);
        return true;
    }

    if (u16Address == WAVE_PAGE_STATUS_ADDR)
    {
        *pu16Response = (uint16_t)WaveDownload_GetPageState(g_waveDownload.u16SelectedPage);
        return true;
    }

    if ((u16Address >= WAVE_DATA_WINDOW_BASE) && (u16Address <= WAVE_DATA_WINDOW_LIMIT))
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
