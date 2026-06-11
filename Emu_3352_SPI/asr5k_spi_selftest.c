/*
 * asr5k_spi_selftest.c
 *
 * Table-driven SPI Master/Slave self-test engine.
 *
 * 架構分層:
 *   [Test script tables]  每個測試 = 一張 ST_SELFTEST_STEP 步驟表
 *                         + 一個 final validator。多階段測試 (Test3/9)
 *                         不再需要散落的 phase 旗標。
 *   [Engine]              通用步驟執行器 + counter delta 共同驗證。
 *   [Port layer]          asr5k_spi_selftest_port.h — 所有 Master/Slave
 *                         相依集中於此,日後拆分只改 port。
 *
 * SPI traffic is started exclusively through the master port API; all
 * validation uses existing results and diagnostics (read-only observation).
 */

#include "asr5k_spi_selftest.h"
#include "asr5k_spi_selftest_port.h"
#include "board.h"
#include "driverlib.h"

/* ========================================================================
 * Engine configuration
 * ======================================================================== */
#define SELFTEST_WAIT_LIMIT      5000000UL
#define SELFTEST_EXECUTED_COUNT  9U
#define SELFTEST_UART_RX_SIZE    16U
#define SELFTEST_UART_TX_SIZE    96U

#define SELFTEST_TARGET_PAGE     1U      /* wave page used by Test5~9      */

/* Test6 partial-write script parameters */
#define TEST6_SAMPLE_COUNT       4U
#define TEST6_FIRST_ADDR         (WAVE_WINDOW_BASE_ADDR)
#define TEST6_LAST_ADDR          (WAVE_WINDOW_BASE_ADDR + TEST6_SAMPLE_COUNT - 1U)
#define TEST6_SAMPLE_VALUE(i)    ((uint16_t)(((i) + 1U) * 0x10U))

/* ========================================================================
 * Fault codes (host visible)
 * ======================================================================== */
#define SELFTEST_FAULT_START_REJECTED    0x3001U
#define SELFTEST_FAULT_WAIT_TIMEOUT      0x3002U
#define SELFTEST_FAULT_MASTER_STATUS     0x3003U
#define SELFTEST_FAULT_MASTER_RESULT     0x3004U
#define SELFTEST_FAULT_MASTER_DETAIL     0x3005U
#define SELFTEST_FAULT_OUTPUT_STATE      0x3006U
#define SELFTEST_FAULT_DMA_DONE_DELTA    0x3007U
#define SELFTEST_FAULT_PARSE_OK_DELTA    0x3008U
#define SELFTEST_FAULT_PARSE_FAIL_DELTA  0x3009U
#define SELFTEST_FAULT_DMA_RESTART_DELTA 0x300AU
#define SELFTEST_FAULT_SLAVE_ERROR_FLAGS 0x300BU
#define SELFTEST_FAULT_BLOCK_RESULT      0x300CU
#define SELFTEST_FAULT_RAMP_DATA         0x300DU
/* 0x300E~0x3011 reserved (legacy sine/packet tests, removed in Phase 2)   */
#define SELFTEST_FAULT_PAGE_SELECT       0x3012U  /* Test5 metadata         */
#define SELFTEST_FAULT_SAMPLE_WRITE      0x3013U  /* Test6 content          */
#define SELFTEST_FAULT_WAVE_METADATA     0x3014U  /* Test6/7 metadata       */
#define SELFTEST_FAULT_PRECHECK          0x3015U  /* Test8 validator gate   */
#define SELFTEST_FAULT_WAVE_FINAL        0x3016U  /* Test9 final state      */

/* ========================================================================
 * Engine types
 * ======================================================================== */
typedef enum {
    SELFTEST_STATE_IDLE = 0,
    SELFTEST_STATE_START,
    SELFTEST_STATE_WAIT_RUNNING,
    SELFTEST_STATE_WAIT_DONE,
    SELFTEST_STATE_STEP_DONE
} SELFTEST_STATE_e;

/*
 * Per-step check hook.
 * 在該步驟 master 回報 PASSED 後立即呼叫 (例如 Test3 需逐步確認
 * OUTPUT_ON 的中間狀態)。回傳 0 表示通過;非 0 為 fault code,
 * 並透過 *pFailStep 回報失敗位置。
 */
typedef uint16_t (*PFN_STEP_CHECK)(uint16_t u16StepIndex,
                                   uint16_t *pFailStep);

/*
 * Test-level final validator.
 * 引擎已先完成 counter delta 計算與共同驗證,validator 只需檢查
 * 該測試特有的 slave 端狀態,並填寫 result->actual。
 */
typedef uint16_t (*PFN_TEST_VALIDATE)(volatile ST_ASR5K_SPI_TEST_RESULT *pResult,
                                      uint16_t *pFailStep);

typedef struct {
    SPI_MASTER_TEST_CMD_e eCommand;     /* master command to issue          */
    uint16_t u16Address;
    uint16_t u16Data;
    uint16_t u16ExpectedResult;         /* checked when bCheckResult != 0   */
    uint16_t bCheckResult;
    uint16_t u16FailStepOnResult;       /* fail step reported on mismatch   */
    PFN_STEP_CHECK pfnStepCheck;        /* optional, NULL to skip           */
} ST_SELFTEST_STEP;

typedef struct {
    ASR5K_SPI_TEST_ID_e eTestId;
    const ST_SELFTEST_STEP *pSteps;
    uint16_t u16StepCount;
    uint32_t u32ExpectedSummary;        /* copied into result->expected     */
    uint32_t u32DmaDoneDelta;           /* expected delta over whole test   */
    uint16_t bDmaDeltaIsMinimum;        /* 0: exact, 1: minimum (long xfer) */
    PFN_TEST_VALIDATE pfnValidate;      /* test-specific final checks       */
} ST_SELFTEST_TEST;

