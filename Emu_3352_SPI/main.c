//
// Included Files
//
//
// Included Files
//
#include "board.h"
#include "c2000ware_libraries.h"
#include "device.h"
#include "driverlib.h"
#include "common.h"
#include "shareram.h"

// SPI modules: master task, slave task, self-test automation.
// spi_fifo / spi_pingpong are internal details of spi_b_slave and are
// not included here.
#include "SPIA_Master/SPI_master.h"
#include "SPIB_Slave/spi_slave.h"
#include "asr5k_spi_selftest.h"

#ifdef _FLASH
#pragma DATA_SECTION(sAccessCPU1, "MSGRAM_CPU1_TO_CPU2")
ST_SHARERAM sAccessCPU1;

#pragma DATA_SECTION(sReadCPU2, "MSGRAM_CPU2_TO_CPU1")
ST_SHARERAM sReadCPU2;

ST_DRV sDrv = {
    .fgStatus = _CSTAT_INIT_DRV_PARAM,
};
#endif
// Main

//
void main(void) {

  //
  // Initialize device clock and peripherals
  //
  Device_init();

  //
  // Disable pin locks and enable internal pull-ups.
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
  // PinMux and Peripheral Initialization
  //
  Board_init();

  //
  // C2000Ware Library initialization
  //
  C2000Ware_libraries_init();

  // Initialize the Slave before the Master can issue its first frame.
//  runSPIBslave();

  //
  // Enable Global Interrupt (INTM) and real time interrupt (DBGM)
  //
  EINT;
  ERTM;

  //
  // Loop Forever
  //
  for (;;) {

    // Run the production Slave logic before the Master issues the next frame.
    runSPIBslave();
    // Initialize on first call, then execute the SPIA Master task.
    runSPIAmaster();
    // Run the non-blocking SPI test document automation.
    Asr5kSpiSelfTest_Run();
#ifdef _FLASH
    // Service the SCIA Modbus/debug port.
    runDebug();
#endif
  }
}
