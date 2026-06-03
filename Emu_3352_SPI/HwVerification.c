/*
 * HwVerification.c
 *
 *  Created on: 2026/05/08
 *      Author: Antigravity
 *  Description: Added SPI_isBusy check before ending state to prevent early CS de-assertion.
 */

#include "HwVerification.h"
#include <math.h>
#include "board.h"
#include "i2c.h"
#include "asysctl.h"
#include "fsi.h"
#include "timetask.h"
#include <SPIB_Slave/spi_slave.h>

// ==============================================================================
// Global Instance
// ==============================================================================
ST_HWTEST g_hwTest = { .u16DacRawSet = 2048, .f32DacVoltSet = 1.5f };

// ==============================================================================
// Private Scheduler
// ==============================================================================
typedef struct {
    uint32_t u32LastTick_A;
    uint32_t u32LastTick_Sdram;
    uint16_t u16Initialized;
} ST_HWVERIF_SCHED;

static ST_HWVERIF_SCHED s_sched = { 0, 0, 0 };
//static uint16_t s_u16State = 0; // 0:Idle, 1:Wait
//static uint16_t s_u16Exp   = 0;

static inline uint32_t calcDelta(uint32_t u32Now, uint32_t u32Last) {
    return (u32Now >= u32Last) ? (u32Now - u32Last) : ((SW_TIMER - u32Last) + u32Now);
}


void HwVerification_Init(void) {
    g_hwTest.stFlash.u32ID = 0;
//    s_u16State = 0;
    s_sched.u16Initialized = 1;
    g_hwTest.stSdram.u16Ctrl = 0;

    // Explicitly initialize DAC-A for physical oscilloscope testing
    EALLOW;
    // DAC-A
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 0U);
    EDIS;
}

void HwVerification_RunAllTests(void) {
    uint32_t u32Now = U32_UPCNTS;
    if (s_sched.u16Initialized == 0) HwVerification_Init();
    if (calcDelta(u32Now, s_sched.u32LastTick_A) >= T_2D5MS) {
        HwVerification_ADC_RunLoopback(); HwVerification_UpdateMonitor();
        s_sched.u32LastTick_A = u32Now;
    }
//    HwVerification_FLASH_RunTest();
    
    if (g_hwTest.stSdram.u16Ctrl == 1) { // Mode 1: Single Trigger
        HwVerification_SDRAM_RunTest();
        if (g_hwTest.stSdram.u16Ctrl == 1) { // Basic test passed
            HwVerification_SDRAM_RunStressTest();
        }
        if (g_hwTest.stSdram.u16Ctrl == 1) { // Both tests passed
            g_hwTest.stSdram.u16Ctrl = 0;    // Return to idle/pass state
        }
    }
    else if (g_hwTest.stSdram.u16Ctrl == 2) { // Mode 2: Continuous Burn-in (100ms)
        if (calcDelta(u32Now, s_sched.u32LastTick_Sdram) >= T_100MS) {
            HwVerification_SDRAM_RunStressTest();
            s_sched.u32LastTick_Sdram = u32Now;
        }
    }
    else if(g_hwTest.u16SPIBLOOP == 1){
        HwVerification_SPIB_RunLoopbackTest();
        g_hwTest.u16SPIBLOOP = 0;
    }
}

//void HwVerification_FLASH_RunTest(void) {
//    uint32_t u32Base = SPIA_EXTFLASH_BASE;
//
//    if (s_u16State == 0) { // IDLE
//        uint16_t u16Trig = g_hwTest.stFlash.u16Trigger;
//        if (u16Trig == 0) return;
//        g_hwTest.stFlash.u16Trigger = 0;
//        SPI_resetRxFIFO(u32Base);
//
//        if (u16Trig == 1) { // Read ID
//            SPI_writeDataNonBlocking(u32Base, 0x9F00);
//            for(s_u16Exp=0; s_u16Exp<3; s_u16Exp++) SPI_writeDataNonBlocking(u32Base, 0x0000);
//            s_u16Exp = 4;
//        } else if (u16Trig == 2) { // WREN
//            SPI_writeDataNonBlocking(u32Base, 0x0600); s_u16Exp = 1;
//        } else if (u16Trig == 3) { // Erase Sector 0
//            SPI_writeDataNonBlocking(u32Base, 0x2000);
//            for(s_u16Exp=0; s_u16Exp<3; s_u16Exp++) SPI_writeDataNonBlocking(u32Base, 0x0000);
//            s_u16Exp = 4;
//        } else if (u16Trig == 4) { // Read Data @ 0
//            SPI_writeDataNonBlocking(u32Base, 0x0300);
//            for(s_u16Exp=0; s_u16Exp<5; s_u16Exp++) SPI_writeDataNonBlocking(u32Base, 0x0000);
//            s_u16Exp = 6;
//        } else if (u16Trig == 5) { // Write Data @ 0
//            SPI_writeDataNonBlocking(u32Base, 0x0200); // Cmd
//            for(s_u16Exp=0; s_u16Exp<3; s_u16Exp++) SPI_writeDataNonBlocking(u32Base, 0x0000); // Addr 0
//            SPI_writeDataNonBlocking(u32Base, 0x5500); // Data 1
//            SPI_writeDataNonBlocking(u32Base, 0xAA00); // Data 2
//            s_u16Exp = 6;
//        }
//        s_u16State = 1;
//    } else { // WAIT
//        if (SPI_getRxFIFOStatus(u32Base) < s_u16Exp || SPI_isBusy(u32Base)) return;
//
//        uint16_t u16Buf[8];
//        for (uint16_t i=0; i<s_u16Exp; i++) u16Buf[i] = SPI_readDataNonBlocking(u32Base) & 0xFF;
//
//        if (s_u16Exp == 4) { // ID Result
//            g_hwTest.stFlash.u32ID = ((uint32_t)u16Buf[1] << 16) | ((uint32_t)u16Buf[2] << 8) | u16Buf[3];
//        } else if (s_u16Exp == 6) { // Read Result
//            g_hwTest.stFlash.u32Data = ((uint32_t)u16Buf[4] << 8) | u16Buf[5];
//        }
//        s_u16State = 0;
//    }
//}

