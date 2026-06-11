/*
 * dds_api.h - DDS Public API Interface
 *
 * Created on: January 12, 2026
 * Author: Cody Chen
 *
 * This file provides a clean API for controlling DDS from ISR and timetask
 */

#ifndef DDS_DDS_API_H_
#define DDS_DDS_API_H_

#include "dds_common.h"
#include "dds_core.h"
#include <stdint.h>

/**
 * @brief Initialize DDS system (call from main initialization)
 * @param u32SampleRate Sample rate in Hz (typically 100000 for 100KHz)
 * @param u32InitFreq_x100 Initial frequency * 100 (e.g., 5000 = 50.00 Hz)
 * @param u16InitAmp Initial amplitude (0-65535, PU format)
 * @param u16InitOffset Initial offset (0-65535)
 */
static inline void DDS_Init(uint32_t u32SampleRate, uint32_t u32InitFreq_x100,
                            uint16_t u16InitAmp, uint16_t u16InitOffset) {
  sDDS.fgState = DDS_STATE_INIT_TABLE;
  sDDS.fgRecordState = DDS_STATE_IDLE;
  sDDS.u32SampleRate = u32SampleRate;
  sDDS.u32Frequency_x100 = u32InitFreq_x100;
  sDDS.u16UserAmplitude = u16InitAmp;
  // If Init Freq is 0, start in DC mode (Amp=0)
  if (u32InitFreq_x100 == 0) {
    sDDS.u16Amplitude = 0;
  } else {
    sDDS.u16Amplitude = u16InitAmp;
  }
  sDDS.u16Offset = u16InitOffset;

  // Pre-calculate optimization factor for phase increment
  /*
   * Factor = 2^32 / (SampleRate * 100)
   * This factor is used to replace division with multiplication in runtime
   */
  sDDS.f32PhaseStepFactor = 4294967296.0f / ((float32_t)u32SampleRate * 100.0f);

  // Initialize phase accumulator
  sDDS.u32PhaseAccumulator = 0UL;

  /* Optimization: Use pre-calculated multiplication logic */
  sDDS.u32PhaseIncrement =
      (uint32_t)((float32_t)u32InitFreq_x100 * sDDS.f32PhaseStepFactor);

  // Initialize table building
  sDDS.u16TableIndex = 0U;
  sDDS.bTableReady = 0U;
  sDDS.bInitComplete = 0U;

  // Initialize control flags
  sDDS.bStartRequested = 0U;
  sDDS.bStopRequested = 0U;

  // Initialize delay targets (default values, can be changed later)
  sDDS.u32DelayOnTarget = DDS_DELAY_ON_COUNT;
  sDDS.u32DelayOffTarget = DDS_DELAY_OFF_COUNT;
  sDDS.u32DelayCounter = 0UL;

  // Initialize phase control (default to immediate)
  sDDS.s16PhaseOnDegrees = DDS_PHASE_IMMEDIATE;
  sDDS.s16PhaseOffDegrees = DDS_PHASE_IMMEDIATE;

  // Initialize Ramp target tracking
  sDDS.u16AmpRampTarget = u16InitAmp;
  sDDS.u16OffsetRampTarget = u16InitOffset;
  sDDS.u32FreqRampTarget = u32InitFreq_x100;

  // Initialize Ramp structures
  sDDS.sAmpRamp.u32State = 0U;
  sDDS.sAmpRamp.f32Target = (float32_t)u16InitAmp / 65535.0f;
  sDDS.sAmpRamp.f32StepUp = 0.0001f; // Default: 10000 steps (0.1s at 100KHz)
  sDDS.sAmpRamp.f32StepDown = 0.0001f;
  sDDS.sAmpRamp.f32LowLimit = 0.0f;
  sDDS.sAmpRamp.f32HighLimit = 1.0f;
  sDDS.sAmpRamp.f32Out = (float32_t)u16InitAmp / 65535.0f;

  sDDS.sOffsetRamp.u32State = 0U;
  sDDS.sOffsetRamp.f32Target = (float32_t)u16InitOffset / 65535.0f;
  sDDS.sOffsetRamp.f32StepUp = 0.0001f;
  sDDS.sOffsetRamp.f32StepDown = 0.0001f;
  sDDS.sOffsetRamp.f32LowLimit = 0.0f;
  sDDS.sOffsetRamp.f32HighLimit = 1.0f;
  sDDS.sOffsetRamp.f32Out = (float32_t)u16InitOffset / 65535.0f;

  sDDS.sFreqRamp.u32State = 0U;
  sDDS.sFreqRamp.f32Target =
      (float32_t)u32InitFreq_x100 / 100.0f; // Convert to Hz
  sDDS.sFreqRamp.f32StepUp = 0.0001f;
  sDDS.sFreqRamp.f32StepDown = 0.0001f;
  sDDS.sFreqRamp.f32LowLimit = 0.01f;    // 1.00 Hz minimum
  sDDS.sFreqRamp.f32HighLimit = 1000.0f; // 1000.00 Hz maximum
  sDDS.sFreqRamp.f32Out = (float32_t)u32InitFreq_x100 / 100.0f; // Convert to Hz
}

