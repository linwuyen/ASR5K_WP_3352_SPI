#include "common.h"
#include "shareram.h"
#include "mb_slave/ModbusSlave.h"

#pragma DATA_SECTION(sReadCPU1,"RD_CPU1");
ST_SHARERAM sReadCPU1;
#pragma DATA_SECTION(sAccessCPU2,"RW_CPU2");
ST_SHARERAM sAccessCPU2;

//
// Main
//
void main(void)
{
    //
    // Initialize device clock and peripherals
    //
    Device_init();

    // The fapi_ram_LoadStart, fapi_ram_LoadSize, and fapi_ram_RunStart symbols
    // are created by the linker. Refer to the device .cmd file.
    //
    memcpy(&fapi_ram_RunStart, &fapi_ram_LoadStart, (size_t)&fapi_ram_LoadSize);


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
    // Initialize SysConfig Configuration
    //
    Board_init();

    //
    // Sync CPUs so the blinking starts at the same time, though the LEDs toggle at different frequency
    //
    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_SYNC);

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;


    //
    // Loop Forever
    //
    for(;;)
    {
        pollTimeTask();
    }
}

//
// End of File
//
