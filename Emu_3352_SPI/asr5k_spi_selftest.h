/*
 * asr5k_spi_selftest.h
 *
 * Extensible SPI Master/Slave automated test framework (table-driven).
 *
 * Test overview (Phase 2 - wave download pipeline):
 *   Test1  Register write                 (legacy, unchanged)
 *   Test2  Register read                  (legacy, unchanged)
 *   Test3  Output relay set/clear         (legacy, unchanged)
 *   Test4  SEQ block write x16            (legacy, unchanged)
 *   Test5  Wave page select               0x0958 + page_id -> selected-page metadata
 *   Test6  Wave sample write (partial)    0x3000~0x3FFF window -> page/index correctness
 *   Test7  Incomplete page status guard  Remains DOWNLOADING, RX_DONE clear
 *   Test8  Validate pre-check (negative)  sample count / continuity / checksum
 *                                         gatekeeping; incomplete page must be
 *                                         rejected and Output must be OFF
 *   Test9  Full 4096 download -> status -> validate -> activate
 *                                         long-transfer stability, zero DMA loss,
 *                                         parser counters, final page state LOCKED
 *   Test10 SPI Packet V1 PING probe       pure-C A2/A4/A5 PING->PONG executed on
 *                                         target CPU (board-observable). One benign
 *                                         register read drives the engine; the PING
 *                                         itself is validated in-memory. Does NOT
 *                                         test SPI wire transport / SPIB / DMA.
 */

#ifndef ASR5K_SPI_SELFTEST_H_
#define ASR5K_SPI_SELFTEST_H_

#include <stdint.h>

/*
 * A6-3B gated passive Packet V1 wire probe. Default 0 keeps the selftest at
 * Test1~Test10 with no wire hook and no external-master requirement. Set to 1
 * (e.g. a -DSPI_PACKET_V1_WIRE_PROBE_ENABLE=1 test build, NOT via .cproject) to
 * compile in Test11. The default MUST stay 0 here.
 */
#ifndef SPI_PACKET_V1_WIRE_PROBE_ENABLE
#define SPI_PACKET_V1_WIRE_PROBE_ENABLE 0
#endif

#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
#define ASR5K_SPI_SELFTEST_RECORD_COUNT 11U
#else
#define ASR5K_SPI_SELFTEST_RECORD_COUNT 10U
#endif

typedef enum {
    ASR5K_SPI_TEST_ID_1 = 1,
    ASR5K_SPI_TEST_ID_2,
    ASR5K_SPI_TEST_ID_3,
    ASR5K_SPI_TEST_ID_4,
    ASR5K_SPI_TEST_ID_5,
    ASR5K_SPI_TEST_ID_6,
    ASR5K_SPI_TEST_ID_7,
    ASR5K_SPI_TEST_ID_8,
    ASR5K_SPI_TEST_ID_9,
    ASR5K_SPI_TEST_ID_10
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
    ,
    ASR5K_SPI_TEST_ID_11   /* A6-3B passive Packet V1 wire receive */
#endif
} ASR5K_SPI_TEST_ID_e;

typedef enum {
    ASR5K_SPI_TEST_NOT_RUN = 0,
    ASR5K_SPI_TEST_RUNNING,
    ASR5K_SPI_TEST_PASS,
    ASR5K_SPI_TEST_FAIL,
    ASR5K_SPI_TEST_NOT_IMPLEMENTED
} ASR5K_SPI_TEST_STATUS_e;

typedef enum {
    ASR5K_SPI_SELFTEST_IDLE = 0,
    ASR5K_SPI_SELFTEST_RUNNING,
    ASR5K_SPI_SELFTEST_PASS,
    ASR5K_SPI_SELFTEST_FAIL
} ASR5K_SPI_SELFTEST_STATUS_e;

/*
 * Fail-step encoding.
 * Existing values stay fixed (host-side decoder compatibility); new items
 * are always appended at the end.
 */