/**
 * @brief Get DDS output sample (call from 100KHz ISR)
 * @return 16-bit output sample (0-65535)
 */
static inline uint16_t DDS_GetSample(void) { return getDDS(&sDDS); }

/**
 * @brief Execute DDS state machine step (call from 100KHz ISR, before getDDS)
 * @note This performs Delay, Phase, and Ramp control
 */
static inline void DDS_Step(void) { stepDDS(&sDDS); }

/**
 * @brief Poll DDS for table initialization (call from timetask.c)
 */
static inline void DDS_Poll(void) { runDDS(); }

/**
 * @brief Start DDS waveform generation
 * @note Sets the start request flag, actual start happens in state machine
 */
static inline void DDS_Start(void) { sDDS.bStartRequested = 1U; }

/**
 * @brief Stop DDS waveform generation
 * @note Sets the stop request flag, actual stop happens in state machine
 */
static inline void DDS_Stop(void) { sDDS.bStopRequested = 1U; }

/**
 * @brief Set target frequency (updated by Ramp if enabled)
 * @param u32Freq_x100 Frequency * 100 (100 to 100000, 1.00 to 1000.00 Hz)
 */
static inline void DDS_SetFrequency(uint32_t u32Freq_x100) {
  // 1. Handle DC Mode (0 Hz)
  if (u32Freq_x100 == 0) {
    sDDS.u32Frequency_x100 = 0;
    sDDS.u32PhaseIncrement = 0;
    sDDS.u16Amplitude = 0;
    return;
  }

  // 2. Clamp AC Mode frequency
  if (u32Freq_x100 < DDS_MIN_FREQ_X100)
    u32Freq_x100 = DDS_MIN_FREQ_X100;
  if (u32Freq_x100 > DDS_MAX_FREQ_X100)
    u32Freq_x100 = DDS_MAX_FREQ_X100;

  // 3. Check if transitioning from DC Mode (Freq=0) to AC Mode
  if (sDDS.u32Frequency_x100 == 0) {
    sDDS.u16Amplitude = sDDS.u16UserAmplitude;
  }

  sDDS.u32Frequency_x100 = u32Freq_x100;

  // Update phase increment immediately if Ramp is disabled
  if (!(sDDS.sFreqRamp.u32State & _ENABLE_DDS_RAMP)) {
    /*
     * Optimization: Use pre-calculated float multiplication instead of 64-bit
     * division This ensures deterministic execution time in ISR or fast loops
     */
    sDDS.u32PhaseIncrement =
        (uint32_t)((float32_t)u32Freq_x100 * sDDS.f32PhaseStepFactor);
  }
}

/**
 * @brief Set target amplitude (updated by Ramp if enabled)
 * @param u16Amp Amplitude in PU (0-65535, where 65535 = 1.0 PU)
 */
static inline void DDS_SetAmplitude(uint16_t u16Amp) {
  sDDS.u16UserAmplitude = u16Amp;
  // Update effective amplitude only if in AC mode (Freq > 0)
  if (sDDS.u32Frequency_x100 > 0) {
    sDDS.u16Amplitude = u16Amp;
  }
}

/**
 * @brief Set target offset
 * @param u16Offset DC offset (0-65535)
 */
static inline void DDS_SetOffset(uint16_t u16Offset) {
  sDDS.u16Offset = u16Offset;
}

/**
 * @brief Set Delay ON time
 * @param f32Seconds Delay time in seconds (0.000 to 60.000)
 * @note At 100KHz sample rate: 1 second = 100000 counts
 */
static inline void DDS_SetDelayOn(float32_t f32Seconds) {
  if (f32Seconds < 0.0f)
    f32Seconds = 0.0f;
  if (f32Seconds > 60.0f)
    f32Seconds = 60.0f;
  sDDS.u32DelayOnTarget = (uint32_t)(f32Seconds * sDDS.u32SampleRate);
}

/**
 * @brief Set Delay OFF time
 * @param f32Seconds Delay time in seconds (0.000 to 60.000)
 */
