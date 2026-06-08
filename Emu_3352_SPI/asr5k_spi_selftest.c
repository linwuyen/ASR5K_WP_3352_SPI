/*
 * asr5k_spi_selftest.c
 *
 * Test orchestration only. SPI traffic is started exclusively through
 * startSPIAmasterTest(); all validation uses existing results and diagnostics.
 */

#include "asr5k_spi_selftest.h"
#include "board.h"
#include "driverlib.h"
#include "SPIA_Master/SPI_master.h"
#include "SPIB_Slave/spi_slave.h"

#define SELFTEST_WAIT_LIMIT 5000000UL
#define SELFTEST_EXECUTED_COUNT 8U
#define SELFTEST_UART_RX_SIZE 16U
#define SELFTEST_UART_TX_SIZE 96U

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
#define SELFTEST_FAULT_SINE_DATA         0x300EU
#define SELFTEST_FAULT_PACKET_STATE      0x300FU
#define SELFTEST_FAULT_PACKET_PROTOCOL   0x3010U
#define SELFTEST_FAULT_PACKET_COUNTERS   0x3011U

typedef enum {
    SELFTEST_STATE_IDLE = 0,
    SELFTEST_STATE_START,
    SELFTEST_STATE_WAIT_RUNNING,
    SELFTEST_STATE_WAIT_DONE
} SELFTEST_STATE_e;

typedef struct {
    ASR5K_SPI_TEST_ID_e test_id;
    SPI_MASTER_TEST_CMD_e command;
    uint16_t address;
    uint16_t data;
    uint32_t expected;
    uint16_t implemented;
} ST_SELFTEST_COMMAND;

#pragma DATA_SECTION(s_testCommand, "asr5k_spi_selftest_config")
static const ST_SELFTEST_COMMAND
s_testCommand[ASR5K_SPI_SELFTEST_RECORD_COUNT] = {
    { ASR5K_SPI_TEST_ID_1, SPI_MASTER_TEST_CMD_WRITE,
      0x0401U, 0x1234U, 0x04471234UL, 1U },
    { ASR5K_SPI_TEST_ID_2, SPI_MASTER_TEST_CMD_READ,
      0x0400U, 0x0000U, 0x051A8A90UL, 1U },
    { ASR5K_SPI_TEST_ID_3, SPI_MASTER_TEST_CMD_WRITE,
      0x0900U, 0x0001U, 0x00010000UL, 1U },
    { ASR5K_SPI_TEST_ID_4, SPI_MASTER_TEST_CMD_SEQ_WRITE_16,
      0x0000U, 0x0000U, 0x10110010UL, 1U },
    { ASR5K_SPI_TEST_ID_5, SPI_MASTER_TEST_CMD_WAVE_4095,
      0x0000U, 0x0000U, 0x78000FFFUL, 1U },
    { ASR5K_SPI_TEST_ID_6, SPI_MASTER_TEST_CMD_SINE_4095,
      0x0000U, 0x0000U, 0U, 1U },
    { ASR5K_SPI_TEST_ID_7, SPI_MASTER_TEST_CMD_REG_FRAME_1000,
      0x0000U, 0x0000U, 1000UL, 1U },
    { ASR5K_SPI_TEST_ID_8, SPI_MASTER_TEST_CMD_PACKET_WRITE,
      0x0900U, 0x0001U, 0x00040000UL, 1U }
};

#pragma DATA_SECTION(s_executionOrder, "asr5k_spi_selftest_config")
static const ASR5K_SPI_TEST_ID_e
s_executionOrder[SELFTEST_EXECUTED_COUNT] = {
    ASR5K_SPI_TEST_ID_1,
    ASR5K_SPI_TEST_ID_2,
    ASR5K_SPI_TEST_ID_3,
    ASR5K_SPI_TEST_ID_4,
    ASR5K_SPI_TEST_ID_5,
    ASR5K_SPI_TEST_ID_6,
    ASR5K_SPI_TEST_ID_7,
    ASR5K_SPI_TEST_ID_8
};

#pragma DATA_SECTION(g_asr5kSpiSelfTest, "asr5k_spi_selftest_state")
volatile ST_ASR5K_SPI_SELFTEST_RESULT g_asr5kSpiSelfTest;

