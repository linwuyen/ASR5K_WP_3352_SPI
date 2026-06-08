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
#include "timetask.h"

// Include refactored SPI Master module
#include "SPIA_Master/SPI_master.h"
#include "SPIB_Slave/spi_slave.h"
#include "SPIB_Slave/spi_fifo.h"
#include "asr5k_spi_selftest.h"


uint16_t cnt;

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

#if ASR5K_ENABLE_FIFO_SELFTEST
  (void)FIFO_Test_Run();
#endif

  //
  // Loop Forever
  //
  for (;;) {
      cnt++;
    // Run the production Slave logic before the Master issues the next frame.
    runSPIBslave();
    // Initialize on first call, then execute the SPIA Master task.
    runSPIAmaster();
    // Run the non-blocking SPI test document automation.
    Asr5kSpiSelfTest_Run();
  }
}