static inline void DDS_SetDelayOff(float32_t f32Seconds) {
  if (f32Seconds < 0.0f)
    f32Seconds = 0.0f;
  if (f32Seconds > 60.0f)
    f32Seconds = 60.0f;
  sDDS.u32DelayOffTarget = (uint32_t)(f32Seconds * sDDS.u32SampleRate);
}

/**
 * @brief Set Phase ON angle
 * @param f32Degrees Start angle in degrees (-1.0 = immediate, 0.0 to 360.0)
 */
static inline void DDS_SetPhaseOn(float32_t f32Degrees) {
  if (f32Degrees < -1.0f)
    f32Degrees = -1.0f;
  if (f32Degrees > 360.0f)
    f32Degrees = 360.0f;
  sDDS.s16PhaseOnDegrees = (int16_t)f32Degrees;
}

/**
 * @brief Set Phase OFF angle
 * @param f32Degrees Stop angle in degrees (-1.0 = immediate, 0.0 to 360.0)
 */
static inline void DDS_SetPhaseOff(float32_t f32Degrees) {
  if (f32Degrees < -1.0f)
    f32Degrees = -1.0f;
  if (f32Degrees > 360.0f)
    f32Degrees = 360.0f;
  sDDS.s16PhaseOffDegrees = (int16_t)f32Degrees;
}

/**
 * @brief Check if any ramp is active
 * @return 1 if any ramp is active, 0 otherwise
 */
static inline uint16_t DDS_IsAnyRampActive(void) {
  return ((sDDS.sAmpRamp.u32State & _ENABLE_DDS_RAMP) ||
          (sDDS.sOffsetRamp.u32State & _ENABLE_DDS_RAMP) ||
          (sDDS.sFreqRamp.u32State & _ENABLE_DDS_RAMP))
             ? 1U
             : 0U;
}

/**
 * @brief Configure Amplitude Ramp
 * @param f32RampUpTime Ramp up time in seconds (0.000 to 60.000)
 * @param f32RampDownTime Ramp down time in seconds (0.000 to 60.000)
 * @param bEnable Enable/disable ramp (1=enable, 0=disable)
 */
static inline void DDS_SetAmplitudeRamp(float32_t f32RampUpTime,
                                        float32_t f32RampDownTime,
                                        uint16_t bEnable) {
  if (f32RampUpTime < 0.001f)
    f32RampUpTime = 0.001f;
  if (f32RampUpTime > 60.0f)
    f32RampUpTime = 60.0f;
  if (f32RampDownTime < 0.001f)
    f32RampDownTime = 0.001f;
  if (f32RampDownTime > 60.0f)
    f32RampDownTime = 60.0f;

  // Calculate step size: (1.0 PU) / (time_seconds * sample_rate)
  sDDS.sAmpRamp.f32StepUp = 1.0f / (f32RampUpTime * sDDS.u32SampleRate);
  sDDS.sAmpRamp.f32StepDown = 1.0f / (f32RampDownTime * sDDS.u32SampleRate);

  if (bEnable) {
    sDDS.sAmpRamp.u32State |= _ENABLE_DDS_RAMP;
    sDDS.sAmpRamp.f32Out =
        (float32_t)sDDS.u16Amplitude / 65535.0f; // Initialize to current value
  } else {
    sDDS.sAmpRamp.u32State &= ~_ENABLE_DDS_RAMP;
  }
  sDDS.bAnyRampActive = DDS_IsAnyRampActive();
}

/**
 * @brief Configure Offset Ramp
 * @param f32RampUpTime Ramp up time in seconds (0.000 to 60.000)
 * @param f32RampDownTime Ramp down time in seconds (0.000 to 60.000)
 * @param bEnable Enable/disable ramp (1=enable, 0=disable)
 */
static inline void DDS_SetOffsetRamp(float32_t f32RampUpTime,
                                     float32_t f32RampDownTime,
                                     uint16_t bEnable) {
  if (f32RampUpTime < 0.001f)
    f32RampUpTime = 0.001f;
  if (f32RampUpTime > 60.0f)
    f32RampUpTime = 60.0f;
  if (f32RampDownTime < 0.001f)
    f32RampDownTime = 0.001f;
  if (f32RampDownTime > 60.0f)
    f32RampDownTime = 60.0f;

  sDDS.sOffsetRamp.f32StepUp = 1.0f / (f32RampUpTime * sDDS.u32SampleRate);
  sDDS.sOffsetRamp.f32StepDown = 1.0f / (f32RampDownTime * sDDS.u32SampleRate);

  if (bEnable) {
    sDDS.sOffsetRamp.u32State |= _ENABLE_DDS_RAMP;
    sDDS.sOffsetRamp.f32Out = (float32_t)sDDS.u16Offset / 65535.0f;
  } else {
    sDDS.sOffsetRamp.u32State &= ~_ENABLE_DDS_RAMP;
  }
  sDDS.bAnyRampActive = DDS_IsAnyRampActive();
}

