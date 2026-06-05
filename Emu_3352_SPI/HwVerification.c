/*
 * HwVerification.c (Preview Sandbox)
 *
 * Created on: 2026/05/08
 * Author: Antigravity
 * Description: Hardware validation routing and central diagnostics monitor.
 */

#include "HwVerification.h"
#include <math.h>
#include "board.h"
#include "i2c.h"
#include "asysctl.h"
#include "fsi.h"
#include "timetask.h"

// ==============================================================================
// Global Instance
// ==============================================================================
ST_HWTEST g_hwTest = { .u16DacRawSet = 2048, .f32DacVoltSet = 1.5f };

// ==============================================================================
// Private Scheduler and Flash State
// ==============================================================================
typedef struct {
    u32 u32LastTick_A;
    u32 u32LastTick_Sdram;
    u16 u16Initialized;
} ST_HWVERIF_SCHED;

static ST_HWVERIF_SCHED s_sched = { 0U, 0U, 0U };
static u16 s_u16FlashState = 0U; /* 0:Idle, 1:Wait */
static u16 s_u16FlashExp   = 0U;
static u32 s_u32FlashTimer = 0U;

static inline u32 calcDelta(u32 u32Now, u32 u32Last) {
    return (u32Now >= u32Last) ? (u32Now - u32Last) : ((SW_TIMER - u32Last) + u32Now);
}

void HwVerification_Init(void) {
    g_hwTest.stFlash.u32ID = 0U;
    g_hwTest.stFlash.u32Status = 0U;
    g_hwTest.stFlash.u32Data = 0U;
    g_hwTest.stFlash.u16Trigger = 0U;
    g_hwTest.stFlash.u16RxCount = 0U;
    s_sched.u16Initialized = 1U;
    g_hwTest.stSdram.u16Ctrl = 0U;
    s_u16FlashState = 0U;

    /* Initialize external SPI Flash diagnostics structure */
    CommDiag_Init((ST_COMM_DIAG *)&g_hwTest.stFlash.stComm);

    /* Explicitly initialize DAC-A for physical oscilloscope testing */
    EALLOW;
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 0U);
    EDIS;
}

void HwVerification_RunAllTests(void) {
    u32 u32Now = U32_UPCNTS;
    if (s_sched.u16Initialized == 0U) {
        HwVerification_Init();
    }
    
    if (calcDelta(u32Now, s_sched.u32LastTick_A) >= T_2D5MS) {
        HwVerification_ADC_RunLoopback();
        HwVerification_UpdateMonitor();
        s_sched.u32LastTick_A = u32Now;
    }
    
    /* Optional runtime FLASH test execution based on triggers */
    HwVerification_FLASH_RunTest();
    
    if (g_hwTest.stSdram.u16Ctrl == 1U) { /* Mode 1: Single Trigger */
        HwVerification_SDRAM_RunTest();
        if (g_hwTest.stSdram.u16Ctrl == 1U) { /* Basic test passed */
            HwVerification_SDRAM_RunStressTest();
        }
        if (g_hwTest.stSdram.u16Ctrl == 1U) { /* Both tests passed */
            g_hwTest.stSdram.u16Ctrl = 0U;    /* Return to idle/pass state */
        }
    }
    else if (g_hwTest.stSdram.u16Ctrl == 2U) { /* Mode 2: Continuous Burn-in (100ms) */
        if (calcDelta(u32Now, s_sched.u32LastTick_Sdram) >= T_100MS) {
            HwVerification_SDRAM_RunStressTest();
            s_sched.u32LastTick_Sdram = u32Now;
        }
    }
    else if (g_hwTest.u16SPIBLOOP == 1U) {
        HwVerification_SPIB_RunLoopbackTest();
        g_hwTest.u16SPIBLOOP = 0U;
    }
}

