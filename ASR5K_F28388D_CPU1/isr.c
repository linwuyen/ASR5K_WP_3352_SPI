/*
 * isr.c
 *
 *  Created on: Mar 18, 2024
 *      Author: cody_chen
 */

#include "common.h"
#include "shareram.h"
#include "c28protection.h"
#include "cTimeMeas.h"

#include "isr_common.h"
#include "dds/dds_api.h"

ST_DRV sDrv = {
    .fgStatus = _CSTAT_INIT_DRV_PARAM,

};

void initDerivedHwConfig(void)
{
}

void initHwConfigAndDrvParams(void)
{
    initHwConfig(&sHwConfig);
    initDerivedHwConfig();

    /* DDS parameter targets for JTAG control */
    sDrv.u32TargetFreq_x100 = 5000UL; /* 50.00 Hz default */
    sDrv.u16TargetAmp = 16383U;       /* Half amplitude default */
    sDrv.u16TargetOff = 32768U;       /* True center offset (2^15) */

    /* DDS delay and phase control (JTAG accessible) */
    sDrv.f32DelayOnTime = 0.001f;   /* 0.001 seconds delay-on */
    sDrv.f32DelayOffTime = 0.0005f; /* 0.0005 seconds delay-off */
    sDrv.f32PhaseOnAngle = 0.0f;    /* Start at 0 degrees */
    sDrv.f32PhaseOffAngle = 180.0f; /* Stop at 180 degrees */
    sDrv.bDdsStartCmd = 0U;         /* Start command flag */
    sDrv.bDdsStopCmd = 0U;          /* Stop command flag */

    /* DDS Ramp control parameters (JTAG accessible) - Time in seconds (0.000~60.000) */
    sDrv.f32AmpRampTimeUp = 1.0f;   /* Amplitude ramp up time: 1.0 second */
    sDrv.f32AmpRampTimeDown = 0.5f; /* Amplitude ramp down time: 0.5 second */
    sDrv.bAmpRampEnabled = 0U;      /* Amplitude ramp disabled by default */

    sDrv.f32OffsetRampTimeUp = 2.0f;   /* Offset ramp up time: 2.0 seconds */
    sDrv.f32OffsetRampTimeDown = 1.0f; /* Offset ramp down time: 1.0 second */
    sDrv.bOffsetRampEnabled = 0U;      /* Offset ramp disabled by default */

    sDrv.f32FreqRampTimeUp = 3.0f;   /* Frequency ramp up time: 3.0 seconds */
    sDrv.f32FreqRampTimeDown = 2.0f; /* Frequency ramp down time: 2.0 seconds */
    sDrv.bFreqRampEnabled = 0U;      /* Frequency ramp disabled by default */

    /* DDS status variables (JTAG readable) */
    sDrv.bDdsReady = 0U;
    sDrv.bDdsRunning = 0U;
    sDrv.u16DdsSample = 0U;
    sDrv.u32DdsUpdateCounter = 0UL;

    /* DDS Ramp status variables (JTAG readable) */
    sDrv.bAmpRampComplete = 1U;    /* Amplitude ramp complete status */
    sDrv.bOffsetRampComplete = 1U; /* Offset ramp complete status */
    sDrv.bFreqRampComplete = 1U;   /* Frequency ramp complete status */
    sDrv.bAnyRampActive = 0U;      /* Any ramp currently active */

    sDrv.u16CurrentAmp = sDrv.u16TargetAmp;             /* Current amplitude value */
    sDrv.u16CurrentOffset = sDrv.u16TargetOff;          /* Current offset value */
    sDrv.u32CurrentFreq_x100 = sDrv.u32TargetFreq_x100; /* Current frequency */

    /* Initialize DDS Ramp debug variables */
    sDrv.u16FreqRampStatus = 0U;                       /* Frequency ramp status */
    sDrv.u32FreqRampCurrent = sDrv.u32TargetFreq_x100; /* Frequency ramp current */
    sDrv.u32FreqRampTarget = sDrv.u32TargetFreq_x100;  /* Frequency ramp target */
    sDrv.u16ActiveRampMask = 0U;                       /* Active ramp mask */

    /* Initialize DDS with current parameters */
    DDS_Init(100000UL, sDrv.u32TargetFreq_x100, sDrv.u16TargetAmp, sDrv.u16TargetOff);
}

#ifdef _FLASH
#pragma SET_CODE_SECTION(".TI.ramfunc")
#endif //_FLASH

__interrupt void INT_CPU1_ADCA_1_ISR(void)
{
    /* Start timing */
    startTimerMeasure(&sDrv.tpIsrCost);

    /* Execute DDS state machine (Delay, Phase, Ramp control) */
    DDS_Step();

    /* Get DDS sample and output to DAC */
    sDrv.u16DdsSample = DDS_GetSample();
    uint16_t u16DacValue = sDrv.u16DdsSample >> 4; /* Convert 16-bit to 12-bit for DAC */
    DAC_setShadowValue(RSVD_DACC_BASE, u16DacValue);

    /* Update DDS status for JTAG monitoring */
    sDrv.bDdsReady = DDS_IsInitComplete();
    sDrv.bDdsRunning = DDS_IsRunning();

    /* Stop timing */
    stopTimerMeasure(&sDrv.tpIsrCost);
    measTimerLength(&sDrv.tpIsrLength);

    /* Clear the interrupt flag */
    ADC_clearInterruptStatus(ADCA_AIN1_BASE, ADC_INT_NUMBER1);

    /* Acknowledge the interrupt */
    Interrupt_clearACKGroup(INT_ADCA_AIN1_1_INTERRUPT_ACK_GROUP);
}

#ifdef _FLASH
#pragma SET_CODE_SECTION()
#endif //_FLASH
