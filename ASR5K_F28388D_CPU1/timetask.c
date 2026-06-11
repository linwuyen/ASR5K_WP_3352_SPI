/*
 * timetask.c
 *
 *  Created on: May 17, 2024
 *      Author: User
 */

#include "./cla/initCLA.h"
#include "common.h"
#include "dds/dds_api.h"
#include "shareram.h"


void recalParameters(void) {
  initHwConfigAndDrvParams();
  //    initParamsForCLA();
}

void updateFgStatus(HAL_DRV hal) {}

void pollSlowError(void);
void pollUpdateParamCLA(void);
void (*pollParams)(void) = pollSlowError;

void pollSlowError(void) { pollParams = pollUpdateParamCLA; }

void pollUpdateParamCLA(void) {

  updateFgStatus(&sDrv);

  pollParams = pollSlowError;
}

void task25msec(void *s) {
  pollParams();

  if (isCallbackReady()) {
    if (GETn_STAT(_CSTAT_INIT_PARAMS, sDrv)) {
      recalParameters();

      sAccessCPU1.f32Cpu1VinScale = sHwConfig.f32VoutScale;
      sAccessCPU1.f32Cpu1IoutScale = sHwConfig.f32IoutScale;

      SET_STAT(_CSTAT_INIT_PARAMS, sDrv);
    }
  }

  if ((0 != sDrv.u32HeartBeat) && (0 != sCLA.u32HeartBeat)) {
    SET_STAT(_CSTAT_THREAD_READY, sDrv);
  }
}

void task2D5msec(void *s) {

  if (GET_STAT(_CSTAT_INIT_SUCCESS, sDrv)) {
    scanWarning();
  }

  measTimerLength(&sDrv.tpTaskLength);
}

/**
 * @brief Update DDS parameters and synchronize status
 * @note Called in asapTask() for parameter synchronization only
 * @note DDS_Step() executes in 100KHz ISR for precise timing control
 * @note DDS_Poll() completes sine table initialization (4096 cycles)
 */
