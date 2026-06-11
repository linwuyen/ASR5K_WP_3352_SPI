/*
 * dds_core.h
 *
 * Core DDS functions (Inline optimized)
 *
 * Created on: January 13, 2026
 * Author: Cody Chen
 */

#ifndef DDS_DDS_CORE_H_
#define DDS_DDS_CORE_H_

#include "dds_common.h"
#include <stdint.h>

/**
 * @brief Check if phase has reached or passed target within tolerance
 * @param u32CurrentPhase Current phase accumulator value
 * @param u32TargetPhase Target phase value
 * @return 1 if reached/passed, 0 otherwise
 */
static inline uint16_t isPhaseMatched(uint32_t u32CurrentPhase,
                                      uint32_t u32TargetPhase) {
  // Check if current phase is within tolerance window of target
  // Handle case where phase wraps around (32-bit overflow is intentional)
  int32_t s32Diff = (int32_t)(u32CurrentPhase - u32TargetPhase);

  // Return true if phase is at or past target (within forward tolerance window)
  // Allows for ±tolerance range to handle fast phase increments
  return ((s32Diff >= 0) && (s32Diff <= (int32_t)DDS_PHASE_TOLERANCE)) ? 1U
                                                                       : 0U;
}

/**
 * @brief Get DDS output sample (Inlined for speed)
 * @param hal DDS context pointer
 * @return 16-bit output sample
 */
static inline uint16_t getDDS(HAL_DDS hal) {
  if (hal->fgState & DDS_STATE_ERROR) {
    return hal->u16Offset;
  }

  // Extract table index using optimized bit shift and mask (4096-point table)
  // Note: Phase accumulator is updated in stepDDS(), not here
  uint16_t u16Index =
      (uint16_t)((hal->u32PhaseAccumulator >> DDS_PHASE_SHIFT_BITS) &
                 DDS_TABLE_MASK);

  // Get pre-calculated bipolar sine value from dedicated RAMGS4 memory
  int16_t s16SineVal = (int16_t)g_au16DdsSineTable[u16Index];

  // Amplitude scaling using Q15 multiply (optimized for C2000 hardware
  // multiplier)
  int32_t s32Scaled =
      ((int32_t)s16SineVal * (int32_t)hal->u16Amplitude) >> DDS_AMP_SHIFT_BITS;

  // Add DC offset and saturate (branchless operation)
  int32_t s32Result = s32Scaled + (int32_t)hal->u16Offset;

  // Efficient saturation using bit manipulation (branchless)
  if (s32Result < 0)
    return 0;
  if (s32Result > DDS_OUTPUT_MAX)
    return DDS_OUTPUT_MAX;

  return s32Result;
}

/**
 * @brief DDS State Machine Step (Inlined for speed)
 * @param hal DDS context pointer
 * @note This function implements the complete FSM logic with Delay, Phase, and
 * Ramp control
 */