/* ========================================================================
 * Forward declarations: step checks and validators
 * ======================================================================== */
static uint16_t StepCheck_OutputIsOn(uint16_t u16StepIndex, uint16_t *pFailStep);
static uint16_t StepCheck_OutputIsOff(uint16_t u16StepIndex, uint16_t *pFailStep);

static uint16_t Validate_Test1_RegisterWrite(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test2_RegisterRead(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test3_OutputToggle(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test4_BlockWrite16(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test5_PageSelect(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test6_SampleWrite(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test7_DownloadComplete(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test8_ValidatePrecheck(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test9_FullPipeline(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);

/* ========================================================================
 * Test scripts
 * 新增測試: 加一張步驟表 + 一個 validator + s_testTable 一列即可。
 * ======================================================================== */

/* ---- Test1: single register write -------------------------------------- */
#pragma DATA_SECTION(s_test1Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test1Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, 0x0401U, 0x1234U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};

/* ---- Test2: single register read ---------------------------------------- */
#pragma DATA_SECTION(s_test2Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test2Steps[] = {
    { SPI_MASTER_TEST_CMD_READ, 0x0400U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};

/* ---- Test3: output relay set then clear --------------------------------- */
#pragma DATA_SECTION(s_test3Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test3Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, 0x0900U, 0x0001U,
      1U, 1U, ASR5K_SPI_FAIL_STEP_OUTPUT_SET,   StepCheck_OutputIsOn  },
    { SPI_MASTER_TEST_CMD_WRITE, 0x0900U, 0x0000U,
      0U, 1U, ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR, StepCheck_OutputIsOff }
};

/* ---- Test4: 16-word sequential block write ------------------------------ */
#pragma DATA_SECTION(s_test4Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test4Steps[] = {
    { SPI_MASTER_TEST_CMD_SEQ_WRITE_16, 0x0000U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};

/* ---- Test5: wave page select --------------------------------------------
 * 0x0958 寫入 page_id,確認 selected-page metadata 正確初始化。      */
#pragma DATA_SECTION(s_test5Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test5Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_PAGE_SELECT_ADDR, SELFTEST_TARGET_PAGE,
      SELFTEST_TARGET_PAGE, 1U, ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT, 0 }
};

/* ---- Test6: partial sample write into 0x3000 window ---------------------
 * 寫入 4 筆取樣,確認 window parser 落到正確 page/index。            */
#pragma DATA_SECTION(s_test6Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test6Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_WINDOW_BASE_ADDR + 0U, TEST6_SAMPLE_VALUE(0U),
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_WINDOW_BASE_ADDR + 1U, TEST6_SAMPLE_VALUE(1U),
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_WINDOW_BASE_ADDR + 2U, TEST6_SAMPLE_VALUE(2U),
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_WINDOW_BASE_ADDR + 3U, TEST6_SAMPLE_VALUE(3U),
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};

/* ---- Test7: download-complete metadata transition ------------------------ */
#pragma DATA_SECTION(s_test7Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test7Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_DOWNLOAD_CTRL_ADDR, 0x0001U,
      1U, 1U, ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD, 0 }
};

/* ---- Test8: validate pre-check (NEGATIVE test) ---------------------------
 * Test6 只寫了 4/4096 筆,validator 必須因 sample count 不足而拒絕。
 * 同時確認 Output 維持 OFF (validation 不得觸發功率級)。            */
#pragma DATA_SECTION(s_test8Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test8Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_VALIDATE_ADDR, 0x0001U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, StepCheck_OutputIsOff }
};

/* ---- Test9: full pipeline -------------------------------------------------
 * select → full 4096 download → complete → validate(VALID) → activate(LOCKED)
 * 重點: 長傳輸下 DMA CH3 無遺失 (delta 計數一致)、終態正確。          */
#pragma DATA_SECTION(s_test9Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test9Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_PAGE_SELECT_ADDR, SELFTEST_TARGET_PAGE,
      SELFTEST_TARGET_PAGE, 1U, ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT, 0 },
    { SPI_MASTER_TEST_CMD_WAVE_DOWNLOAD, 0x0000U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_DOWNLOAD_CTRL_ADDR, 0x0001U,
      1U, 1U, ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_VALIDATE_ADDR, 0x0001U,
      WAVE_PAGE_STATE_VALID, 1U, ASR5K_SPI_FAIL_STEP_WAVE_VALIDATE, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_ACTIVATE_ADDR, 0x0001U,
      WAVE_PAGE_STATE_LOCKED, 1U, ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE, 0 }
};

/* ---- Master test table ----------------------------------------------------
 * u32DmaDoneDelta: 一次 register write/read = 2 frames。
 *   T5: 1*2   T6: 4*2   T7: 1*2   T8: 1*2
 *   T9: select(2) + 4096-download(>=4100) + ctrl(2)+validate(2)+activate(2)
 *       → minimum 4108。
 * -------------------------------------------------------------------------- */
