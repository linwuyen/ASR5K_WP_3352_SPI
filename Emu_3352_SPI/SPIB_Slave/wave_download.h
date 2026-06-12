#ifndef WAVE_DOWNLOAD_H_
#define WAVE_DOWNLOAD_H_

#include <stdint.h>
#include <stdbool.h>

/* Register addresses are owned by cmd_id.h; these fallbacks apply only when
 * cmd_id.h is not included first (e.g. standalone host-side unit tests). */
#ifndef WAVE_PAGE_SELECT_ADDR
#define WAVE_PAGE_SELECT_ADDR       0x0958
#endif
#ifndef WAVE_DOWNLOAD_CTRL_ADDR
#define WAVE_DOWNLOAD_CTRL_ADDR     0x0959
#endif
#ifndef WAVE_PAGE_STATUS_ADDR
#define WAVE_PAGE_STATUS_ADDR       0x095A
#endif
#ifndef WAVE_VALIDATE_ADDR
#define WAVE_VALIDATE_ADDR          0x0960
#endif
#ifndef WAVE_ACTIVATE_ADDR
#define WAVE_ACTIVATE_ADDR          0x0961
#endif

#define WAVE_DATA_WINDOW_BASE       0x3000U
#define WAVE_DATA_WINDOW_LIMIT      0x3FFFU

#define WAVE_SAMPLES_PER_PAGE       4096U
#define WAVE_SDRAM_BASE_ADDR        0x80000000UL

#define WAVE_MAX_PAGES              256U
#define WAVE_PAGE_INVALID           0xFFFFU

/* D10-compatible page states only */
typedef enum {
    WAVE_PAGE_STATE_EMPTY = 0,
    WAVE_PAGE_STATE_DOWNLOADING,
    WAVE_PAGE_STATE_DOWNLOAD_COMPLETE,
    WAVE_PAGE_STATE_VALIDATING,
    WAVE_PAGE_STATE_VALID,
    WAVE_PAGE_STATE_INVALID,
    WAVE_PAGE_STATE_LOCKED
} ST_WAVE_PAGE_STATE;

typedef struct {
    uint32_t u32WriteCount;         /* Total sample words written to SDRAM */
    uint32_t u32VerifyFailCount;    /* Total read-back mismatch count (DIAG_SELFTEST only) */
    uint16_t u16LastVerifyExpected; /* Last expected value (DIAG_SELFTEST only) */
    uint16_t u16LastVerifyObserved; /* Last observed value (DIAG_SELFTEST only) */
} ST_WAVE_DOWNLOAD_DIAG;

typedef struct {
    uint16_t u16SelectedPage;                   /* Selected Page Index (0-255 or 0xFFFF) */
    uint16_t u16ActivePage;                     /* Currently active page index used by DDS */
    uint16_t u16PageState[WAVE_MAX_PAGES];      /* State of each of the 256 pages */
    uint16_t u16SampleCount[WAVE_MAX_PAGES];    /* Number of samples received for each page */
    uint16_t u16LastAddress[WAVE_MAX_PAGES];    /* Last sample address received for continuity check */
    bool     bAddressContinuous[WAVE_MAX_PAGES]; /* Whether address sequence has been continuous */
    bool     bDownloadComplete[WAVE_MAX_PAGES]; /* Whether complete command has been received */
    ST_WAVE_DOWNLOAD_DIAG stDiag;               /* Diagnostic counters */
} ST_WAVE_DOWNLOAD;

extern ST_WAVE_DOWNLOAD g_waveDownload;

/**
 * @brief Initialize Wave Download Service.
 */
void WaveDownload_Init(void);

/**
 * @brief Select download page index.
 */
void WaveDownload_SetPage(uint16_t u16Page);

/**
 * @brief Write sample word to selected page SDRAM.
 */
uint16_t WaveDownload_WriteSample(uint16_t u16Offset, uint16_t u16Sample);

/**
 * @brief Read sample word from specified page storage (abstraction).
 */
uint16_t WaveDownload_ReadSample(uint16_t u16PageId, uint16_t u16Offset);

/**
 * @brief Complete download process for selected page.
 */
uint16_t WaveDownload_CompleteDownload(uint16_t u16Data);

/**
 * @brief Validate selected page.
 * @param u16OutputOn  Current OUTPUT_ON state (0 = output off).
 *                     Caller injects this value so wave_download has no
 *                     direct dependency on spi_slave globals.
 */
uint16_t WaveDownload_ValidatePage(uint16_t u16OutputOn);

/**
 * @brief Activate selected page for DDS.
 * @param u16OutputOn  Current OUTPUT_ON state (0 = output off).
 */
uint16_t WaveDownload_ActivatePage(uint16_t u16OutputOn);

/**
 * @brief Get state of specified page.
 */
uint16_t WaveDownload_GetPageState(uint16_t u16Page);

/**
 * @brief Route write commands to Wave Download Service if applicable.
 * @param u16OutputOn  Current OUTPUT_ON state forwarded to validate/activate.
 */
bool WaveDownload_HandleWrite(uint16_t u16Address, uint16_t u16Data,
                              uint16_t *pu16Response, uint16_t u16OutputOn);

#endif /* WAVE_DOWNLOAD_H_ */