typedef enum {
    ASR5K_SPI_FAIL_STEP_NONE = 0,
    ASR5K_SPI_FAIL_STEP_START,
    ASR5K_SPI_FAIL_STEP_WAIT_RUNNING,
    ASR5K_SPI_FAIL_STEP_WAIT_DONE,
    ASR5K_SPI_FAIL_STEP_MASTER_STATUS,
    ASR5K_SPI_FAIL_STEP_MASTER_RESULT,
    ASR5K_SPI_FAIL_STEP_MASTER_DETAIL,
    ASR5K_SPI_FAIL_STEP_SPI_DIAG,
    ASR5K_SPI_FAIL_STEP_DMA_DONE,
    ASR5K_SPI_FAIL_STEP_PARSE_OK,
    ASR5K_SPI_FAIL_STEP_PARSE_FAIL,
    ASR5K_SPI_FAIL_STEP_DMA_RESTART,
    ASR5K_SPI_FAIL_STEP_ERROR_FLAGS,
    ASR5K_SPI_FAIL_STEP_OUTPUT_SET,
    ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR,
    ASR5K_SPI_FAIL_STEP_BLOCK_RESULT,
    ASR5K_SPI_FAIL_STEP_RAMP_DATA,
    ASR5K_SPI_FAIL_STEP_SINE_DATA,
    ASR5K_SPI_FAIL_STEP_PACKET_STATE,
    ASR5K_SPI_FAIL_STEP_PACKET_PROTOCOL_COUNT,
    ASR5K_SPI_FAIL_STEP_PACKET_COUNTERS,
    ASR5K_SPI_FAIL_STEP_WAVE_DOWNLOAD,
    ASR5K_SPI_FAIL_STEP_WAVE_VALIDATE,
    ASR5K_SPI_FAIL_STEP_WAVE_ACTIVATE,
    ASR5K_SPI_FAIL_STEP_WAVE_SDRAM,
    /* ---- Phase 2 additions (append only) ---- */
    ASR5K_SPI_FAIL_STEP_WAVE_PAGE_SELECT,   /* Test5: page select metadata   */
    ASR5K_SPI_FAIL_STEP_WAVE_SAMPLE_WRITE,  /* Test6: window write content   */
    ASR5K_SPI_FAIL_STEP_WAVE_METADATA,      /* Test6/7: count/addr/complete  */
    ASR5K_SPI_FAIL_STEP_WAVE_PRECHECK,      /* Test8: validator gatekeeping  */
    ASR5K_SPI_FAIL_STEP_STEP_RESULT,        /* generic per-step result check */
    ASR5K_SPI_FAIL_STEP_PKTV1_PING          /* Test10: Packet V1 PING probe  */
#if (SPI_PACKET_V1_WIRE_PROBE_ENABLE == 1)
    ,
    ASR5K_SPI_FAIL_STEP_PKTV1_WIRE          /* Test11: Packet V1 wire receive */
#endif
} ASR5K_SPI_FAIL_STEP_e;

typedef struct {
    uint32_t dma_done;
    uint32_t parse_ok;
    uint32_t parse_fail;
    uint32_t dma_restart;
} ST_ASR5K_SPI_COUNTERS;

typedef struct {
    uint16_t test_id;
    ASR5K_SPI_TEST_STATUS_e status;
    uint32_t expected;
    uint32_t actual;
    uint16_t fail_step;
    uint16_t current_step;          /* step index inside the test script */
    uint16_t spiA_fault;
    uint16_t spiB_fault;
    uint16_t error_flags;
    uint16_t fault_code;
    ST_ASR5K_SPI_COUNTERS baseline;
    ST_ASR5K_SPI_COUNTERS delta;
} ST_ASR5K_SPI_TEST_RESULT;

typedef struct {
    volatile uint16_t start;
    volatile ASR5K_SPI_SELFTEST_STATUS_e status;
    volatile char result_text[5];
    volatile uint16_t current_test_id;
    volatile uint16_t failed_test_id;
    volatile uint16_t failed_step;
    volatile uint16_t fault_code;
    volatile uint16_t completed_test_count;
    volatile uint16_t implemented_test_count;
    volatile uint16_t uart_command_count;
    volatile ST_ASR5K_SPI_TEST_RESULT test[ASR5K_SPI_SELFTEST_RECORD_COUNT];
} ST_ASR5K_SPI_SELFTEST_RESULT;

extern volatile ST_ASR5K_SPI_SELFTEST_RESULT g_asr5kSpiSelfTest;

void Asr5kSpiSelfTest_Run(void);

/*
 * SCI integration hooks. UartRxByte returns 1 only when the byte belongs to
 * the ASCII self-test command and must not be passed to Modbus.
 */
uint16_t Asr5kSpiSelfTest_UartRxByte(uint16_t byte, uint16_t portIdle);
void Asr5kSpiSelfTest_UartRun(uint32_t sciBase);

#endif /* ASR5K_SPI_SELFTEST_H_ */
