#ifndef WAVE_DOWNLOAD_H_
#define WAVE_DOWNLOAD_H_

#include <stdint.h>
#include <stdbool.h>

/* Register addresses are owned by cmd_id.h; these fallbacks apply only when
 * cmd_id.h is not included first (e.g. standalone host-side unit tests). */
#ifndef WAVE_PAGE_SELECT_ADDR
#define WAVE_PAGE_SELECT_ADDR       0x0958
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
#ifndef WAVE_STATUS_REG_READY
#define WAVE_STATUS_REG_READY       0x0001U
#define WAVE_STATUS_RX_DONE         0x0002U
#define WAVE_STATUS_ERROR           0x0004U
#define WAVE_STATUS_READY_MASK      \
    (WAVE_STATUS_REG_READY | WAVE_STATUS_RX_DONE)
#endif

#define WAVE_DATA_WINDOW_BASE       0x3000U
#define WAVE_DATA_WINDOW_LIMIT      0x3FFFU

#define WAVE_SAMPLES_PER_PAGE       4096U
#define WAVE_SDRAM_BASE_ADDR        0x80000000UL

#define WAVE_MAX_PAGES              256U
#define WAVE_PAGE_INVALID           0xFFFFU

/* Sandbox fake SDRAM only backs pages 0..WAVE_FAKE_SDRAM_PAGES-1.  This is the
 * single source for that count: storage access (g_pFakeSdramPages[]) and the
 * per-page storage-consistency checksum must agree on it. */
#define WAVE_FAKE_SDRAM_PAGES       3U

/* Validation error codes (subset of D10 section 17 "Error Codes").
 * Reported via g_waveDownload.u16LastValidateError after WaveDownload_ValidatePage.
 * NOTE: WAVE_VALIDATE_ERR_CHECKSUM here is produced by the M5 sandbox
 * storage-consistency checksum (write->storage->readback), NOT the full
 * D10 per-sample transport checksum area (deferred to EMIF1/formal board). */
#define WAVE_VALIDATE_ERR_NONE                  0x0000U
#define WAVE_VALIDATE_ERR_INVALID_PAGE_ID       0x0001U
#define WAVE_VALIDATE_ERR_OUTPUT_ON             0x0002U
#define WAVE_VALIDATE_ERR_ADDRESS_OUT_OF_RANGE  0x0003U
#define WAVE_VALIDATE_ERR_ADDRESS_DISCONTINUITY 0x0004U
#define WAVE_VALIDATE_ERR_SAMPLE_COUNT          0x0005U
#define WAVE_VALIDATE_ERR_CHECKSUM              0x0006U
#define WAVE_VALIDATE_ERR_DOWNLOAD_COMPLETE     0x0007U

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
    uint32_t u32ChecksumErrorCount; /* M5 storage-consistency checksum mismatch count */
} ST_WAVE_DOWNLOAD_DIAG;

typedef struct {
    uint16_t u16SelectedPage;                   /* Selected Page Index (0-255 or 0xFFFF) */
    uint16_t u16ActivePage;                     /* Currently active page index used by DDS */
    uint16_t u16LastValidateError;              /* Last WaveDownload_ValidatePage error (WAVE_VALIDATE_ERR_*) */
    uint16_t u16ExpectedLength;                 /* Samples expected for the active burst */
    uint16_t u16Status;                         /* WAVE_STATUS_* register flags */
    uint16_t u16PageState[WAVE_MAX_PAGES];      /* State of each of the 256 pages */
    uint16_t u16SampleCount[WAVE_MAX_PAGES];    /* Number of samples received for each page */
    uint16_t u16LastAddress[WAVE_MAX_PAGES];    /* Last sample address received for continuity check */
    bool     bAddressContinuous[WAVE_MAX_PAGES]; /* Whether address sequence has been continuous */
    bool     bDownloadComplete[WAVE_MAX_PAGES]; /* Whether the expected burst completed */
    uint32_t u32PageChecksum[WAVE_FAKE_SDRAM_PAGES]; /* M5 sandbox per-page storage-consistency checksum (fake SDRAM pages 0..2 only) */
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
 * @brief Start a burst download and reset the selected page metadata.
 */
uint16_t WaveDownload_BeginBurst(uint16_t u16ExpectedLength);

/**
 * @brief Finalize a burst after all DMA and deferred parsing has completed.
 */
uint16_t WaveDownload_FinalizeBurst(bool bTransportOk);

/**
 * @brief Mark normal register DMA ready for status/control transactions.
 */
void WaveDownload_SetRegReady(void);

/**
 * @brief Return the WAVE_STATUS_* bitfield exposed by 0x095A.
 */
uint16_t WaveDownload_GetStatus(void);

/**
 * @brief Validate selected page.
 * @param u16OutputOn  Current OUTPUT_ON state (0 = output off).
 *                     Caller injects this value so wave_download has no
 *                     direct dependency on spi_slave globals.
 */
uint16_t WaveDownload_ValidatePage(uint16_t u16OutputOn);

/**
 * @brief Activate selected page.
 * @param u16OutputOn  Current OUTPUT_ON state (0 = output off).
 */
uint16_t WaveDownload_ActivatePage(uint16_t u16OutputOn);

/**
 * @brief Return page state for diagnostics/tests.
 */
uint16_t WaveDownload_GetPageState(uint16_t u16Page);

/**
 * @brief Wave Download Service address handler.
 * @return true if address was handled by wave download service.
 */
bool WaveDownload_HandleWrite(uint16_t u16Address, uint16_t u16Data,
                              uint16_t *pu16Response, uint16_t u16OutputOn);

#endif /* WAVE_DOWNLOAD_H_ */
