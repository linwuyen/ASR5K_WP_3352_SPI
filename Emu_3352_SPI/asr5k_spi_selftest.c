/*
 * asr5k_spi_selftest.c
 *
 * Table-driven SPI Master/Slave self-test engine.
 *
 * Layered architecture (ASCII-ONLY for MS950 compilation safety):
 *   [Test script tables]  each test = one ST_SELFTEST_STEP table
 *                         + one final validator.  Multi-phase tests
 *                         (Test3/9) no longer need scattered phase flags.
 *   [Engine]              generic step executor + shared counter-delta
 *                         verification.
 *   [Port layer]          asr5k_spi_selftest_port.h - all Master/Slave
 *                         dependencies live there; a future split only
 *                         changes the port.
 *
 * SPI traffic is started exclusively through the master port API; all
 * validation uses existing results and diagnostics (read-only observation).
 */

#include "asr5k_spi_selftest.h"
#include "asr5k_spi_selftest_port.h"
#include "board.h"
#include "driverlib.h"
#include "SPI_PacketV1/spi_packet_v1_probe.h"   /* Test10: pure-C PING probe */

#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
#include "SPI_PacketV1/spi_packet_v1_wire_probe.h" /* Test11: recognizer snapshot */

/* A6-3B passive wire probe accessors, defined in SPIB_Slave/spi_b_slave.c.
 * Declared locally under the same guard so spi_slave.h is NOT modified. */
extern void     SPIB_PacketV1WireProbe_Reset(void);
extern void     SPIB_PacketV1WireProbe_Arm(void);
extern void     SPIB_PacketV1WireProbe_Disarm(void);
extern uint16_t SPIB_PacketV1WireProbe_IsArmed(void);
extern void     SPIB_PacketV1WireProbe_GetSnapshot(ST_PKTV1_WIRE_PROBE *out);
#endif

/* ========================================================================
 * Engine configuration
 * ======================================================================== */
#define SELFTEST_WAIT_LIMIT      5000000UL
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
#define SELFTEST_EXECUTED_COUNT  11U
#else
#define SELFTEST_EXECUTED_COUNT  10U
#endif
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
#define SELFTEST_FAULT_PKTV1_PING        0x3017U  /* Test10 PING probe      */
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
#define SELFTEST_FAULT_PKTV1_WIRE_TIMEOUT  0x3018U /* Test11 no external frame */
#define SELFTEST_FAULT_PKTV1_WIRE_BAD_FRAME 0x3019U /* Test11 bad cmd/len/CRC  */
#define PKTV1_WIRE_TEST_CMD_PING           0x8000U /* expected wire cmd_id     */
#define PKTV1_WIRE_RX_OK_MARKER            0x504FU /* receive-OK marker (NOT a PONG) */
/* Bounded wait for the external master's frame (poll-count, not wall-clock).
 * Finite by construction so Test11 can never hang; tune on-board if needed. */
#define SELFTEST_PKTV1_WIRE_WAIT_LIMIT     (SELFTEST_WAIT_LIMIT * 40UL)
#endif

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
 * Called right after the master reports PASSED for the step (e.g. Test3
 * verifies the intermediate OUTPUT_ON state per step).  Returns 0 on pass;
 * non-zero is a fault code, with *pFailStep reporting the fail location.
 */
typedef uint16_t (*PFN_STEP_CHECK)(uint16_t u16StepIndex,
                                   uint16_t *pFailStep);

/*
 * Test-level final validator.
 * The engine has already computed counter deltas and run the shared
 * checks; the validator only inspects test-specific slave-side state
 * and fills in result->actual.
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
static uint16_t StepCheck_WaveStatusIncomplete(uint16_t u16StepIndex, uint16_t *pFailStep);
static uint16_t StepCheck_WaveStatusReady(uint16_t u16StepIndex, uint16_t *pFailStep);
static uint16_t WaveStatusResponseIsValid(void);

static uint16_t Validate_Test1_RegisterWrite(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test2_RegisterRead(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test3_OutputToggle(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test4_BlockWrite16(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test5_PageSelect(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test6_SampleWrite(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test7_IncompleteStatus(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test8_ValidatePrecheck(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test9_FullPipeline(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
static uint16_t Validate_Test10_PacketV1Ping(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
static uint16_t Validate_Test11_PktV1Wire(volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep);
#endif

/* ========================================================================
 * Test scripts
 * To add a test: add one step table + one validator + one s_testTable row.
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
 * Write page_id to 0x0958; confirm selected-page metadata initializes.   */
