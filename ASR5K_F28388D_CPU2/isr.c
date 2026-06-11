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
}


#ifdef _FLASH
#pragma SET_CODE_SECTION(".TI.ramfunc")
#endif //_FLASH

__interrupt void INT_CPU1_ADCA_1_ISR (void)
{
//    // Start timing
//    startTimerMeasure(&sDrv.tpIsrCost);
//
//
//
//    // Stop timing
//    stopTimerMeasure(&sDrv.tpIsrCost);
//    measTimerLength(&sDrv.tpIsrLength);
//    // Recorded at 202401219 14:00 The program running time is 5.16usec
//
//    // Clear the interrupt flag
//    ADC_clearInterruptStatus(CPU1_ADCA_BASE, ADC_INT_NUMBER1);
//
//    // Acknowledge the interrupt
//    Interrupt_clearACKGroup(INT_CPU1_ADCA_1_INTERRUPT_ACK_GROUP);
}



#ifdef _FLASH
#pragma SET_CODE_SECTION()
#endif //_FLASH
