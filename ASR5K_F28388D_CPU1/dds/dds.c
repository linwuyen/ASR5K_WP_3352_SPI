
/*
 * dds.c - Direct Digital Synthesis with Complete State Machine
 *
 * Created on: January 12, 2026
 * Author: Cody Chen
 *
 * FSM States Flow:
 *   1. STOPPED (Enable required) -> DELAY_ON
 *   2. DELAY_ON (counting, can interrupt to STOPPED if Disable)
 *   3. PHASE_ON (find start angle, 0.0~360.0 degrees) -> RUNNING
 *   4. RUNNING (Amp/Offset/Freq with Ramp control) -> DELAY_OFF if Disable
 *   5. DELAY_OFF (counting, can return to RUNNING if Enable)
 *   6. PHASE_OFF (find stop angle, 0.0~360.0 degrees) -> STOPPED
 */

#include "dds_buildTable.h"
#include "dds_common.h"
#include "dds_core.h"
#include <stdint.h>

/** @brief Global DDS context instance */
ST_DDS sDDS;

/** @brief 4096-point sine table allocated in dedicated RAMGS4 memory section */
#ifdef __TI_COMPILER_VERSION__
#pragma DATA_SECTION(g_au16DdsSineTable, ".dds_sine_table")
#endif
uint16_t g_au16DdsSineTable[DDS_TABLE_SIZE];

DDS_STAT initDDS(HAL_DDS hal) {
  // Build 4096-point table one entry per call (true non-blocking)
  uint16_t bComplete = buildSineTable(g_au16DdsSineTable, &hal->u16TableIndex);

  if (bComplete) {
    // 4096-point table initialization complete
    hal->bTableReady = 1;
    hal->bInitComplete = 1;
    sDDS.fgRecordState = DDS_STATE_INIT_TABLE;
    // Default to STOPPED state instead of RUNNING
    return DDS_STATE_STOPPED;
  }

  // Continue building table (one entry per polling cycle, 4096 cycles total)
  return DDS_STATE_INIT_TABLE;
}

/**
 * @brief Main DDS polling function - Called from timetask.c
 * @note Handles table initialization only
 */
void runDDS(void) {
  switch (sDDS.fgState) {
  case DDS_STATE_IDLE:
  case DDS_STATE_INIT_TABLE:
    sDDS.fgState = initDDS(&sDDS);
    break;
  case DDS_STATE_RUNNING:
  case DDS_STATE_STOPPED:
  case DDS_STATE_DELAY_ON:
  case DDS_STATE_DELAY_OFF:
  case DDS_STATE_PHASE_OFF:
  case DDS_STATE_STARTED:
  case DDS_STATE_AMP_RAMP_DOWN:
    // State machine processing happens in stepDDS() called from ISR
    break;
  default:
    sDDS.fgState = DDS_STATE_ERROR;
    break;
  }
}