#pragma DATA_SECTION(s_testTable, "asr5k_spi_selftest_config")
static const ST_SELFTEST_TEST s_testTable[SELFTEST_EXECUTED_COUNT] = {
    { ASR5K_SPI_TEST_ID_1, s_test1Steps, 1U,
      0x04471234UL, 2U,    0U, Validate_Test1_RegisterWrite },

    { ASR5K_SPI_TEST_ID_2, s_test2Steps, 1U,
      0x051A8A90UL, 2U,    0U, Validate_Test2_RegisterRead },

    { ASR5K_SPI_TEST_ID_3, s_test3Steps, 2U,
      0x00010000UL, 4U,    0U, Validate_Test3_OutputToggle },

    { ASR5K_SPI_TEST_ID_4, s_test4Steps, 1U,
      0x10110010UL, 21U,   1U, Validate_Test4_BlockWrite16 },

    { ASR5K_SPI_TEST_ID_5, s_test5Steps, 1U,
      (uint32_t)SELFTEST_TARGET_PAGE, 2U, 0U, Validate_Test5_PageSelect },

    { ASR5K_SPI_TEST_ID_6, s_test6Steps, TEST6_SAMPLE_COUNT,
      ((uint32_t)TEST6_SAMPLE_COUNT << 16) | (uint32_t)TEST6_LAST_ADDR,
      (uint32_t)(TEST6_SAMPLE_COUNT * 2U), 0U, Validate_Test6_SampleWrite },

    { ASR5K_SPI_TEST_ID_7, s_test7Steps, 1U,
      (uint32_t)WAVE_PAGE_STATE_DOWNLOAD_COMPLETE, 2U, 0U,
      Validate_Test7_DownloadComplete },

    { ASR5K_SPI_TEST_ID_8, s_test8Steps, 1U,
      (uint32_t)WAVE_PAGE_STATE_INVALID, 2U, 0U,
      Validate_Test8_ValidatePrecheck },

    { ASR5K_SPI_TEST_ID_9, s_test9Steps, 5U,
      (uint32_t)WAVE_PAGE_STATE_LOCKED, 4108U, 1U,
      Validate_Test9_FullPipeline }
};

/* ========================================================================
 * Engine state (all in dedicated section for easy CCS watch)
 * ======================================================================== */
#pragma DATA_SECTION(g_asr5kSpiSelfTest, "asr5k_spi_selftest_state")
volatile ST_ASR5K_SPI_SELFTEST_RESULT g_asr5kSpiSelfTest;

#pragma DATA_SECTION(s_state, "asr5k_spi_selftest_state")
static SELFTEST_STATE_e s_state;
#pragma DATA_SECTION(s_testIndex, "asr5k_spi_selftest_state")
static uint16_t s_testIndex;          /* index into s_testTable            */
#pragma DATA_SECTION(s_stepIndex, "asr5k_spi_selftest_state")
static uint16_t s_stepIndex;          /* index into current step table     */
#pragma DATA_SECTION(s_waitCount, "asr5k_spi_selftest_state")
static uint32_t s_waitCount;

#pragma DATA_SECTION(s_uartRx, "asr5k_spi_selftest_state")
static char s_uartRx[SELFTEST_UART_RX_SIZE];
#pragma DATA_SECTION(s_uartRxIndex, "asr5k_spi_selftest_state")
static uint16_t s_uartRxIndex;
#pragma DATA_SECTION(s_uartCapture, "asr5k_spi_selftest_state")
static uint16_t s_uartCapture;
#pragma DATA_SECTION(s_uartTx, "asr5k_spi_selftest_state")
static char s_uartTx[SELFTEST_UART_TX_SIZE];
#pragma DATA_SECTION(s_uartTxLength, "asr5k_spi_selftest_state")
static uint16_t s_uartTxLength;
#pragma DATA_SECTION(s_uartTxIndex, "asr5k_spi_selftest_state")
static uint16_t s_uartTxIndex;
#pragma DATA_SECTION(s_uartLastStatus, "asr5k_spi_selftest_state")
static ASR5K_SPI_SELFTEST_STATUS_e s_uartLastStatus;

/* ========================================================================
 * Small helpers
 * ======================================================================== */
static const ST_SELFTEST_TEST *currentTest(void)
{
    return &s_testTable[s_testIndex];
}

static volatile ST_ASR5K_SPI_TEST_RESULT *currentResult(void)
{
    return &g_asr5kSpiSelfTest.test[(uint16_t)currentTest()->eTestId - 1U];
}

static void setResultText(char c0, char c1, char c2, char c3)
{
    g_asr5kSpiSelfTest.result_text[0] = c0;
    g_asr5kSpiSelfTest.result_text[1] = c1;
    g_asr5kSpiSelfTest.result_text[2] = c2;
    g_asr5kSpiSelfTest.result_text[3] = c3;
    g_asr5kSpiSelfTest.result_text[4] = '\0';
}

static void readCounters(volatile ST_ASR5K_SPI_COUNTERS *pCounters)
{
    pCounters->dma_done    = SelfTestPort_SlaveDmaDoneCount();
    pCounters->parse_ok    = SelfTestPort_SlaveParseOkCount();
    pCounters->parse_fail  = SelfTestPort_SlaveParseFailCount();
    pCounters->dma_restart = SelfTestPort_SlaveDmaRestartCount();
}

static void calculateDelta(volatile ST_ASR5K_SPI_TEST_RESULT *pResult)
{
    pResult->delta.dma_done =
        SelfTestPort_SlaveDmaDoneCount()    - pResult->baseline.dma_done;
    pResult->delta.parse_ok =
        SelfTestPort_SlaveParseOkCount()    - pResult->baseline.parse_ok;
    pResult->delta.parse_fail =
        SelfTestPort_SlaveParseFailCount()  - pResult->baseline.parse_fail;
    pResult->delta.dma_restart =
        SelfTestPort_SlaveDmaRestartCount() - pResult->baseline.dma_restart;
}

static void captureFaults(volatile ST_ASR5K_SPI_TEST_RESULT *pResult)
{
    pResult->spiA_fault  = SelfTestPort_MasterFaultCode();
    pResult->spiB_fault  = SelfTestPort_SlaveFaultCode();
    pResult->error_flags = SelfTestPort_SlaveErrorFlags();
}

/*
 * Compose a diagnostic fault code from master/slave hardware diagnostics.
 *   0x1xyy: master fault, x = source, yy = code
 *   0x2xyy: slave fault / rx error flags
 */
