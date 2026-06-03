/*
 * HwVerification.h
 */

#ifndef HWVERIFICATION_H_
#define HWVERIFICATION_H_

#include <stdint.h>
#include "device.h"
#include "board.h"    // MUST include board.h BEFORE timetask.h for SWTIRMER_BASE
#include "timetask.h" 

// ==============================================================================
// Hardware Test Structure
// ==============================================================================
typedef struct {
    uint16_t u16Ctrl;           // 1=Trigger, 0=Idle/Pass, >1=Error
    uint32_t u32StressPass;
    uint32_t u32StressErr;
    uint32_t u32FailAddr;
    uint32_t u32FailRead;
} ST_SDRAM_DIAG;

typedef struct {
    uint32_t u32ID;
    uint32_t u32Status;
    uint32_t u32Data;           // Merged variable for read results
    uint16_t u16Trigger;
    uint16_t u16RxCount;
} ST_FLASH_DIAG;

typedef struct {
    // ADC/DAC
    uint16_t u16AdcRaw;
    float32_t f32AdcVolt;
    uint16_t u16DacRawSet;
    float32_t f32DacVoltSet;
    float32_t f32McuTemp;
    
    // SPI Flash
    ST_FLASH_DIAG stFlash;
    
    // Others
    uint16_t u16FsiStatus;
    ST_SDRAM_DIAG stSdram;


    uint16_t u16SPIBLOOP;
} ST_HWTEST;

extern ST_HWTEST g_hwTest;

// ==============================================================================
// Public API
// ==============================================================================
void HwVerification_Init(void);
void HwVerification_RunAllTests(void);

// Task-specific
void HwVerification_FLASH_RunTest(void);
void HwVerification_ADC_RunLoopback(void);
void HwVerification_UpdateMonitor(void);
void HwVerification_SDRAM_RunTest(void);
void HwVerification_SDRAM_RunStressTest(void);
void HwVerification_SPIB_RunLoopbackTest(void);
#endif /* HWVERIFICATION_H_ */