#pragma DATA_SECTION(s_state, "asr5k_spi_selftest_state")
static SELFTEST_STATE_e s_state;
#pragma DATA_SECTION(s_commandIndex, "asr5k_spi_selftest_state")
static uint16_t s_commandIndex;
#pragma DATA_SECTION(s_test3Phase, "asr5k_spi_selftest_state")
static uint16_t s_test3Phase;
#pragma DATA_SECTION(s_test3SetActual, "asr5k_spi_selftest_state")
static uint16_t s_test3SetActual;
#pragma DATA_SECTION(s_test8Phase, "asr5k_spi_selftest_state")
static uint16_t s_test8Phase;
#pragma DATA_SECTION(s_test8PacketActual, "asr5k_spi_selftest_state")
static uint16_t s_test8PacketActual;
#pragma DATA_SECTION(s_packetProtocolRxBaseline, "asr5k_spi_selftest_state")
static uint32_t s_packetProtocolRxBaseline;
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

static volatile ST_ASR5K_SPI_TEST_RESULT *getTestResult(uint16_t testId)
{
    return &g_asr5kSpiSelfTest.test[testId - 1U];
}

static const ST_SELFTEST_COMMAND *getCurrentCommand(void)
{
    return &s_testCommand[(uint16_t)s_executionOrder[s_commandIndex] - 1U];
}

static void setResultText(char c0, char c1, char c2, char c3)
{
    g_asr5kSpiSelfTest.result_text[0] = c0;
    g_asr5kSpiSelfTest.result_text[1] = c1;
    g_asr5kSpiSelfTest.result_text[2] = c2;
    g_asr5kSpiSelfTest.result_text[3] = c3;
    g_asr5kSpiSelfTest.result_text[4] = '\0';
}

static void readCounters(volatile ST_ASR5K_SPI_COUNTERS *counters)
{
    counters->dma_done = gSpibRxDmaDoneCount;
    counters->parse_ok = gSpibRxParseOkCount;
    counters->parse_fail = gSpibRxParseFailCount;
    counters->dma_restart = gSpibRxDmaRestartCount;
}

static void calculateDelta(volatile ST_ASR5K_SPI_TEST_RESULT *result)
{
    result->delta.dma_done =
        gSpibRxDmaDoneCount - result->baseline.dma_done;
    result->delta.parse_ok =
        gSpibRxParseOkCount - result->baseline.parse_ok;
    result->delta.parse_fail =
        gSpibRxParseFailCount - result->baseline.parse_fail;
    result->delta.dma_restart =
        gSpibRxDmaRestartCount - result->baseline.dma_restart;
}

static void captureFaults(volatile ST_ASR5K_SPI_TEST_RESULT *result)
{
    result->spiA_fault = spiA_master.stDiag.u16FaultCode;
    result->spiB_fault = spiB_slave.stDiag.u16FaultCode;
    result->error_flags = gSpibRxErrorFlags;
}

static uint16_t getDiagnosticFault(void)
{
    if (spiA_master.stDiag.u16FaultCode != 0U) {
        return (uint16_t)(0x1000U |
            (((uint16_t)spiA_master.stDiag.eFaultSource & 0x000FU) << 8U) |
            (spiA_master.stDiag.u16FaultCode & 0x00FFU));
    }

    if ((spiB_slave.stDiag.u16FaultCode != 0U) ||
        (gSpibRxErrorFlags != 0U)) {
        return (uint16_t)(0x2000U |
            (((uint16_t)spiB_slave.stDiag.eFaultSource & 0x000FU) << 8U) |
            ((spiB_slave.stDiag.u16FaultCode | gSpibRxErrorFlags) & 0x00FFU));
    }

    return 0U;
}