#pragma DATA_SECTION(s_test5Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test5Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_PAGE_SELECT_ADDR, SELFTEST_TARGET_PAGE,
      SELFTEST_TARGET_PAGE, 1U, ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT, 0 }
};

/* ---- Test6: partial sample write into 0x3000 window ---------------------
 * Write 4 samples; confirm the window parser hits the right page/index. */
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

/* ---- Test7: partial-download status / incomplete-page guard -------------- */
#pragma DATA_SECTION(s_test7Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test7Steps[] = {
    { SPI_MASTER_TEST_CMD_READ, WAVE_PAGE_STATUS_ADDR, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_WAVE_METADATA,
      StepCheck_WaveStatusIncomplete }
};

/* ---- Test8: validate pre-check (NEGATIVE test) ---------------------------
 * Test6 wrote only 4/4096 samples, so the validator must reject the page
 * for insufficient sample count.  Also confirm Output stays OFF
 * (validation must never engage the power stage).                        */
#pragma DATA_SECTION(s_test8Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test8Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_VALIDATE_ADDR, 0x0001U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, StepCheck_OutputIsOff }
};

/* ---- Test9: full pipeline -------------------------------------------------
 * select -> full 4096 download -> status-ready -> validate(VALID)
 *        -> activate(LOCKED)
 * Focus: no DMA CH3 loss during the long transfer (delta counters agree)
 * and the correct final state.                                            */
#pragma DATA_SECTION(s_test9Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test9Steps[] = {
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_PAGE_SELECT_ADDR, SELFTEST_TARGET_PAGE,
      SELFTEST_TARGET_PAGE, 1U, ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT, 0 },
    { SPI_MASTER_TEST_CMD_WAVE_DOWNLOAD, 0x0000U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD, 0 },
    { SPI_MASTER_TEST_CMD_READ, WAVE_PAGE_STATUS_ADDR, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD,
      StepCheck_WaveStatusReady },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_VALIDATE_ADDR, 0x0001U,
      WAVE_PAGE_STATE_VALID, 1U, ASR5K_SPI_FAIL_STEP_WAVE_VALIDATE, 0 },
    { SPI_MASTER_TEST_CMD_WRITE, WAVE_ACTIVATE_ADDR, 0x0001U,
      WAVE_PAGE_STATE_LOCKED, 1U, ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE, 0 }
};

/* ---- Test10: SPI Packet V1 PING probe -----------------------------------
 * One benign register read (identical to Test2: READ 0x0400) drives the step
 * engine and re-confirms the legacy path; the Packet V1 PING->PONG itself is
 * validated purely in-memory in Validate_Test10_PacketV1Ping().            */
#pragma DATA_SECTION(s_test10Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test10Steps[] = {
    { SPI_MASTER_TEST_CMD_READ, 0x0400U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};

#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
/* ---- Test11: passive Packet V1 wire receive -----------------------------
 * No SPIA master command is issued. The engine diverts Test11 to a dedicated
 * arm/wait/disarm flow (selfTestRunPktV1Wire); this benign NONE step entry only
 * satisfies the table shape and is never executed. */
#pragma DATA_SECTION(s_test11Steps, "asr5k_spi_selftest_config")
static const ST_SELFTEST_STEP s_test11Steps[] = {
    { SPI_MASTER_TEST_CMD_NONE, 0x0000U, 0x0000U,
      0U, 0U, ASR5K_SPI_FAIL_STEP_NONE, 0 }
};
#endif

/* ---- Master test table ----------------------------------------------------
 * u32DmaDoneDelta: one register write/read = 2 frames.
 *   T5: 1*2   T6: 4*2   T7: 1*2   T8: 1*2
 *   T9: measured successful status/validate/activate path produces a
 *       minimum of 4107 DMA/parser completions.  The threshold remains a
 *       minimum because STATUS_POLL may retry.
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
      (uint32_t)WAVE_PAGE_STATE_DOWNLOADING, 2U, 0U,
      Validate_Test7_IncompleteStatus },

    { ASR5K_SPI_TEST_ID_8, s_test8Steps, 1U,
      (uint32_t)WAVE_PAGE_STATE_INVALID, 2U, 0U,
      Validate_Test8_ValidatePrecheck },

    { ASR5K_SPI_TEST_ID_9, s_test9Steps, 5U,
      (uint32_t)WAVE_PAGE_STATE_LOCKED, 4107U, 1U,
      Validate_Test9_FullPipeline },

    { ASR5K_SPI_TEST_ID_10, s_test10Steps, 1U,
      0x8000504FUL, 2U, 0U, Validate_Test10_PacketV1Ping }
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
    ,
    /* Test11 expects actual == 0x8000504F (cmd 0x8000 | RX-OK marker 0x504F).
     * DMA-done delta is not engine-validated for Test11 (custom flow). */
    { ASR5K_SPI_TEST_ID_11, s_test11Steps, 1U,
      0x8000504FUL, 0U, 0U, Validate_Test11_PktV1Wire }