/**
 * @brief Configure Frequency Ramp
 * @param f32RampUpTime Ramp up time in seconds (0.000 to 60.000)
 * @param f32RampDownTime Ramp down time in seconds (0.000 to 60.000)
 * @param bEnable Enable/disable ramp (1=enable, 0=disable)
 */
static inline void DDS_SetFrequencyRamp(float32_t f32RampUpTime,
                                        float32_t f32RampDownTime,
                                        uint16_t bEnable) {
  if (f32RampUpTime < 0.001f)
    f32RampUpTime = 0.001f;
  if (f32RampUpTime > 60.0f)
    f32RampUpTime = 60.0f;
  if (f32RampDownTime < 0.001f)
    f32RampDownTime = 0.001f;
  if (f32RampDownTime > 60.0f)
    f32RampDownTime = 60.0f;

  // Frequency range: 1.00 Hz to 1000.00 Hz (normalized to 0.01 to 10.0)
  // Calculate step for full range sweep in given time
  float32_t f32FreqRange =
      sDDS.sFreqRamp.f32HighLimit - sDDS.sFreqRamp.f32LowLimit; // 1000.0 - 0.01
  sDDS.sFreqRamp.f32StepUp =
      f32FreqRange / (f32RampUpTime * sDDS.u32SampleRate);
  sDDS.sFreqRamp.f32StepDown =
      f32FreqRange / (f32RampDownTime * sDDS.u32SampleRate);

  if (bEnable) {
    sDDS.sFreqRamp.u32State |= _ENABLE_DDS_RAMP;
    // Initialize f32Out to current frequency (u32Frequency_x100 / 100 = Hz)
    sDDS.sFreqRamp.f32Out = (float32_t)sDDS.u32Frequency_x100 / 100.0f;
  } else {
    sDDS.sFreqRamp.u32State &= ~_ENABLE_DDS_RAMP;

    /*
     * When disabling frequency ramp, we must ensure PhaseIncrement matches
     * the target frequency immediately, as stepDDS optimization will skip this.
     */
    sDDS.u32PhaseIncrement =
        (uint32_t)((float32_t)sDDS.u32Frequency_x100 * sDDS.f32PhaseStepFactor);
  }
  sDDS.bAnyRampActive = DDS_IsAnyRampActive();
}

/**
 * @brief Get current DDS state
 * @return Current state (DDS_STAT enum)
 */
static inline DDS_STAT DDS_GetState(void) { return sDDS.fgState; }

/**
 * @brief Check if DDS is running
 * @return 1 if running, 0 otherwise
 */
static inline uint16_t DDS_IsRunning(void) {
  return (sDDS.fgState == DDS_STATE_RUNNING) ? 1U : 0U;
}

/**
 * @brief Check if DDS initialization is complete
 * @return 1 if complete, 0 otherwise
 */
static inline uint16_t DDS_IsInitComplete(void) { return sDDS.bInitComplete; }

/**
 * @brief Get current frequency setting
 * @return Frequency * 100 (100 to 100000)
 */
static inline uint32_t DDS_GetFrequency(void) { return sDDS.u32Frequency_x100; }

/**
 * @brief Get current amplitude setting
 * @return Amplitude (0-65535)
 */
static inline uint16_t DDS_GetAmplitude(void) { return sDDS.u16Amplitude; }

/**
 * @brief Get current offset setting
 * @return Offset (0-65535)
 */
static inline uint16_t DDS_GetOffset(void) { return sDDS.u16Offset; }

/**
 * @brief Check if a specific ramp is complete
 * @param u16RampIndex Ramp index: 0=Amplitude, 1=Offset, 2=Frequency
 * @return 1 if complete, 0 otherwise
 */
static inline uint16_t DDS_IsRampComplete(uint16_t u16RampIndex) {
  switch (u16RampIndex) {
  case 0: // Amplitude
    return (sDDS.sAmpRamp.u32State & _DDS_RAMP_DONE) ? 1U : 0U;
  case 1: // Offset
    return (sDDS.sOffsetRamp.u32State & _DDS_RAMP_DONE) ? 1U : 0U;
  case 2: // Frequency
    return (sDDS.sFreqRamp.u32State & _DDS_RAMP_DONE) ? 1U : 0U;
  default:
    return 0U;
  }
}

#endif /* DDS_DDS_API_H_ */