// Stubs
void HwVerification_ADC_RunLoopback(void) {
    if (spiB_slave.u16BlockStatus == 3U) {
        static uint16_t s_u16PlayIdx = 0U;
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
        g_hwTest.f32AdcVolt = (float32_t)g_hwTest.u16AdcRaw * 0.0007326f;
    }
}
void HwVerification_UpdateMonitor(void){
    uint16_t r=ADC_readResult(ADCARESULT_BASE,ADC_SOC_NUMBER1);
    ADC_forceSOC(ADCA_BASE,ADC_SOC_NUMBER1); g_hwTest.f32McuTemp=ADC_getTemperatureC(r,3.0f);
}
void HwVerification_FSI_RunTest(void) {}
#define SDRAM_START_ADDR  0x80000000UL

void HwVerification_SDRAM_RunTest(void) {
    volatile uint32_t *pu32Mem = (uint32_t *)SDRAM_START_ADDR;
    uint32_t u32Index;
    uint32_t u32TestPattern;
    
    for(u32Index = 0; u32Index < 32; u32Index++) {
        u32TestPattern = (1UL << u32Index);
        *pu32Mem = u32TestPattern;
        if(*pu32Mem != u32TestPattern) {
            g_hwTest.stSdram.u16Ctrl = 0x8000 | (uint16_t)(u32Index & 0x001F);
            return;
        }
    }
}

#define STRESS_BLOCK_SIZE 1000UL

void HwVerification_SDRAM_RunStressTest(void) {
    volatile uint32_t *pu32Mem = (uint32_t *)SDRAM_START_ADDR;
    uint32_t u32Index;
    uint16_t u16ErrorDetected = 0;
    
    for(u32Index = 0; u32Index < STRESS_BLOCK_SIZE; u32Index++) {
        pu32Mem[u32Index] = ~((uint32_t)&pu32Mem[u32Index]);
    }
    
    for(u32Index = 0; u32Index < STRESS_BLOCK_SIZE; u32Index++) {
        uint32_t u32ExpectedData = ~((uint32_t)&pu32Mem[u32Index]);
        uint32_t u32ReadData = pu32Mem[u32Index];
        if(u32ReadData != u32ExpectedData) {
            g_hwTest.stSdram.u32FailAddr = (uint32_t)&pu32Mem[u32Index];
            g_hwTest.stSdram.u32FailRead = u32ReadData;
            g_hwTest.stSdram.u16Ctrl = 0x8100;
            u16ErrorDetected = 1;
            break;
        }
    }
    
    if(u16ErrorDetected == 1) {
        g_hwTest.stSdram.u32StressErr++;
    } else {
        g_hwTest.stSdram.u32StressPass++;
    }
}


//
// Function: HwVerification_SPIB_RunLoopbackTest
// Description: Internal loopback validation for SPIB slave packet processing.
//
extern volatile uint32_t g_u32DebugLastTx; // Target monitoring variable

void HwVerification_SPIB_RunLoopbackTest(void)
{
    uint32_t u32Base = SPIB_SYSTEM_BASE;
    uint16_t u16SimulatedGroupCmd = 0x0401U; // Group=0x04 (Version), ID=0x01
    uint16_t u16SimulatedData     = 0x0000U;


    if(spiB_slave.fsm == _INIT_SPI_AS_SLAVE)
    {
        runSPIBslave();
    }


    SPI_enableLoopback(u32Base);


    SPI_resetTxFIFO(u32Base);
    SPI_resetRxFIFO(u32Base);
    g_u32DebugLastTx = 0U;


    SPI_writeDataNonBlocking(u32Base, u16SimulatedGroupCmd);
    SPI_writeDataNonBlocking(u32Base, u16SimulatedData);


    uint16_t u16Iter;
    for(u16Iter = 0U; u16Iter < 2U; u16Iter++)
    {
        runSPIBslave();
    }


    SPI_disableLoopback(u32Base);
}




void HwVerification_UpdateIO(void){}
void HwVerification_I2C_RunTest(void){}
void HwVerification_BasicIO_Init(void){}
void HwVerification_BasicIO_RunHeartbeat(void){}
void HwVerification_Advanced_Init(void){}
void HwVerification_ADC_Init(void){}
