// #############################################################################
//
//  FILE:   empty_c28x_dual_sysconfig_cpu1.c
//
//  TITLE: SysConfig Empty Project
//
//  CPU1 Empty Project Example
//
//  This example is an empty project setup for Driverlib development for CPU1.
//
// #############################################################################

//
// Included Files
//
#include "common.h"
#include "shareram.h"
#include "mb_slave/ModbusSlave.h"
#include "cTimeMeas.h"
#include "dds/dds_api.h"

#pragma DATA_SECTION(sAccessCPU1, "RW_CPU1");
ST_SHARERAM sAccessCPU1;
#pragma DATA_SECTION(sReadCPU2, "RD_CPU2");
ST_SHARERAM sReadCPU2;

#define ALLOW_CPU2_ACCESS_GSRAM (MEMCFG_SECT_GS5 | MEMCFG_SECT_GS6 | MEMCFG_SECT_GS7 | MEMCFG_SECT_GS8 | MEMCFG_SECT_GS9 | MEMCFG_SECT_GS10 | MEMCFG_SECT_GS11 | \
                                 MEMCFG_SECT_GS15)

uint32_t MEP_ScaleFactor;

volatile uint32_t ePWM[] = {0, EPWM1_BASE, EPWM2_BASE, EPWM3_BASE, EPWM4_BASE};

//
// Main
//
void main(void)
{
    //
    // Initialize device clock and peripherals
    //
    Device_init();

#ifdef _FLASH
    //
    // Send boot command to allow the CPU2 application to begin execution
    //
    Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR0);
#endif // _STANDALONE

    //
    // Give memory access to GS13 RAM to CPU2
    //
    MemCfg_setGSRAMMasterSel(ALLOW_CPU2_ACCESS_GSRAM,
                             MEMCFG_GSRAMMASTER_CPU2);

    // The fapi_ram_LoadStart, fapi_ram_LoadSize, and fapi_ram_RunStart symbols
    // are created by the linker. Refer to the device .cmd file.
    //
    memcpy(&fapi_ram_RunStart, &fapi_ram_LoadStart, (size_t)&fapi_ram_LoadSize);

    // FLASH Initialization:
    // The "FLASH_init()" should be called after or during initialization functions like
    // Device_init() or Device_enableAllPeripherals().
    FLASH_init();

    //
    // Disable pin locks and enable internal pullups.
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell Interrupt
    // Service Routines (ISR).
    //
    Interrupt_initVectorTable();

    //
    // Board Initialization
    //
    Board_init();
    initCLA();
    initPWM();

    // Initialize DDS system with enhanced control
    DDS_Init(100000UL, sDrv.u32TargetFreq_x100, sDrv.u16TargetAmp, sDrv.u16TargetOff);

    //
    // Sync CPUs so the blinking starts at the same time, though the LEDs toggle at different frequency
    //
    IPC_sync(IPC_CPU1_L_CPU2_R, IPC_SYNC);
    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;

    //
    // Loop Forever
    //
    for (;;)
    {

        // Start timing
        startTimerMeasure(&sDrv.tpMainCost);

        pollTimeTask();

        // Stop timing
        stopTimerMeasure(&sDrv.tpMainCost);
        // Recorded at 202401219 14:00 The program running time is 5.16usec
    }
}

//
// End of File
//