static uint16_t getDiagnosticFault(void)
{
    if (SelfTestPort_MasterFaultCode() != 0U) {
        return (uint16_t)(0x1000U |
            ((SelfTestPort_MasterFaultSource() & 0x000FU) << 8U) |
            (SelfTestPort_MasterFaultCode() & 0x00FFU));
    }

    if ((SelfTestPort_SlaveFaultCode() != 0U) ||
        (SelfTestPort_SlaveErrorFlags() != 0U)) {
        return (uint16_t)(0x2000U |
            ((SelfTestPort_SlaveFaultSource() & 0x000FU) << 8U) |
            ((SelfTestPort_SlaveFaultCode() |
              SelfTestPort_SlaveErrorFlags()) & 0x00FFU));
    }

    return 0U;
}

/* ========================================================================
 * Common counter validation
 * 對整個測試 (所有步驟累計) 的 DMA / parser delta 做一致性驗證:
 *   - dma_done 符合期望 (exact 或 minimum)
 *   - parse_ok == dma_done       (每個 frame 都被成功解析)
 *   - parse_fail == 0
 *   - dma_restart >= dma_done    (每個 frame 後 DMA 都有被重掛)
 *   - slave error flags == 0
 * ======================================================================== */
static uint16_t validateCounters(
    const volatile ST_ASR5K_SPI_TEST_RESULT *pResult,
    uint32_t u32ExpectedDone,
    uint16_t bMinimum,
    uint16_t *pFailStep)
{
    uint16_t u16Fault = getDiagnosticFault();

    if (u16Fault != 0U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_SPI_DIAG;
        return u16Fault;
    }
    if (bMinimum != 0U) {
        if (pResult->delta.dma_done < u32ExpectedDone) {
            *pFailStep = ASR5K_SPI_FAIL_STEP_DMA_DONE;
            return SELFTEST_FAULT_DMA_DONE_DELTA;
        }
    } else {
        if (pResult->delta.dma_done != u32ExpectedDone) {
            *pFailStep = ASR5K_SPI_FAIL_STEP_DMA_DONE;
            return SELFTEST_FAULT_DMA_DONE_DELTA;
        }
    }
    if (pResult->delta.parse_ok != pResult->delta.dma_done) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_PARSE_OK;
        return SELFTEST_FAULT_PARSE_OK_DELTA;
    }
    if (pResult->delta.parse_fail != 0U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_PARSE_FAIL;
        return SELFTEST_FAULT_PARSE_FAIL_DELTA;
    }
    if (pResult->delta.dma_restart < pResult->delta.dma_done) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_DMA_RESTART;
        return SELFTEST_FAULT_DMA_RESTART_DELTA;
    }
    if (pResult->error_flags != 0U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_ERROR_FLAGS;
        return SELFTEST_FAULT_SLAVE_ERROR_FLAGS;
    }

    return 0U;
}

/* ========================================================================
 * Step checks (per-step intermediate assertions)
 * ======================================================================== */
static uint16_t StepCheck_OutputIsOn(uint16_t u16StepIndex,
                                     uint16_t *pFailStep)
{
    (void)u16StepIndex;
    if (SelfTestPort_OutputOn() != 1U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_OUTPUT_SET;
        return SELFTEST_FAULT_OUTPUT_STATE;
    }
    return 0U;
}

static uint16_t StepCheck_OutputIsOff(uint16_t u16StepIndex,
                                      uint16_t *pFailStep)
{
    (void)u16StepIndex;
    if (SelfTestPort_OutputOn() != 0U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR;
        return SELFTEST_FAULT_OUTPUT_STATE;
    }
    return 0U;
}

/* ========================================================================
 * Test validators (final, test-specific checks)
 * 慣例: 先填 result->actual (host 可觀測),再回傳 fault code。
 * ======================================================================== */

/* ---- Test1: write detail word must echo expected frame ------------------ */
static uint16_t Validate_Test1_RegisterWrite(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    pResult->actual = SelfTestPort_MasterDetail32();
    if (pResult->actual != pResult->expected) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_MASTER_DETAIL;
        return SELFTEST_FAULT_MASTER_DETAIL;
    }
    return 0U;
}

/* ---- Test2: read detail word -------------------------------------------- */
static uint16_t Validate_Test2_RegisterRead(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    pResult->actual = SelfTestPort_MasterDetail32();
    if (pResult->actual != pResult->expected) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_MASTER_DETAIL;
        return SELFTEST_FAULT_MASTER_DETAIL;
    }
    return 0U;
}

/* ---- Test3: actual = (set_ok << 16) | clear_result ---------------------- */
static uint16_t Validate_Test3_OutputToggle(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    /* 中間狀態已由 step check 驗證,這裡只彙整終態。 */
    uint16_t u16ClearOk =
        ((SelfTestPort_MasterResult16() == 0U) &&
         (SelfTestPort_OutputOn() == 0U)) ? 0U : 1U;

    pResult->actual = (1UL << 16) | (uint32_t)u16ClearOk;
    if (pResult->actual != pResult->expected) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR;
        return SELFTEST_FAULT_OUTPUT_STATE;
    }
    return 0U;
}