void updateDDS(void) {
  /* Poll DDS for table initialization (4096 cycles to complete) */
  DDS_Poll();

  /* Update counter for JTAG monitoring */
  sDrv.u32DdsUpdateCounter++;

  /* === RAMP CONTROL PARAMETER UPDATES (Differential Update) === */
  static float32_t s_f32LastAmpTimeUp = 0.0f, s_f32LastAmpTimeDown = 0.0f;
  static uint16_t s_bLastAmpRampEnabled = 0U;
  static float32_t s_f32LastOffsetTimeUp = 0.0f, s_f32LastOffsetTimeDown = 0.0f;
  static uint16_t s_bLastOffsetRampEnabled = 0U;
  static float32_t s_f32LastFreqTimeUp = 0.0f, s_f32LastFreqTimeDown = 0.0f;
  static uint16_t s_bLastFreqRampEnabled = 0U;

  /* Batch check for Ramp parameter changes using bitmask */
  uint16_t u16RampChangeMask = 0U;

  /* Check Amplitude Ramp changes */
  if ((sDrv.f32AmpRampTimeUp != s_f32LastAmpTimeUp) ||
      (sDrv.f32AmpRampTimeDown != s_f32LastAmpTimeDown) ||
      (sDrv.bAmpRampEnabled != s_bLastAmpRampEnabled)) {
    u16RampChangeMask |= 0x01U;
  }

  /* Check Offset Ramp changes */
  if ((sDrv.f32OffsetRampTimeUp != s_f32LastOffsetTimeUp) ||
      (sDrv.f32OffsetRampTimeDown != s_f32LastOffsetTimeDown) ||
      (sDrv.bOffsetRampEnabled != s_bLastOffsetRampEnabled)) {
    u16RampChangeMask |= 0x02U;
  }

  /* Check Frequency Ramp changes */
  if ((sDrv.f32FreqRampTimeUp != s_f32LastFreqTimeUp) ||
      (sDrv.f32FreqRampTimeDown != s_f32LastFreqTimeDown) ||
      (sDrv.bFreqRampEnabled != s_bLastFreqRampEnabled)) {
    u16RampChangeMask |= 0x04U;
  }

  /* Apply Ramp parameter updates only when changed */
  if (u16RampChangeMask & 0x01U) {
    DDS_SetAmplitudeRamp(sDrv.f32AmpRampTimeUp, sDrv.f32AmpRampTimeDown,
                         sDrv.bAmpRampEnabled);
    s_f32LastAmpTimeUp = sDrv.f32AmpRampTimeUp;
    s_f32LastAmpTimeDown = sDrv.f32AmpRampTimeDown;
    s_bLastAmpRampEnabled = sDrv.bAmpRampEnabled;
  }

  if (u16RampChangeMask & 0x02U) {
    DDS_SetOffsetRamp(sDrv.f32OffsetRampTimeUp, sDrv.f32OffsetRampTimeDown,
                      sDrv.bOffsetRampEnabled);
    s_f32LastOffsetTimeUp = sDrv.f32OffsetRampTimeUp;
    s_f32LastOffsetTimeDown = sDrv.f32OffsetRampTimeDown;
    s_bLastOffsetRampEnabled = sDrv.bOffsetRampEnabled;
  }

  if (u16RampChangeMask & 0x04U) {
    DDS_SetFrequencyRamp(sDrv.f32FreqRampTimeUp, sDrv.f32FreqRampTimeDown,
                         sDrv.bFreqRampEnabled);
    s_f32LastFreqTimeUp = sDrv.f32FreqRampTimeUp;
    s_f32LastFreqTimeDown = sDrv.f32FreqRampTimeDown;
    s_bLastFreqRampEnabled = sDrv.bFreqRampEnabled;
  }

  /* === DDS TARGET PARAMETER UPDATES (Differential Update) === */
  /* Note: Actual Ramp processing happens in DDS_Update() during ISR */
  if (sDrv.u32TargetFreq_x100 != DDS_GetFrequency()) {
    DDS_SetFrequency(sDrv.u32TargetFreq_x100);
  }

  if (sDrv.u16TargetAmp != DDS_GetAmplitude()) {
    DDS_SetAmplitude(sDrv.u16TargetAmp);
  }

  if (sDrv.u16TargetOff != DDS_GetOffset()) {
    DDS_SetOffset(sDrv.u16TargetOff);
  }

  /* === STATUS SYNCHRONIZATION FOR JTAG MONITORING === */
  sDrv.bAmpRampComplete = DDS_IsRampComplete(0U);    /* Amplitude */
  sDrv.bOffsetRampComplete = DDS_IsRampComplete(1U); /* Offset */
  sDrv.bFreqRampComplete = DDS_IsRampComplete(2U);   /* Frequency */
  sDrv.bAnyRampActive = DDS_IsAnyRampActive();

  /* Update current values for JTAG monitoring */
  sDrv.u16CurrentAmp = DDS_GetAmplitude();
  sDrv.u16CurrentOffset = DDS_GetOffset();
  sDrv.u32CurrentFreq_x100 = DDS_GetFrequency();

  /* === COMMAND PROCESSING (Start/Stop with Delay/Phase Control) === */
  /* Note: Actual Delay/Phase timing control happens in DDS_Update() FSM */
  if (sDrv.bDdsStartCmd) {
    sDrv.bDdsStartCmd = 0U; /* Auto-clear command flag */

    /* Configure delay and phase settings before start */
    DDS_SetDelayOn(sDrv.f32DelayOnTime);
    DDS_SetPhaseOn(sDrv.f32PhaseOnAngle);

    DDS_Start(); /* FSM will handle DELAY_ON -> PHASE_ON -> RUNNING transitions
                  */
  }

  if (sDrv.bDdsStopCmd) {
    sDrv.bDdsStopCmd = 0U; /* Auto-clear command flag */

    /* Configure delay and phase settings before stop */
    DDS_SetDelayOff(sDrv.f32DelayOffTime);
    DDS_SetPhaseOff(sDrv.f32PhaseOffAngle);

    DDS_Stop(); /* FSM will handle DELAY_OFF -> PHASE_OFF -> STOPPED transitions
                 */
  }
}

void asapTask(void *s) {
  runDebug();

  runManualFlashApi();
  runFlashStorage();

  updateDDS();
}

ST_TIMETASK time_task[] = {{task2D5msec, 0, T_2D5MS},
                           {task25msec, 0, T_25MS},
                           {asapTask, 0, 0},
                           {0, 0, 0}};

void pollTimeTask(void) { scanTimeTask(time_task, (void *)0); }