#endif
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
 * Consistency check of DMA / parser deltas accumulated over the whole
 * test (all steps):
 *   - dma_done matches expectation (exact or minimum)
 *   - parse_ok == dma_done       (every frame parsed successfully)
 *   - parse_fail == 0
 *   - dma_restart >= dma_done    (DMA re-armed after every frame)
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

static uint16_t StepCheck_WaveStatusIncomplete(uint16_t u16StepIndex,
                                                uint16_t *pFailStep)
{
    uint16_t u16Status = SelfTestPort_MasterResult16();

    (void)u16StepIndex;
    if ((WaveStatusResponseIsValid() == 0U) ||
        ((u16Status & WAVE_STATUS_RX_DONE) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }
    return 0U;
}

static uint16_t StepCheck_WaveStatusReady(uint16_t u16StepIndex,
                                          uint16_t *pFailStep)
{
    uint16_t u16Status = SelfTestPort_MasterResult16();

    (void)u16StepIndex;
    if ((WaveStatusResponseIsValid() == 0U) ||
        ((u16Status & WAVE_STATUS_READY_MASK) !=
         WAVE_STATUS_READY_MASK) ||
        ((u16Status & WAVE_STATUS_ERROR) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD;
        return SELFTEST_FAULT_WAVE_METADATA;
    }
    return 0U;
}

static uint16_t WaveStatusResponseIsValid(void)
{
    uint32_t u32Detail = SelfTestPort_MasterDetail32();
    uint16_t u16RxAddr = (uint16_t)(u32Detail >> 16U);
    uint16_t u16RxData = (uint16_t)u32Detail;
    uint16_t u16ExpectedAddr =
        (uint16_t)(WAVE_PAGE_STATUS_ADDR +
                   (u16RxData & 0x00FFU) +
                   (u16RxData >> 8U));

    if ((u16RxData &
         (uint16_t)~(WAVE_STATUS_READY_MASK | WAVE_STATUS_ERROR)) != 0U) {
        return 0U;
    }

    return (u16RxAddr == u16ExpectedAddr) ? 1U : 0U;
}

/* ========================================================================
 * Test validators (final, test-specific checks)
 * Convention: fill result->actual first (host observable), then return
 * the fault code.
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
    /* Intermediate states were verified by step checks; only summarize
     * the final state here. */
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
 * After writing 0x0958:
 *   - u16SelectedPage == target page
 *   - page metadata reset (count=0, complete=false)
 *   - page state enters DOWNLOADING
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
 * Also read back every sample and compare against the written value.
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

    /* Must still be downloading; must not enter complete early. */
    if ((SelfTestPort_WavePageState(u16Page) !=
         WAVE_PAGE_STATE_DOWNLOADING) ||
        (SelfTestPort_WaveDownloadComplete(u16Page) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }

    /* Compare sample content one by one (page/index alignment check). */
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

/* ---- Test7: partial download remains incomplete --------------------------- */
static uint16_t Validate_Test7_IncompleteStatus(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;
    uint16_t u16Status = SelfTestPort_MasterResult16();

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    if ((SelfTestPort_WaveDownloadComplete(u16Page) != 0U) ||
        (SelfTestPort_WavePageState(u16Page) !=
         WAVE_PAGE_STATE_DOWNLOADING) ||
        ((u16Status & WAVE_STATUS_RX_DONE) != 0U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }

    /* Reading status must not alter the partial-download metadata. */
    if ((SelfTestPort_WaveSampleCount(u16Page) != TEST6_SAMPLE_COUNT) ||
        (SelfTestPort_WaveAddressContinuous(u16Page) != 1U)) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_METADATA;
        return SELFTEST_FAULT_WAVE_METADATA;
    }
    return 0U;
}

/* ---- Test8: validator gatekeeping (negative) -------------------------------
 * Page holds only 4/4096 samples -> validate must reject:
 *   - resulting page state is INVALID (not VALID)
 *   - must not activate / must not change the active page
 *   - Output must stay OFF (already verified by step check; re-confirm
 *     the final state here)
 * If the slave spec defines "stay DOWNLOAD_COMPLETE after rejection",
 * adjust the WAVE_PAGE_STATE_INVALID macro; the engine is unchanged.
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test8_ValidatePrecheck(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    if (SelfTestPort_WavePageState(u16Page) == WAVE_PAGE_STATE_VALID) {
        /* Incomplete page passed validation -> validator failed to
         * gate; this is a severe defect. */
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
 * Per-step results (VALID/LOCKED) were verified by the step table; here
 * we confirm the slave-side final state and full 4096-sample integrity.
 * -------------------------------------------------------------------------- */
static uint16_t Validate_Test9_FullPipeline(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    uint16_t u16Page = SELFTEST_TARGET_PAGE;
    uint16_t u16Index;

    pResult->actual = (uint32_t)SelfTestPort_WavePageState(u16Page);

    /* Final metadata state. */
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

    /* Final page state / active page. */
    if (SelfTestPort_WavePageState(u16Page) != WAVE_PAGE_STATE_LOCKED) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }
    if (SelfTestPort_WaveActivePage() != u16Page) {
        pResult->actual = (uint32_t)SelfTestPort_WaveActivePage();
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* The RAM-page flow must not trigger the Flash commit path. */
    if (SelfTestPort_FlashPathIdle() != 1U) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_VALIDATE;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* Storage write count == total samples (second proof that DMA CH3
     * lost nothing). */
    if (SelfTestPort_WaveWriteCount() <
        (uint32_t)WAVE_PAGE_SAMPLE_COUNT) {
        *pFailStep = ASR5K_SPI_FAIL_STEP_WAVE_SDRAM;
        return SELFTEST_FAULT_WAVE_FINAL;
    }

    /* Full scan of all 4096 ramp samples:
     * expected = ((idx+1)*16) truncated to 16-bit. */
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

/* ---- Test10: SPI Packet V1 PING probe (pure-C logic on target CPU) -------
 * Board-observable proof that the A2/A4/A5 PING->PONG path executes correctly
 * on the target. The single benign register read (Test10 step) only drives the
 * existing step engine; the Packet V1 PING itself is built and checked entirely
 * in memory here. This does NOT exercise SPI wire transport / SPIB / DMA.    */
static uint16_t Validate_Test10_PacketV1Ping(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    ST_PKTV1_PROBE_RESULT probe;
    PKTV1_PROBE_RESULT_e  rc = SpiPacketV1_RunPingProbe(&probe);

    if (rc != PKTV1_PROBE_OK) {
        /* Pack per-stage diagnostics into actual for failure analysis. */
        pResult->actual =
            (((uint32_t)probe.result          & 0xFFUL) << 24) |
            (((uint32_t)probe.loopback_result & 0xFFUL) << 16) |
            (((uint32_t)probe.parser_result   & 0xFFUL) << 8)  |
             ((uint32_t)probe.dispatch_result & 0xFFUL);
        *pFailStep = ASR5K_SPI_FAIL_STEP_PKTV1_PING;
        return SELFTEST_FAULT_PKTV1_PING;
    }

    /* Success: observable actual = response_cmd<<16 | pong = 0x8000504F. */
    pResult->actual =
        ((uint32_t)probe.response_cmd << 16) | (uint32_t)probe.pong_word;
    return 0U;
}

#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
/* ---- Test11: passive Packet V1 wire receive (table placeholder validator) -
 * Test11 is handled by the dedicated arm/wait/disarm flow selfTestRunPktV1Wire()
 * and never reaches the generic step/validator path. This stub only fills the
 * test-table row's function pointer; it is not executed. */
static uint16_t Validate_Test11_PktV1Wire(
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult, uint16_t *pFailStep)
{
    (void)pResult;
    (void)pFailStep;
    return 0U;
}
#endif

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
    g_u32DiagMasterBurstDoneTick = 0U;
    g_u32DiagMasterWaitAckStartTick = 0U;
    g_u32DiagMasterWaitAckFailTick = 0U;
    g_u16DiagMasterLastTxCmd = 0U;
    g_u16DiagMasterLastTxData = 0U;
    g_u16DiagMasterGateSeen = 0U;

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
        if (!((pTest->eTestId == ASR5K_SPI_TEST_ID_9) &&
              (pStep->eCommand == SPI_MASTER_TEST_CMD_READ) &&
              (pStep->u16Address == WAVE_PAGE_STATUS_ADDR))) {
            s_waitCount = 0U;
        }
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

    /* The burst-to-register seam may consume the first status request or
     * return stale filler.  Retry this step until a valid READY response
     * arrives; a real slave ERROR is handled by the step check below. */
    if ((pTest->eTestId == ASR5K_SPI_TEST_ID_9) &&
        (pStep->eCommand == SPI_MASTER_TEST_CMD_READ) &&
        (pStep->u16Address == WAVE_PAGE_STATUS_ADDR)) {
        uint16_t u16Status = SelfTestPort_MasterResult16();

        if ((WaveStatusResponseIsValid() == 0U) ||
            (((u16Status & WAVE_STATUS_READY_MASK) !=
              WAVE_STATUS_READY_MASK) &&
             ((u16Status & WAVE_STATUS_ERROR) == 0U))) {
            if (s_waitCount > SELFTEST_WAIT_LIMIT) {
                failSelfTest(ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD,
                             SELFTEST_FAULT_WAIT_TIMEOUT);
            } else {
                s_state = SELFTEST_STATE_START;
            }
            return;
        }
    }

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

#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
/* Test11: passive Packet V1 wire receive. No SPIA master, no UART, no TX/PONG.
 * Arms the SPIB passive probe, waits a bounded number of polls for ONE external
 * Packet V1 PING frame, records the receive result on the existing selftest
 * surface, and disarms on success / bad-frame / timeout. */
static void selfTestRunPktV1Wire(void)
{
    volatile ST_ASR5K_SPI_TEST_RESULT *pResult = currentResult();
    ST_PKTV1_WIRE_PROBE snap;

    if (s_state == SELFTEST_STATE_START) {
        readCounters(&pResult->baseline);
        pResult->status = ASR5K_SPI_TEST_RUNNING;
        pResult->current_step = 0U;
        SPIB_PacketV1WireProbe_Arm();        /* arm + reset recognizer */
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_WAIT_DONE;
        return;
    }

    SPIB_PacketV1WireProbe_GetSnapshot(&snap);

    if (snap.state == PKTV1_WIRE_PROBE_FRAME_OK) {
        SPIB_PacketV1WireProbe_Disarm();
        calculateDelta(pResult);
        captureFaults(pResult);
        if ((snap.cmd_id == PKTV1_WIRE_TEST_CMD_PING) &&
            (snap.payload_words == 0U)) {
            /* actual = received cmd_id<<16 | receive-OK marker = 0x8000504F. */
            pResult->actual = ((uint32_t)snap.cmd_id << 16) |
                              (uint32_t)PKTV1_WIRE_RX_OK_MARKER;
            pResult->status = ASR5K_SPI_TEST_PASS;
            g_asr5kSpiSelfTest.completed_test_count++;
            s_testIndex++;
            s_stepIndex = 0U;
            s_waitCount = 0U;
            g_asr5kSpiSelfTest.current_test_id = 0U;
            g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_PASS;
            setResultText('P', 'A', 'S', 'S');
            s_state = SELFTEST_STATE_IDLE;
        } else {
            pResult->actual = ((uint32_t)snap.cmd_id << 16) |
                              (uint32_t)snap.payload_words;
            failSelfTest(ASR5K_SPI_FAIL_STEP_PKTV1_WIRE,
                         SELFTEST_FAULT_PKTV1_WIRE_BAD_FRAME);
        }
        return;
    }

    if (snap.state == PKTV1_WIRE_PROBE_FRAME_ERROR) {
        SPIB_PacketV1WireProbe_Disarm();
        pResult->actual = ((uint32_t)snap.result << 24) |
                          ((uint32_t)snap.crc_actual & 0xFFFFUL);
        failSelfTest(ASR5K_SPI_FAIL_STEP_PKTV1_WIRE,
                     SELFTEST_FAULT_PKTV1_WIRE_BAD_FRAME);
        return;
    }

    /* IDLE or COLLECTING: bounded wait for the external master's frame. */
    s_waitCount++;
    if (s_waitCount > SELFTEST_PKTV1_WIRE_WAIT_LIMIT) {
        SPIB_PacketV1WireProbe_Disarm();
        failSelfTest(ASR5K_SPI_FAIL_STEP_PKTV1_WIRE,
                     SELFTEST_FAULT_PKTV1_WIRE_TIMEOUT);
    }
}
#endif /* SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1 */

void Asr5kSpiSelfTest_Run(void)
{
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
    /* Test11 uses a passive-receive flow instead of a SPIA master step. */
    if ((s_state != SELFTEST_STATE_IDLE) &&
        (currentTest()->eTestId == ASR5K_SPI_TEST_ID_11)) {
        selfTestRunPktV1Wire();
        return;
    }
#endif

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