static inline void stepDDS(HAL_DDS hal) {

  // Process Ramp control for Amplitude (only update target when changed)
  if (hal->sAmpRamp.u32State & _ENABLE_DDS_RAMP) {
    if (sDDS.fgState & DDS_STATE_RUNNING) {
      // Only update target if amplitude changed
      if (hal->u16Amplitude != hal->u16AmpRampTarget) {
        hal->u16AmpRampTarget = hal->u16Amplitude;
        hal->sAmpRamp.f32Target = (float32_t)hal->u16Amplitude / 65535.0f;
      }
    } else {
      hal->sAmpRamp.f32Target = 0.0f;
      hal->u16AmpRampTarget = 0.0f;
    }

    // Execute ramp calculation
    calDdsRamp(&hal->sAmpRamp);
    // Apply ramped value
    hal->u16Amplitude = (uint16_t)(hal->sAmpRamp.f32Out * 65535.0f);
  } else {
    // When ramp disabled, track current value
    hal->u16AmpRampTarget = hal->u16Amplitude;
  }

  // Process Ramp control for Offset (only update target when changed)
  if (hal->sOffsetRamp.u32State & _ENABLE_DDS_RAMP) {
    // Only update target if offset changed
    if (hal->u16Offset != hal->u16OffsetRampTarget) {
      hal->u16OffsetRampTarget = hal->u16Offset;
      hal->sOffsetRamp.f32Target = (float32_t)hal->u16Offset / 65535.0f;
    }
    // Execute ramp calculation
    calDdsRamp(&hal->sOffsetRamp);
    // Apply ramped value
    hal->u16Offset = (uint16_t)(hal->sOffsetRamp.f32Out * 65535.0f);
  } else {
    // When ramp disabled, track current value
    hal->u16OffsetRampTarget = hal->u16Offset;
  }

  // Process Ramp control for Frequency (only update target when changed)
  if (hal->sFreqRamp.u32State & _ENABLE_DDS_RAMP) {
    // Only update target if frequency changed
    if (hal->u32Frequency_x100 != hal->u32FreqRampTarget) {
      hal->u32FreqRampTarget = hal->u32Frequency_x100;
      // Convert to Hz: u32Frequency_x100 / 100 = Hz
      hal->sFreqRamp.f32Target = (float32_t)hal->u32Frequency_x100 / 100.0f;
    }
    // Execute ramp calculation
    calDdsRamp(&hal->sFreqRamp);
    // Apply ramped value and update phase increment
    // f32Out is in Hz, convert to frequency_x100 format (Hz * 100)
    uint32_t u32RampedFreq = (uint32_t)(hal->sFreqRamp.f32Out * 100.0f);

    /*
     * Optimization: Use pre-calculated float multiplication factor
     * Replaces expensive 64-bit division:
     * u32PhaseIncrement = (u32RampedFreq * 2^32) / (u32SampleRate * 100)
     */
    hal->u32PhaseIncrement =
        (uint32_t)((float32_t)u32RampedFreq * hal->f32PhaseStepFactor);
  } else {
    // When ramp disabled, track current value and update phase increment
    // immediately
    hal->u32FreqRampTarget = hal->u32Frequency_x100;

    /*
     * Optimization: Use pre-calculated float multiplication factor
     * Replaces expensive 64-bit division for immediate phase update
     */
    hal->u32PhaseIncrement =
        (uint32_t)((float32_t)hal->u32Frequency_x100 * hal->f32PhaseStepFactor);
  }

  // State machine processing
  switch (hal->fgState) {

  case DDS_STATE_STOPPED:
    // State 1: Waiting for Enable command
    if (hal->bStartRequested) {
      hal->bStartRequested = 0U;
      // Reset phase accumulator and delay counter for new start sequence
      hal->u32PhaseAccumulator = 0UL;
      hal->u32DelayCounter = 0UL;
      sDDS.fgRecordState = DDS_STATE_STARTED; // Mark Start
      hal->fgState = DDS_STATE_DELAY_ON;
    }
    break;

  case DDS_STATE_DELAY_ON:
    // State 2: Delay ON - counting to target
    // Phase accumulator stays at 0 during delay, will start counting in
    // PHASE_ON
    if (!hal->bStartRequested && hal->bStopRequested) {
      // Interrupted during delay - go directly to STOPPED
      hal->bStopRequested = 0U;
      hal->fgState = DDS_STATE_STOPPED;
      break;
    }

    hal->u32DelayCounter++;
    if (hal->u32DelayCounter >= hal->u32DelayOnTarget) {
      // Delay completed - check Phase ON setting
      if (hal->s16PhaseOnDegrees == DDS_PHASE_IMMEDIATE) {
        // No phase control - start from 0 degrees
        hal->u32PhaseAccumulator = 0UL;
      } else {
        // Set phase accumulator to target angle directly
        hal->u32PhaseAccumulator =
            DDS_DEGREES_TO_PHASE((float32_t)hal->s16PhaseOnDegrees);
      }
      // Start running immediately
      hal->fgState = DDS_STATE_RUNNING;
      sDDS.fgRecordState |= (DDS_STATE_DELAY_ON); // Mark DelayON
    }
    break;

  case DDS_STATE_RUNNING:
    // State 4: Active waveform generation with Ramp control
    // Update phase accumulator for waveform generation
    hal->u32PhaseAccumulator += hal->u32PhaseIncrement;

    sDDS.fgRecordState |= (DDS_STATE_RUNNING); // Mark Running

    if (hal->bStopRequested) {
      hal->bStopRequested = 0U;
      if (hal->sAmpRamp.u32State & _ENABLE_DDS_RAMP) {
        hal->fgState = DDS_STATE_AMP_RAMP_DOWN;
      } else {
        // Start shutdown sequence
        hal->u32DelayCounter = 0UL;
        hal->fgState = DDS_STATE_DELAY_OFF;
      }
    }
    break;

  case DDS_STATE_AMP_RAMP_DOWN:
    // Update phase accumulator for waveform generation
    hal->u32PhaseAccumulator += hal->u32PhaseIncrement;
    if (hal->sAmpRamp.u32State & _DDS_RAMP_DONE) {
      // Calculate target phase for Phase OFF and lock phase increment
      hal->u32TargetPhaseOff =
          DDS_DEGREES_TO_PHASE((float32_t)hal->s16PhaseOffDegrees);
      hal->u32PhaseOffIncrement =
          hal->u32PhaseIncrement; // Lock to prevent Ramp changes
      hal->fgState = DDS_STATE_PHASE_OFF;
    }

    if (hal->bStartRequested) {
      hal->bStartRequested = 0U;
      sDDS.fgRecordState &= (~DDS_STATE_AMP_RAMP_DOWN); // Mark Amp Ramp Down
      hal->fgState = DDS_STATE_RUNNING;
    }
    break;

  case DDS_STATE_DELAY_OFF:
    // State 5: Delay OFF - counting to target
    // Update phase accumulator during delay for continuous phase tracking
    hal->u32PhaseAccumulator += hal->u32PhaseIncrement;

    if (hal->bStartRequested) {
      // Re-enabled during delay - return to RUNNING
      hal->bStartRequested = 0U;
      hal->fgState = DDS_STATE_RUNNING;
      break;
    }

    hal->u32DelayCounter++;
    if (hal->u32DelayCounter >= hal->u32DelayOffTarget) {
      // Delay completed - check Phase OFF setting
      if (hal->s16PhaseOffDegrees == DDS_PHASE_IMMEDIATE) {
        // No phase control - stop immediately
        hal->fgState = DDS_STATE_STOPPED;
      } else {
        // Calculate target phase for Phase OFF and lock phase increment
        hal->u32TargetPhaseOff =
            DDS_DEGREES_TO_PHASE((float32_t)hal->s16PhaseOffDegrees);
        hal->u32PhaseOffIncrement =
            hal->u32PhaseIncrement; // Lock to prevent Ramp changes
        hal->fgState = DDS_STATE_PHASE_OFF;
        sDDS.fgRecordState |= (DDS_STATE_DELAY_OFF); // Mark Delay OFF
      }
    }
    break;

  case DDS_STATE_PHASE_OFF:
    // State 6: Waiting for Phase OFF angle (0.0~360.0 degrees)
    // Use locked phase increment to prevent Ramp interference
    hal->u32PhaseAccumulator += hal->u32PhaseOffIncrement;

    if (isPhaseMatched(hal->u32PhaseAccumulator, hal->u32TargetPhaseOff)) {
      // Target angle reached - stop waveform output
      hal->fgState = DDS_STATE_STOPPED;
      hal->u32PhaseAccumulator = 0UL;              // Reset phase
      sDDS.fgRecordState = DDS_STATE_STOPPED;      // Mark Start
      sDDS.fgRecordState |= (DDS_STATE_PHASE_OFF); // Mark Phase Off
    }
    break;

  case DDS_STATE_INIT_TABLE:
    // Table initialization in progress
    break;

  default:
    hal->fgState = DDS_STATE_ERROR;
    break;
  }
}

#endif /* DDS_DDS_CORE_H_ */