void HwVerification_FLASH_RunTest(void) {
    u32 u32Base = SPIA_BASE;

    if (s_u16FlashState == 0U) { /* IDLE */
        u16 u16Trig = g_hwTest.stFlash.u16Trigger;
        if (u16Trig == 0U) {
            return;
        }
        g_hwTest.stFlash.u16Trigger = 0U;
        SPI_resetRxFIFO(u32Base);
        s_u32FlashTimer = U32_UPCNTS;

        if (u16Trig == 1U) { /* Read ID */
            SPI_writeDataNonBlocking(u32Base, 0x9F00U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            s_u16FlashExp = 4U;
        } else if (u16Trig == 2U) { /* WREN */
            SPI_writeDataNonBlocking(u32Base, 0x0600U);
            s_u16FlashExp = 1U;
        } else if (u16Trig == 3U) { /* Erase Sector 0 */
            SPI_writeDataNonBlocking(u32Base, 0x2000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            s_u16FlashExp = 4U;
        } else if (u16Trig == 4U) { /* Read Data @ 0 */
            SPI_writeDataNonBlocking(u32Base, 0x0300U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            s_u16FlashExp = 6U;
        } else if (u16Trig == 5U) { /* Write Data @ 0 */
            SPI_writeDataNonBlocking(u32Base, 0x0200U); /* Cmd */
            SPI_writeDataNonBlocking(u32Base, 0x0000U); /* Addr 0 */
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x0000U);
            SPI_writeDataNonBlocking(u32Base, 0x5500U); /* Data 1 */
            SPI_writeDataNonBlocking(u32Base, 0xAA00U); /* Data 2 */
            s_u16FlashExp = 6U;
        }
        s_u16FlashState = 1U;
    } else { /* WAIT */
        u32 u32Elapsed = U32_UPCNTS;
        if (u32Elapsed >= s_u32FlashTimer) {
            u32Elapsed = u32Elapsed - s_u32FlashTimer;
        } else {
            u32Elapsed = (SW_TIMER - s_u32FlashTimer) + u32Elapsed;
        }

        if (SPI_getRxFIFOStatus(u32Base) < s_u16FlashExp || SPI_isBusy(u32Base)) {
            if (u32Elapsed > T_5MS) {
                /* Timeout Error */
                CommDiag_ReportError((ST_COMM_DIAG *)&g_hwTest.stFlash.stComm, 1U, s_u16FlashState, U32_UPCNTS, s_u16FlashExp, 0U, 0U, 0U);
                s_u16FlashState = 0U;
            }
            return;
        }

        u16 u16Buf[8];
        u16 u16Idx;
        for (u16Idx = 0U; u16Idx < s_u16FlashExp; u16Idx++) {
            u16Buf[u16Idx] = SPI_readDataNonBlocking(u32Base) & 0xFFU;
        }

        if (s_u16FlashExp == 4U) { /* ID Result */
            g_hwTest.stFlash.u32ID = ((u32)u16Buf[1] << 16) | ((u32)u16Buf[2] << 8) | u16Buf[3];
            if ((g_hwTest.stFlash.u32ID == 0U) || (g_hwTest.stFlash.u32ID == 0xFFFFFFUL)) {
                CommDiag_ReportError((ST_COMM_DIAG *)&g_hwTest.stFlash.stComm, 2U, s_u16FlashState, U32_UPCNTS, (u16)(g_hwTest.stFlash.u32ID & 0xFFFFU), (u16)(g_hwTest.stFlash.u32ID >> 16), 0U, 0U);
            } else {
                g_hwTest.stFlash.stComm.u32TxTotal++; /* Count success */
                g_hwTest.stFlash.u16RxCount = (u16)g_hwTest.stFlash.stComm.u32TxTotal;
            }
        } else if (s_u16FlashExp == 6U) { /* Read Result */
            g_hwTest.stFlash.u32Data = ((u32)u16Buf[4] << 8) | u16Buf[5];
            g_hwTest.stFlash.stComm.u32TxTotal++; /* Count success */
            g_hwTest.stFlash.u16RxCount = (u16)g_hwTest.stFlash.stComm.u32TxTotal;
        }
        s_u16FlashState = 0U;
    }
}

void HwVerification_ADC_RunLoopback(void) {
    if (spiB_slave.u16BlockStatus == 3U) {
        static u16 s_u16PlayIdx = 0U;
        g_hwTest.u16DacRawSet = g_u16SpiBlockRam[s_u16PlayIdx];
        s_u16PlayIdx++;
        if (s_u16PlayIdx >= spiB_slave.u16BlockExpectedLen) {
            s_u16PlayIdx = 0U;
        }
    }
    DAC_setShadowValue(DACA_BASE, g_hwTest.u16DacRawSet & 0x0FFF);
    ADC_forceSOC(ADCA_BASE, ADC_SOC_NUMBER0);
    if (ADC_getInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1) == true) {
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
        g_hwTest.u16AdcRaw = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
        g_hwTest.f32AdcVolt = (f32)g_hwTest.u16AdcRaw * 0.0007326f;
    }
}

void HwVerification_UpdateMonitor(void) {
    u16 r = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
    ADC_forceSOC(ADCA_BASE, ADC_SOC_NUMBER1); 
    g_hwTest.f32McuTemp = ADC_getTemperatureC(r, 3.0f);

    /* Single-point diagnostic routing from decoupled SPI Master and Slave components */
    g_hwTest.stSpiA = spiA_master.stDiag;
    g_hwTest.stSpiB = spiB_slave.stDiag;
}

void HwVerification_FSI_RunTest(void) {}

#define SDRAM_START_ADDR  0x80000000UL

void HwVerification_SDRAM_RunTest(void) {
    volatile u32 *pu32Mem = (u32 *)SDRAM_START_ADDR;
    u32 u32Index;
    u32 u32TestPattern;
    
    for (u32Index = 0U; u32Index < 32U; u32Index++) {
        u32TestPattern = (1UL << u32Index);
        *pu32Mem = u32TestPattern;
        if (*pu32Mem != u32TestPattern) {
            g_hwTest.stSdram.u16Ctrl = 0x8000U | (u16)(u32Index & 0x001FU);
            return;
        }
    }
}

#define STRESS_BLOCK_SIZE 1000UL

void HwVerification_SDRAM_RunStressTest(void) {
    volatile u32 *pu32Mem = (u32 *)SDRAM_START_ADDR;
    u32 u32Index;
    u16 u16ErrorDetected = 0U;
    
    for (u32Index = 0U; u32Index < STRESS_BLOCK_SIZE; u32Index++) {
        pu32Mem[u32Index] = ~((u32)&pu32Mem[u32Index]);
    }
    
    for (u32Index = 0U; u32Index < STRESS_BLOCK_SIZE; u32Index++) {
        u32 u32ExpectedData = ~((u32)&pu32Mem[u32Index]);
        u32 u32ReadData = pu32Mem[u32Index];
        if (u32ReadData != u32ExpectedData) {
            g_hwTest.stSdram.u32FailAddr = (u32)&pu32Mem[u32Index];
            g_hwTest.stSdram.u32FailRead = u32ReadData;
            g_hwTest.stSdram.u16Ctrl = 0x8100U;
            u16ErrorDetected = 1U;
            break;
        }
    }
    
    if (u16ErrorDetected == 1U) {
        g_hwTest.stSdram.u32StressErr++;
    } else {
        g_hwTest.stSdram.u32StressPass++;
    }
}

extern volatile u32 g_u32DebugLastTx;

void HwVerification_SPIB_RunLoopbackTest(void) {
    u32 u32Base = SPIB_SYSTEM_BASE;
    u16 u16SimulatedGroupCmd = 0x0401U; /* Group=0x04 (Version), ID=0x01 */
    u16 u16SimulatedData     = 0x0000U;
    u16 u16Iter;

    if (spiB_slave.fsm == _INIT_SPI_AS_SLAVE) {
        runSPIBslave();
    }

    SPI_enableLoopback(u32Base);
    SPI_resetTxFIFO(u32Base);
    SPI_resetRxFIFO(u32Base);
    g_u32DebugLastTx = 0U;

    SPI_writeDataNonBlocking(u32Base, u16SimulatedGroupCmd);
    SPI_writeDataNonBlocking(u32Base, u16SimulatedData);

    for (u16Iter = 0U; u16Iter < 2U; u16Iter++) {
        runSPIBslave();
    }

    SPI_disableLoopback(u32Base);
}

void HwVerification_UpdateIO(void) {}
void HwVerification_I2C_RunTest(void) {}
void HwVerification_BasicIO_Init(void) {}
void HwVerification_BasicIO_RunHeartbeat(void) {}
void HwVerification_Advanced_Init(void) {}
void HwVerification_ADC_Init(void) {}