/* ---- Common block-transfer result check (Test4) -------------------------- */
static uint16_t checkBlockResult(uint16_t u16ExpectedLength,
                                 uint16_t *pFailStep)
{
    if ((SelfTestPort_BlockWriteIndex()  != u16ExpectedLength) ||
        (SelfTestPort_BlockExpectedLen() != u16ExpectedLength) ||
        (SelfTestPort_BlockChecksum() !=
         SelfTestPort_BlockExpectedChecksum()) ||
        (SelfTestPort_BlockErrorCode() != SPI_BLOCK_ERROR_NONE) ||
        (SelfTestPort_BlockProgress()  != 100U) ||
        (SelfTestPort_BlockStatus()    != 3U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_BLOCK_RESULT;
        return SELFTEST_FAULT_BLOCK_RESULT;
    }
    return 0U;
}

/* ---- Test4: 16-word block ------------------------------------------------ */
static uint16_t Validate_Test4_BlockWrite16(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Fault;

    pResult->actual =
        ((uint32_t)SelfTestPort_BlockChecksum() << 16) |
        (uint32_t)SelfTestPort_BlockWriteIndex();

    u16Fault = checkBlockResult(16U, pFailStep);
    if (u16Fault != 0U) {
        return u16Fault;
    }
    if (pResult->actual != pResult->expected) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_BLOCK_RESULT;
        return SELFTEST_FAULT_BLOCK_RESULT;
    }
    return 0U;
}

/* ---- Test5: page select metadata -----------------------------------------
 * 0x0958 寫入後:
 *   - u16SelectedPage == 目標 page
 *   - 該 page metadata 重置 (count=0, complete=false)
 *   - page state 進入 DOWNLOADING
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test5_PageSelect(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;

    pResult->actual = (uint32_t)SelfTestPort_WaveSelectedPage();

    if (SelfTestPort_WaveSelectedPage() != u16Page) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT;
        return SELFTEST_FAULT_PAGE_SELECT;
    }
    if ((SelfTestPort_WaveSampleCount(u16Page) != 0U) ||
        (SelfTestPort_WaveDownloadComplete(u16Page) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_PAGE_SELECT;
    }
    if (SelfTestPort_WavePageState(u16Page) !=
        WAVE_PAGE_STATE_DOWNLOADING) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT;
        return SELFTEST_FAULT_PAGE_SELECT;
    }
    return 0U;
}

/* ---- Test6: window parser writes correct page/index -----------------------
 * actual = (sample_count << 16) | last_window_address
 * 並逐筆讀回比對寫入值。
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test6_SampleWrite(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;
    uint16_t u16Index;

    pResult->actual =
        ((uint32_t)SelfTestPort_WaveSampleCount(u16Page) << 16) |
        (uint32_t)SelfTestPort_WaveLastAddress(u16Page);

    if ((SelfTestPort_WaveSampleCount(u16Page) != TEST6_SAMPLE_COUNT) ||
        (SelfTestPort_WaveLastAddress(u16Page) != TEST6_LAST_ADDR) ||
        (SelfTestPort_WaveAddressContinuous(u16Page) != 1U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }

    /* 必須仍在下載中,不得提前進入 complete */
    if ((SelfTestPort_WavePageState(u16Page) !=
         WAVE_PAGE_STATE_DOWNLOADING) ||
        (SelfTestPort_WaveDownloadComplete(u16Page) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }

    /* 取樣內容逐筆比對 (page/index 對位驗證) */
    for (u16Index = 0U; u16Index < TEST6_SAMPLE_COUNT; u16Index++) {
        if (SelfTestPort_WaveReadSample(u16Page, u16Index) !=
            TEST6_SAMPLE_VALUE(u16Index)) {
            pResult->actual =
                ((uint32_t)u16Index << 16) |
                (uint32_t)SelfTestPort_WaveReadSample(u16Page, u16Index);
            *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_SAMPLE_WRITE;
            return SELFTEST_FAULT_SAMPLE_WRITE;
        }
    }
    return 0U;
}

/* ---- Test7: DOWNLOADING → DOWNLOAD_COMPLETE -------------------------------- */
static uint16_t Validate_Test7_DownloadComplete(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    if ((SelfTestPort_WaveDownloadComplete(u16Page) != 1U) ||
        (SelfTestPort_WavePageState(u16Page) !=
         WAVE_PAGE_STATE_DOWNLOAD_COMPLETE)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }

    /* metadata 不得被 complete 動作破壞 */
    if ((SelfTestPort_WaveSampleCount(u16Page) != TEST6_SAMPLE_COUNT) ||
        (SelfTestPort_WaveAddressContinuous(u16Page) != 1U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }
    return 0U;
}

/* ---- Test8: validator gatekeeping (negative) -------------------------------
 * Page 只有 4/4096 筆 → validate 必須拒絕:
 *   - page state 結果為 INVALID (而非 VALID)
 *   - 不得 activate / 不得改變 active page
 *   - Output 必須維持 OFF (已由 step check 驗證,這裡再次確認終態)
 * 若 slave 規格定義「拒絕後維持 DOWNLOAD_COMPLETE」,
 * 調整 WAVE_PAGE_STATE_INVALID 巨集即可,引擎不變。
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test8_ValidatePrecheck(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    if (SelfTestPort_WavePageState(u16Page) == WAVE_PAGE_STATE_VALID) {
        /* 不完整 page 卻通過驗證 → validator 沒把關,屬嚴重缺陷 */
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_PRECHECK;
        return SELFTEST_FAULT_PRECHECK;
    }
    if (SelfTestPort_WavePageState(u16Page) != WAVE_PAGE_STATE_INVALID) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_PRECHECK;
        return SELFTEST_FAULT_PRECHECK;
    }
    if (SelfTestPort_WaveActivePage() == u16Page) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_PRECHECK;
        return SELFTEST_FAULT_PRECHECK;
    }
    if (SelfTestPort_OutputOn() != 0U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR;
        return SELFTEST_FAULT_OUTPUT_STATE;
    }
    return 0U;
}