static uint16_t validateCommon(
    const volatile ST_ASR5K_SPI_TEST_RESULT *result,
    uint32_t expectedDone,
    uint16_t *failStep)
{
    uint16_t fault = getDiagnosticFault();

    if (fault != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_SPI_DIAG;
        return fault;
    }
    if (spiA_master.stTest.eStatus != SPI_TEST_STATUS_PASSED) {
        *failStep = ASR5K_SPI_FAIL_STEP_MASTER_STATUS;
        return SELFTEST_FAULT_MASTER_STATUS;
    }
    if (result->delta.dma_done != expectedDone) {
        *failStep = ASR5K_SPI_FAIL_STEP_DMA_DONE;
        return SELFTEST_FAULT_DMA_DONE_DELTA;
    }
    if (result->delta.parse_ok != expectedDone) {
        *failStep = ASR5K_SPI_FAIL_STEP_PARSE_OK;
        return SELFTEST_FAULT_PARSE_OK_DELTA;
    }
    if (result->delta.parse_fail != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_PARSE_FAIL;
        return SELFTEST_FAULT_PARSE_FAIL_DELTA;
    }
    if (result->delta.dma_restart < result->delta.dma_done) {
        *failStep = ASR5K_SPI_FAIL_STEP_DMA_RESTART;
        return SELFTEST_FAULT_DMA_RESTART_DELTA;
    }
    if (result->error_flags != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_ERROR_FLAGS;
        return SELFTEST_FAULT_SLAVE_ERROR_FLAGS;
    }

    return 0U;
}

static uint16_t validateBlockCommon(
    const volatile ST_ASR5K_SPI_TEST_RESULT *result,
    uint32_t minimumDone,
    uint16_t expectedLength,
    uint16_t *failStep)
{
    uint16_t fault = getDiagnosticFault();

    if (fault != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_SPI_DIAG;
        return fault;
    }
    if (spiA_master.stTest.eStatus != SPI_TEST_STATUS_PASSED) {
        *failStep = ASR5K_SPI_FAIL_STEP_MASTER_STATUS;
        return SELFTEST_FAULT_MASTER_STATUS;
    }
    if (result->delta.dma_done < minimumDone) {
        *failStep = ASR5K_SPI_FAIL_STEP_DMA_DONE;
        return SELFTEST_FAULT_DMA_DONE_DELTA;
    }
    if (result->delta.parse_ok != result->delta.dma_done) {
        *failStep = ASR5K_SPI_FAIL_STEP_PARSE_OK;
        return SELFTEST_FAULT_PARSE_OK_DELTA;
    }
    if (result->delta.parse_fail != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_PARSE_FAIL;
        return SELFTEST_FAULT_PARSE_FAIL_DELTA;
    }
    if (result->delta.dma_restart < result->delta.dma_done) {
        *failStep = ASR5K_SPI_FAIL_STEP_DMA_RESTART;
        return SELFTEST_FAULT_DMA_RESTART_DELTA;
    }
    if (result->error_flags != 0U) {
        *failStep = ASR5K_SPI_FAIL_STEP_ERROR_FLAGS;
        return SELFTEST_FAULT_SLAVE_ERROR_FLAGS;
    }
    if ((spiB_slave.u16BlockWriteIndex != expectedLength) ||
        (spiB_slave.u16BlockExpectedLen != expectedLength) ||
        (spiB_slave.u16BlockChecksum !=
         spiB_slave.u16BlockExpectedChecksum) ||
        (spiB_slave.u16BlockErrorCode != SPI_BLOCK_ERROR_NONE) ||
        (spiB_slave.u16BlockProgress != 100U) ||
        (spiB_slave.u16BlockStatus != 3U)) {
        *failStep = ASR5K_SPI_FAIL_STEP_BLOCK_RESULT;
        return SELFTEST_FAULT_BLOCK_RESULT;
    }

    return 0U;
}

static uint32_t findSineMismatch(void)
{
    uint16_t index;

    for (index = 0U; index < SPI_SINE_TABLE_SIZE; index++) {
        if (g_u16SpiMasterWaveRam[index] != g_u16SpiBlockRam[index]) {
            return (uint32_t)index + 1UL;
        }
    }

    return 0U;
}

