/*
 * asr5k_spi_selftest.h
 *
 * Extensible SPI Master/Slave automated test framework.
 */

#ifndef ASR5K_SPI_SELFTEST_H_
#define ASR5K_SPI_SELFTEST_H_

#include <stdint.h>

#define ASR5K_SPI_SELFTEST_RECORD_COUNT 9U

typedef enum {
    ASR5K_SPI_TEST_ID_1 = 1,
    ASR5K_SPI_TEST_ID_2,
    ASR5K_SPI_TEST_ID_3,
    ASR5K_SPI_TEST_ID_4,
    ASR5K_SPI_TEST_ID_5,
    ASR5K_SPI_TEST_ID_6,
    ASR5K_SPI_TEST_ID_7,
    ASR5K_SPI_TEST_ID_8,
    ASR5K_SPI_TEST_ID_9
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
    ASR5K_SPI_FAIL_STEP_WAVE_SDRAM
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