/* ---- Test9: full pipeline final state ---------------------------------------
 * 步驟結果 (VALID/LOCKED) 已由 step table 逐步驗證,
 * 這裡確認 slave 端終態與 4096 筆內容完整性。
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test9_FullPipeline(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;
    uint16_t u16Index;

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    /* metadata 終態 */
    if ((SelfTestPort_WaveSelectedPage() != u16Page) ||
        (SelfTestPort_WaveSampleCount(u16Page) !=
         WAVE_PAGE_SAMPLE_COUNT) ||
        (SelfTestPort_WaveAddressContinuous(u16Page) != 1U) ||
        (SelfTestPort_WaveLastAddress(u16Page) !=
         WAVE_WINDOW_LAST_ADDR) ||
        (SelfTestPort_WaveDownloadComplete(u16Page) != 1U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* page state / active page 終態 */
    if (SelfTestPort_WavePageState(u16Page) != WAVE_PAGE_STATE_LOCKED) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }
    if (SelfTestPort_WaveActivePage() != u16Page) {
        pResult->actual = (uint32_t)SelfTestPort_WaveActivePage();
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* RAM page 流程不得觸發 Flash commit 路徑 */
    if (SelfTestPort_FlashPathIdle() != 1U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_VALIDATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* 後端儲存寫入計數 = 取樣總數 (DMA CH3 無遺失的第二佐證) */
    if (SelfTestPort_WaveWriteCount() <
        (uint32_t)WAVE_PAGE_SAMPLE_COUNT) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_SDRAM;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* 4096 筆 ramp 內容全掃描: expected = ((idx+1)*16) 截斷至 16-bit  */
    for (u16Index = 0U; u16Index < WAVE_PAGE_SAMPLE_COUNT; u16Index++) {
        uint16_t u16Expected =
            (uint16_t)(((uint32_t)u16Index + 1UL) * 16UL);
        uint16_t u16Actual =
            SelfTestPort_WaveReadSample(u16Page, u16Index);
        if (u16Actual != u16Expected) {
            pResult->actual =
                ((uint32_t)u16Index << 16) | (uint32_t)u16Actual;
            *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_SDRAM;
            return SELFTEST_FAULT_RAMP_DATA;
        }
    }

    pResult->actual = (uint32_t)WAVE_PAGE_STATE_LOCKED;
    return 0U;
}

/* ========================================================================
 * Engine core
 * ======================================================================== */
static void resetTestRecord(volatile ST_ASR5K_SPI_TEST_RESULT *pResult,
                            uint16_t u16TestId)
{
    pResult->test_id = u16TestId;
    pResult->status = ASR5K_SPI_TEST_NOT_RUN;
    pResult->expected = 0U;
    pResult->actual = 0U;
    pResult->fail_step = ASR5K_SPI_FAIL_STEP_NONE;
    pResult->current_step = 0U;
    pResult->spiA_fault = 0U;
    pResult->spiB_fault = 0U;
    pResult->error_flags = 0U;
    pResult->fault_code = 0U;
    pResult->baseline.dma_done = 0U;
    pResult->baseline.parse_ok = 0U;
    pResult->baseline.parse_fail = 0U;
    pResult->baseline.dma_restart = 0U;
    pResult->delta.dma_done = 0U;
    pResult->delta.parse_ok = 0U;
    pResult->delta.parse_fail = 0U;
    pResult->delta.dma_restart = 0U;
}

static void resetResults(void)
{
    uint16_t u16Index;

    g_asr5kSpiSelfTest.start = 0U;
    g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_RUNNING;
    setResultText('R', 'U', 'N', ' ');
    g_asr5kSpiSelfTest.current_test_id = 0U;
    g_asr5kSpiSelfTest.failed_test_id = 0U;
    g_asr5kSpiSelfTest.failed_step = ASR5K_SPI_FAIL_STEP_NONE;
    g_asr5kSpiSelfTest.fault_code = 0U;
    g_asr5kSpiSelfTest.completed_test_count = 0U;
    g_asr5kSpiSelfTest.implemented_test_count = SELFTEST_EXECUTED_COUNT;

    for (u16Index = 0U; u16Index < ASR5K_SPI_SELFTEST_RECORD_COUNT;
         u16Index++) {
        resetTestRecord(&g_asr5kSpiSelfTest.test[u16Index],
                        u16Index + 1U);
    }

    for (u16Index = 0U; u16Index < SELFTEST_EXECUTED_COUNT; u16Index++) {
        g_asr5kSpiSelfTest
            .test[(uint16_t)s_testTable[u16Index].eTestId - 1U]
            .expected = s_testTable[u16Index].u32ExpectedSummary;
    }

    s_testIndex = 0U;
    s_stepIndex = 0U;
    s_waitCount = 0U;
    s_state = SELFTEST_STATE_START;
}

static void failSelfTest(uint16_t u16FailStep, uint16_t u16Fault)
{
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult = currentResult();

    calculateDelta(pResult);
    captureFaults(pResult);
    pResult->status = ASR5K_SPI_TEST_FAIL;
    pResult->fail_step = u16FailStep;
    pResult->current_step = s_stepIndex;
    pResult->fault_code = u16Fault;
    g_asr5kSpiSelfTest.failed_test_id = pResult->test_id;
    g_asr5kSpiSelfTest.failed_step = u16FailStep;
    g_asr5kSpiSelfTest.fault_code = u16Fault;
    g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_FAIL;
    setResultText('F', 'A', 'I', 'L');
    s_state = SELFTEST_STATE_IDLE;
}

/* 啟動目前步驟。第一步驟啟動前先取 counter baseline。 */
static void startCurrentStep(void)
{
    const ST_SELFTEST_TEST *pTest = currentTest();
    const ST_SELFTEST_STEP *pStep = &pTest->pSteps[s_stepIndex];
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult = currentResult();

    if ((s_stepIndex == 0U) && (s_waitCount == 0U)) {
        readCounters(&pResult->baseline);
        pResult->status = ASR5K_SPI_TEST_RUNNING;
    }
    pResult->current_step = s_stepIndex;

    if (SelfTestPort_MasterStart(pStep->eCommand,
                                 pStep->u16Address,
                                 pStep->u16Data) != 0U) {
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_WAIT_RUNNING;
        return;
    }

    /* master busy: retry until limit */
    s_waitCount++;
    if (s_waitCount > SELFTEST_WAIT_LIMIT) {
        failSelfTest(ASR5K_SPI_FAIL_STEP_START,
                     SELFTEST_FAULT_START_REJECTED);
    }
}

/* 步驟完成: 檢查 per-step 預期值與 hook,推進到下一步驟或結束測試。 */
static void completeCurrentStep(void)
{
    const ST_SELFTEST_TEST *pTest = currentTest();
    const ST_SELFTEST_STEP *pStep = &pTest->pSteps[s_stepIndex];
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult = currentResult();
    uint16_t u16FailStep = ASR5K_SPI_FAIL_STEP_NONE;
    uint16_t u16Fault;

    /* 1) per-step master result */
    if (pStep->bCheckResult != 0U) {
        if (SelfTestPort_MasterResult16() != pStep->u16ExpectedResult) {
            pResult->actual = (uint32_t)SelfTestPort_MasterResult16();
            failSelfTest((pStep->u16FailStepOnResult !=
                          ASR5K_SPI_FAIL_STEP_NONE) ?
                             pStep->u16FailStepOnResult :
                             ASR5K_SPI_FAIL_STEP_STEP_RESULT,
                         SELFTEST_FAULT_MASTER_RESULT);
            return;
        }
    }

    /* 2) per-step hook (e.g. intermediate OUTPUT_ON state) */
    if (pStep->pfnStepCheck != 0) {
        u16Fault = pStep->pfnStepCheck(s_stepIndex, &u16FailStep);
        if (u16Fault != 0U) {
            failSelfTest(u16FailStep, u16Fault);
            return;
        }
    }

    /* 3) more steps in this test? */
    s_stepIndex++;
    if (s_stepIndex < pTest->u16StepCount) {
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_START;
        return;
    }

    /* 4) test finished: common counter validation + final validator */
    SelfTestPort_DebugCapture();
    calculateDelta(pResult);
    captureFaults(pResult);

    u16Fault = validateCounters(pResult,
                                pTest->u32DmaDoneDelta,
                                pTest->bDmaDeltaIsMinimum,
                                &u16FailStep);
    if (u16Fault != 0U) {
        failSelfTest(u16FailStep, u16Fault);
        return;
    }

    u16Fault = pTest->pfnValidate(pResult, &u16FailStep);
    if (u16Fault != 0U) {
        failSelfTest(u16FailStep, u16Fault);
        return;
    }

    /* 5) advance to next test */
    pResult->status = ASR5K_SPI_TEST_PASS;
    g_asr5kSpiSelfTest.completed_test_count++;
    s_testIndex++;
    s_stepIndex = 0U;
    s_waitCount = 0U;

    if (s_testIndex >= SELFTEST_EXECUTED_COUNT) {
        g_asr5kSpiSelfTest.current_test_id = 0U;
        g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_PASS;
        setResultText('P', 'A', 'S', 'S');
        s_state = SELFTEST_STATE_IDLE;
    } else {
        g_asr5kSpiSelfTest.current_test_id =
            (uint16_t)currentTest()->eTestId;
        s_state = SELFTEST_STATE_START;
    }
}

void Asr5kSpiSelfTest_Run(void)
{
    if (s_state == SELFTEST_STATE_IDLE) {
        if (g_asr5kSpiSelfTest.start == 1U) {
            resetResults();
            g_asr5kSpiSelfTest.current_test_id =
                (uint16_t)s_testTable[0].eTestId;
        }
        return;
    }

    if (s_state == SELFTEST_STATE_START) {
        startCurrentStep();
        return;
    }

    s_waitCount++;
    if (s_waitCount > SELFTEST_WAIT_LIMIT) {
        failSelfTest(
            (s_state == SELFTEST_STATE_WAIT_RUNNING) ?
                ASR5K_SPI_FAIL_STEP_WAIT_RUNNING :
                ASR5K_SPI_FAIL_STEP_WAIT_DONE,
            SELFTEST_FAULT_WAIT_TIMEOUT);
        return;
    }

    if (s_state == SELFTEST_STATE_WAIT_RUNNING) {
        if (SelfTestPort_MasterStatus() == SPI_TEST_STATUS_RUNNING) {
            s_state = SELFTEST_STATE_WAIT_DONE;
        } else if (SelfTestPort_MasterStatus() ==
                   SPI_TEST_STATUS_FAILED) {
            uint16_t u16Fault = getDiagnosticFault();
            failSelfTest(ASR5K_SPI_FAIL_STEP_MASTER_STATUS,
                         (u16Fault != 0U) ?
                             u16Fault : SELFTEST_FAULT_MASTER_STATUS);
        }
        return;
    }

    if (s_state == SELFTEST_STATE_STEP_DONE) {
        /* 與 master 完成間隔一個 loop,確保 slave 端解析收尾。 */
        completeCurrentStep();
        return;
    }

    /* SELFTEST_STATE_WAIT_DONE */
    if (SelfTestPort_MasterStatus() == SPI_TEST_STATUS_PASSED) {
        s_state = SELFTEST_STATE_STEP_DONE;
    } else if (SelfTestPort_MasterStatus() == SPI_TEST_STATUS_FAILED) {
        uint16_t u16Fault = getDiagnosticFault();
        failSelfTest(ASR5K_SPI_FAIL_STEP_MASTER_STATUS,
                     (u16Fault != 0U) ?
                         u16Fault : SELFTEST_FAULT_MASTER_STATUS);
    }
}

/* ========================================================================
 * UART (SCI) command interface — unchanged behaviour
 *   "spi_test all" + CR/LF → start; final line: PASS/FAIL summary
 * ======================================================================== */
static uint16_t commandMatches(void)
{
    static const char command[] = "spi_test all";
    uint16_t u16Index;

    if (s_uartRxIndex != (uint16_t)(sizeof(command) - 1U)) {
        return 0U;
    }
    for (u16Index = 0U; u16Index < s_uartRxIndex; u16Index++) {
        if (s_uartRx[u16Index] != command[u16Index]) {
            return 0U;
        }
    }
    return 1U;
}

static void uartQueueText(const char *pText)
{
    uint16_t u16Index = 0U;

    if (s_uartTxIndex < s_uartTxLength) {
        return;
    }
    while ((pText[u16Index] != '\0') &&
           (u16Index < (SELFTEST_UART_TX_SIZE - 1U))) {
        s_uartTx[u16Index] = pText[u16Index];
        u16Index++;
    }
    s_uartTxLength = u16Index;
    s_uartTxIndex = 0U;
}

static char hexDigit(uint16_t u16Value)
{
    u16Value &= 0x000FU;
    return (u16Value < 10U) ?
        (char)('0' + u16Value) :
        (char)('A' + u16Value - 10U);
}

static void appendHex16(char *pBuffer, uint16_t *pIndex, uint16_t u16Value)
{
    pBuffer[(*pIndex)++] = hexDigit(u16Value >> 12U);
    pBuffer[(*pIndex)++] = hexDigit(u16Value >> 8U);
    pBuffer[(*pIndex)++] = hexDigit(u16Value >> 4U);
    pBuffer[(*pIndex)++] = hexDigit(u16Value);
}

static void appendText(char *pBuffer, uint16_t *pIndex, const char *pText)
{
    uint16_t u16TextIndex = 0U;

    while (pText[u16TextIndex] != '\0') {
        pBuffer[(*pIndex)++] = pText[u16TextIndex++];
    }
}

static void uartQueueFinalResult(void)
{
    uint16_t u16Index = 0U;
    uint16_t u16Pass =
        (g_asr5kSpiSelfTest.status == ASR5K_SPI_SELFTEST_PASS);
    const char *pPrefix = u16Pass ? "PASS" : "FAIL";

    if (s_uartTxIndex < s_uartTxLength) {
        return;
    }

    appendText(s_uartTx, &u16Index, pPrefix);
    appendText(s_uartTx, &u16Index, " failed_test_id=");
    s_uartTx[u16Index++] = hexDigit(g_asr5kSpiSelfTest.failed_test_id);
    appendText(s_uartTx, &u16Index, " failed_step=");
    appendHex16(s_uartTx, &u16Index, g_asr5kSpiSelfTest.failed_step);
    appendText(s_uartTx, &u16Index, " fault_code=");
    appendHex16(s_uartTx, &u16Index, g_asr5kSpiSelfTest.fault_code);
    s_uartTx[u16Index++] = '\r';
    s_uartTx[u16Index++] = '\n';
    s_uartTxLength = u16Index;
    s_uartTxIndex = 0U;
}

uint16_t Asr5kSpiSelfTest_UartRxByte(uint16_t byte, uint16_t portIdle)
{
    char character = (char)(byte & 0x00FFU);

    if (s_uartCapture == 0U) {
        if ((portIdle == 0U) || (character != 's')) {
            return 0U;
        }
        s_uartCapture = 1U;
        s_uartRxIndex = 0U;
    }

    if ((character == '\r') || (character == '\n')) {
        if (commandMatches() != 0U) {
            if (g_asr5kSpiSelfTest.status ==
                ASR5K_SPI_SELFTEST_RUNNING) {
                uartQueueText("SPI_TEST BUSY\r\n");
            } else {
                g_asr5kSpiSelfTest.start = 1U;
                g_asr5kSpiSelfTest.uart_command_count++;
                uartQueueText("SPI_TEST RUN\r\n");
                s_uartLastStatus = ASR5K_SPI_SELFTEST_RUNNING;
            }
        } else {
            uartQueueText("SPI_TEST ERROR\r\n");
        }
        s_uartCapture = 0U;
        s_uartRxIndex = 0U;
        return 1U;
    }

    if (s_uartRxIndex < (SELFTEST_UART_RX_SIZE - 1U)) {
        s_uartRx[s_uartRxIndex++] = character;
    } else {
        s_uartCapture = 0U;
        s_uartRxIndex = 0U;
        uartQueueText("SPI_TEST ERROR\r\n");
    }

    return 1U;
}

void Asr5kSpiSelfTest_UartRun(uint32_t sciBase)
{
    if ((s_uartLastStatus == ASR5K_SPI_SELFTEST_RUNNING) &&
        (s_uartTxIndex >= s_uartTxLength) &&
        ((g_asr5kSpiSelfTest.status == ASR5K_SPI_SELFTEST_PASS) ||
         (g_asr5kSpiSelfTest.status == ASR5K_SPI_SELFTEST_FAIL))) {
        uartQueueFinalResult();
        s_uartLastStatus = g_asr5kSpiSelfTest.status;
    }

    if ((s_uartTxIndex < s_uartTxLength) &&
        (SCI_getTxFIFOStatus(sciBase) < SCI_FIFO_TX16)) {
        SCI_writeCharNonBlocking(
            sciBase,
            (uint16_t)(uint8_t)s_uartTx[s_uartTxIndex]);
        s_uartTxIndex++;
    }
}