static uint16_t validateCurrentTest(uint16_t *failStep)
{
    const ST_SELFTEST_COMMAND *command = getCurrentCommand();
    volatile ST_ASR5K_SPI_TEST_RESULT *result =
        getTestResult((uint16_t)command->test_id);
    uint16_t fault;

    switch (command->test_id) {
    case ASR5K_SPI_TEST_ID_1:
    case ASR5K_SPI_TEST_ID_2:
        fault = validateCommon(result, 2U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = ASR5K_SPI_FAIL_STEP_MASTER_DETAIL;
            return SELFTEST_FAULT_MASTER_DETAIL;
        }
        break;

    case ASR5K_SPI_TEST_ID_3:
        fault = validateCommon(result, 4U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = (s_test3SetActual == 0U) ?
                ASR5K_SPI_FAIL_STEP_OUTPUT_SET :
                ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR;
            return SELFTEST_FAULT_OUTPUT_STATE;
        }
        break;

    case ASR5K_SPI_TEST_ID_4:
        fault = validateBlockCommon(result, 21U, 16U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = ASR5K_SPI_FAIL_STEP_BLOCK_RESULT;
            return SELFTEST_FAULT_BLOCK_RESULT;
        }
        break;

    case ASR5K_SPI_TEST_ID_5:
        fault = validateBlockCommon(result, 4100U, 4095U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if ((result->actual != result->expected) ||
            (g_u16SpiBlockRam[0] != 0x0010U) ||
            (g_u16SpiBlockRam[1] != 0x0020U) ||
            (g_u16SpiBlockRam[2] != 0x0030U) ||
            (g_u16SpiBlockRam[4094] != 0xFFF0U)) {
            *failStep = ASR5K_SPI_FAIL_STEP_RAMP_DATA;
            return SELFTEST_FAULT_RAMP_DATA;
        }
        break;

    case ASR5K_SPI_TEST_ID_6:
        fault = validateBlockCommon(result, 4100U, 4095U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = ASR5K_SPI_FAIL_STEP_SINE_DATA;
            return SELFTEST_FAULT_SINE_DATA;
        }
        break;

    case ASR5K_SPI_TEST_ID_7:
        fault = validateCommon(result, 1000U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = ASR5K_SPI_FAIL_STEP_MASTER_RESULT;
            return SELFTEST_FAULT_MASTER_RESULT;
        }
        if (OUTPUT_ON != 1U) {
            *failStep = ASR5K_SPI_FAIL_STEP_OUTPUT_SET;
            return SELFTEST_FAULT_OUTPUT_STATE;
        }
        break;

    case ASR5K_SPI_TEST_ID_8:
        fault = validateCommon(result, 6U, failStep);
        if (fault != 0U) {
            return fault;
        }
        if (result->actual != result->expected) {
            *failStep = ASR5K_SPI_FAIL_STEP_OUTPUT_CLEAR;
            return SELFTEST_FAULT_OUTPUT_STATE;
        }
        if ((spiB_slave.ePacketState != SPIB_PACKET_STATE_IDLE) ||
            (spiB_slave.u16PacketErrorCode != SPIB_PACKET_ERROR_NONE)) {
            *failStep = ASR5K_SPI_FAIL_STEP_PACKET_STATE;
            return SELFTEST_FAULT_PACKET_STATE;
        }
        break;

    default:
        *failStep = ASR5K_SPI_FAIL_STEP_MASTER_STATUS;
        return SELFTEST_FAULT_MASTER_STATUS;
    }

    return 0U;
}

static void resetTestRecord(
    volatile ST_ASR5K_SPI_TEST_RESULT *result,
    uint16_t testId)
{
    result->test_id = testId;
    result->status = ASR5K_SPI_TEST_NOT_RUN;
    result->expected = 0U;
    result->actual = 0U;
    result->fail_step = ASR5K_SPI_FAIL_STEP_NONE;
    result->spiA_fault = 0U;
    result->spiB_fault = 0U;
    result->error_flags = 0U;
    result->fault_code = 0U;
    result->baseline.dma_done = 0U;
    result->baseline.parse_ok = 0U;
    result->baseline.parse_fail = 0U;
    result->baseline.dma_restart = 0U;
    result->delta.dma_done = 0U;
    result->delta.parse_ok = 0U;
    result->delta.parse_fail = 0U;
    result->delta.dma_restart = 0U;
}

static void resetResults(void)
{
    uint16_t index;

    g_asr5kSpiSelfTest.start = 0U;
    g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_RUNNING;
    setResultText('R', 'U', 'N', ' ');
    g_asr5kSpiSelfTest.current_test_id = 0U;
    g_asr5kSpiSelfTest.failed_test_id = 0U;
    g_asr5kSpiSelfTest.failed_step = ASR5K_SPI_FAIL_STEP_NONE;
    g_asr5kSpiSelfTest.fault_code = 0U;
    g_asr5kSpiSelfTest.completed_test_count = 0U;
    g_asr5kSpiSelfTest.implemented_test_count = SELFTEST_EXECUTED_COUNT;

    for (index = 0U; index < ASR5K_SPI_SELFTEST_RECORD_COUNT; index++) {
        resetTestRecord(&g_asr5kSpiSelfTest.test[index], index + 1U);
    }

    for (index = 0U; index < ASR5K_SPI_SELFTEST_RECORD_COUNT; index++) {
        volatile ST_ASR5K_SPI_TEST_RESULT *result =
            getTestResult((uint16_t)s_testCommand[index].test_id);
        result->expected = s_testCommand[index].expected;
        if (s_testCommand[index].implemented == 0U) {
            result->status = ASR5K_SPI_TEST_NOT_IMPLEMENTED;
        }
    }

    s_commandIndex = 0U;
    s_test3Phase = 0U;
    s_test3SetActual = 0U;
    s_test8Phase = 0U;
    s_test8PacketActual = 0U;
    s_packetProtocolRxBaseline = 0U;
    s_waitCount = 0U;
    s_state = SELFTEST_STATE_START;
}

static void failSelfTest(uint16_t failStep, uint16_t fault)
{
    const ST_SELFTEST_COMMAND *command = getCurrentCommand();
    volatile ST_ASR5K_SPI_TEST_RESULT *result =
        getTestResult((uint16_t)command->test_id);

    calculateDelta(result);
    captureFaults(result);
    result->status = ASR5K_SPI_TEST_FAIL;
    result->fail_step = failStep;
    result->fault_code = fault;
    g_asr5kSpiSelfTest.failed_test_id = result->test_id;
    g_asr5kSpiSelfTest.failed_step = failStep;
    g_asr5kSpiSelfTest.fault_code = fault;
    g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_FAIL;
    setResultText('F', 'A', 'I', 'L');
    s_state = SELFTEST_STATE_IDLE;
}

static void startCurrentCommand(void)
{
    const ST_SELFTEST_COMMAND *command = getCurrentCommand();
    volatile ST_ASR5K_SPI_TEST_RESULT *result =
        getTestResult((uint16_t)command->test_id);
    uint16_t data = command->data;
    SPI_MASTER_TEST_CMD_e masterCommand = command->command;
    uint16_t address = command->address;

    if ((command->test_id == ASR5K_SPI_TEST_ID_3) &&
        (s_test3Phase == 1U)) {
        data = 0U;
    }
    if ((command->test_id == ASR5K_SPI_TEST_ID_8) &&
        (s_test8Phase == 1U)) {
        masterCommand = SPI_MASTER_TEST_CMD_WRITE;
        address = 0x0900U;
        data = 0U;
    }

    if ((s_waitCount == 0U) &&
        (s_test3Phase == 0U) &&
        (s_test8Phase == 0U)) {
        readCounters(&result->baseline);
        result->status = ASR5K_SPI_TEST_RUNNING;
        if (command->test_id == ASR5K_SPI_TEST_ID_8) {
            s_packetProtocolRxBaseline =
                spiB_slave.stDiag.stProtocol.stComm.u32RxTotal;
        }
    }

    if (startSPIAmasterTest(masterCommand,
                            address,
                            data) != 0U) {
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_WAIT_RUNNING;
        return;
    }

    s_waitCount++;
    if (s_waitCount > SELFTEST_WAIT_LIMIT) {
        failSelfTest(ASR5K_SPI_FAIL_STEP_START,
                     SELFTEST_FAULT_START_REJECTED);
    }
}

static void completeCurrentTest(void)
{
    const ST_SELFTEST_COMMAND *command = getCurrentCommand();
    volatile ST_ASR5K_SPI_TEST_RESULT *result =
        getTestResult((uint16_t)command->test_id);
    uint16_t failStep = ASR5K_SPI_FAIL_STEP_NONE;
    uint16_t fault;

    if (command->test_id == ASR5K_SPI_TEST_ID_1 ||
        command->test_id == ASR5K_SPI_TEST_ID_2) {
        result->actual = spiA_master.stTest.u32Detail;
    } else if ((command->test_id == ASR5K_SPI_TEST_ID_4) ||
               (command->test_id == ASR5K_SPI_TEST_ID_5)) {
        result->actual =
            ((uint32_t)spiB_slave.u16BlockChecksum << 16U) |
            (uint32_t)spiB_slave.u16BlockWriteIndex;
    } else if (command->test_id == ASR5K_SPI_TEST_ID_6) {
        result->actual = findSineMismatch();
    } else {
        result->actual = spiA_master.stTest.u16Result;
    }

    if ((command->test_id == ASR5K_SPI_TEST_ID_3) &&
        (s_test3Phase == 0U)) {
        s_test3SetActual =
            ((spiA_master.stTest.u16Result == 1U) &&
             (OUTPUT_ON == 1U)) ? 1U : 0U;
        if (s_test3SetActual == 0U) {
            result->actual = 0U;
            failSelfTest(ASR5K_SPI_FAIL_STEP_OUTPUT_SET,
                         SELFTEST_FAULT_OUTPUT_STATE);
            return;
        }
        s_test3Phase = 1U;
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_START;
        return;
    }

    if (command->test_id == ASR5K_SPI_TEST_ID_3) {
        result->actual =
            ((uint32_t)s_test3SetActual << 16U) |
            (((spiA_master.stTest.u16Result == 0U) &&
              (OUTPUT_ON == 0U)) ? 0U : 1U);
    }

    if ((command->test_id == ASR5K_SPI_TEST_ID_8) &&
        (s_test8Phase == 0U)) {
        calculateDelta(result);
        captureFaults(result);
        if ((spiA_master.stTest.u16Result != 4U) ||
            (OUTPUT_ON != 1U) ||
            (spiB_slave.ePacketState != SPIB_PACKET_STATE_IDLE) ||
            (spiB_slave.u16PacketErrorCode != SPIB_PACKET_ERROR_NONE)) {
            failSelfTest(ASR5K_SPI_FAIL_STEP_PACKET_STATE,
                         SELFTEST_FAULT_PACKET_STATE);
            return;
        }
        if ((result->delta.dma_done != 4U) ||
            (result->delta.parse_ok != 4U) ||
            (result->delta.parse_fail != 0U) ||
            (result->delta.dma_restart < 4U)) {
            failSelfTest(ASR5K_SPI_FAIL_STEP_PACKET_COUNTERS,
                         SELFTEST_FAULT_PACKET_COUNTERS);
            return;
        }
        if ((spiB_slave.stDiag.stProtocol.stComm.u32RxTotal -
             s_packetProtocolRxBaseline) != 1U) {
            failSelfTest(ASR5K_SPI_FAIL_STEP_PACKET_PROTOCOL_COUNT,
                         SELFTEST_FAULT_PACKET_PROTOCOL);
            return;
        }
        s_test8PacketActual = spiA_master.stTest.u16Result;
        s_test8Phase = 1U;
        s_waitCount = 0U;
        s_state = SELFTEST_STATE_START;
        return;
    }

    if (command->test_id == ASR5K_SPI_TEST_ID_8) {
        result->actual =
            ((uint32_t)s_test8PacketActual << 16U) |
            (((spiA_master.stTest.u16Result == 0U) &&
              (OUTPUT_ON == 0U)) ? 0U : 1U);
    }

    calculateDelta(result);
    captureFaults(result);
    fault = validateCurrentTest(&failStep);
    if (fault != 0U) {
        failSelfTest(failStep, fault);
        return;
    }

    result->status = ASR5K_SPI_TEST_PASS;
    g_asr5kSpiSelfTest.completed_test_count++;
    s_commandIndex++;
    s_test3Phase = 0U;
    s_test3SetActual = 0U;
    s_test8Phase = 0U;
    s_test8PacketActual = 0U;
    s_waitCount = 0U;

    if (s_commandIndex >= SELFTEST_EXECUTED_COUNT) {
        g_asr5kSpiSelfTest.current_test_id = 0U;
        g_asr5kSpiSelfTest.status = ASR5K_SPI_SELFTEST_PASS;
        setResultText('P', 'A', 'S', 'S');
        s_state = SELFTEST_STATE_IDLE;
    } else {
        g_asr5kSpiSelfTest.current_test_id =
            (uint16_t)s_executionOrder[s_commandIndex];
        s_state = SELFTEST_STATE_START;
    }
}

static uint16_t commandMatches(void)
{
    static const char command[] = "spi_test all";
    uint16_t index;

    if (s_uartRxIndex != (uint16_t)(sizeof(command) - 1U)) {
        return 0U;
    }

    for (index = 0U; index < s_uartRxIndex; index++) {
        if (s_uartRx[index] != command[index]) {
            return 0U;
        }
    }

    return 1U;
}

static void uartQueueText(const char *text)
{
    uint16_t index = 0U;

    if (s_uartTxIndex < s_uartTxLength) {
        return;
    }

    while ((text[index] != '\0') &&
           (index < (SELFTEST_UART_TX_SIZE - 1U))) {
        s_uartTx[index] = text[index];
        index++;
    }

    s_uartTxLength = index;
    s_uartTxIndex = 0U;
}

static char hexDigit(uint16_t value)
{
    value &= 0x000FU;
    return (value < 10U) ?
        (char)('0' + value) :
        (char)('A' + value - 10U);
}

static void appendHex16(char *buffer, uint16_t *index, uint16_t value)
{
    buffer[(*index)++] = hexDigit(value >> 12U);
    buffer[(*index)++] = hexDigit(value >> 8U);
    buffer[(*index)++] = hexDigit(value >> 4U);
    buffer[(*index)++] = hexDigit(value);
}

static void appendText(char *buffer, uint16_t *index, const char *text)
{
    uint16_t textIndex = 0U;

    while (text[textIndex] != '\0') {
        buffer[(*index)++] = text[textIndex++];
    }
}

static void uartQueueFinalResult(void)
{
    uint16_t index = 0U;
    uint16_t pass =
        (g_asr5kSpiSelfTest.status == ASR5K_SPI_SELFTEST_PASS);
    const char *prefix = pass ? "PASS" : "FAIL";

    if (s_uartTxIndex < s_uartTxLength) {
        return;
    }

    appendText(s_uartTx, &index, prefix);
    appendText(s_uartTx, &index, " failed_test_id=");
    s_uartTx[index++] = hexDigit(g_asr5kSpiSelfTest.failed_test_id);
    appendText(s_uartTx, &index, " failed_step=");
    appendHex16(s_uartTx, &index, g_asr5kSpiSelfTest.failed_step);
    appendText(s_uartTx, &index, " fault_code=");
    appendHex16(s_uartTx, &index, g_asr5kSpiSelfTest.fault_code);
    s_uartTx[index++] = '\r';
    s_uartTx[index++] = '\n';
    s_uartTxLength = index;
    s_uartTxIndex = 0U;
}

void Asr5kSpiSelfTest_Run(void)
{
    if (s_state == SELFTEST_STATE_IDLE) {
        if (g_asr5kSpiSelfTest.start == 1U) {
            resetResults();
            g_asr5kSpiSelfTest.current_test_id =
                (uint16_t)s_executionOrder[0];
        }
        return;
    }

    if (s_state == SELFTEST_STATE_START) {
        startCurrentCommand();
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
        if (spiA_master.stTest.eStatus == SPI_TEST_STATUS_RUNNING) {
            s_state = SELFTEST_STATE_WAIT_DONE;
        } else if (spiA_master.stTest.eStatus == SPI_TEST_STATUS_FAILED) {
            uint16_t fault = getDiagnosticFault();
            failSelfTest(ASR5K_SPI_FAIL_STEP_MASTER_STATUS,
                         (fault != 0U) ?
                            fault : SELFTEST_FAULT_MASTER_STATUS);
        }
        return;
    }

    if (spiA_master.stTest.eStatus == SPI_TEST_STATUS_PASSED) {
        completeCurrentTest();
    } else if (spiA_master.stTest.eStatus == SPI_TEST_STATUS_FAILED) {
        uint16_t fault = getDiagnosticFault();
        failSelfTest(ASR5K_SPI_FAIL_STEP_MASTER_STATUS,
                     (fault != 0U) ?
                        fault : SELFTEST_FAULT_MASTER_STATUS);
    }
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
