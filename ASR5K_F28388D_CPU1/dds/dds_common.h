/*
 * context.h
 *
 * DDS FSM Context Data Structure
 *
 * Created on: January 7, 2026
 * Author: Cody Chen
 */

#ifndef DDS_DDS_COMMON_H_
#define DDS_DDS_COMMON_H_

#include "common.h"
#include "dds_config.h"
#include "dds_timectrl.h"
#include <stdint.h>

/**
 * @brief DDS Context - all state data in one structure
 * @note Memory layout optimized for cache efficiency with high-frequency
 * variables first
 */
typedef struct {
  /** @brief Current FSM state */
  DDS_STAT fgState;
  DDS_STAT fgRecordState;

  /** @brief High-frequency access variables (cache-optimized placement) */
  uint32_t
      u32PhaseAccumulator; /**< Most frequently accessed - phase accumulator */
  uint32_t u32PhaseIncrement; /**< Second most frequent - phase increment per
                                 sample */
  uint16_t u16Amplitude;      /**< Used in every sample generation */
  uint16_t u16UserAmplitude;  /**< User setting (preserved during DC mode) */
  uint16_t u16Offset;         /**< Used in every sample generation */

  /** @brief DDS Parameters (less frequent access) */
  uint32_t u32Frequency_x100; /**< Frequency * 100 (100 to 100000, 1.00 to
                                 1000.00 Hz) */
  uint32_t u32SampleRate;     /**< Sample rate in Hz */

  /** @brief Ramp target values (store last applied targets) */
  uint16_t u16AmpRampTarget;    /**< Last amplitude target for Ramp */
  uint16_t u16OffsetRampTarget; /**< Last offset target for Ramp */
  uint32_t u32FreqRampTarget;   /**< Last frequency target for Ramp */

  /** @brief Table initialization progress */
  uint16_t u16TableIndex; /**< Current table building index (0-4095) */

  ST_DDSRAMP sAmpRamp;
  ST_DDSRAMP sOffsetRamp;
  ST_DDSRAMP sFreqRamp;

  /** @brief Status flags */
  uint16_t bTableReady;     /**< Sine table ready flag */
  uint16_t bInitComplete;   /**< Initialization complete flag */
  uint16_t bStartRequested; /**< Start request flag */
  uint16_t bStopRequested;  /**< Stop request flag */
  uint16_t
      bAnyRampActive; /**< Cached flag: (Amp | Offset | Freq) Ramp Active */

  /** @brief Delay timing counters */
  uint32_t u32DelayCounter;   /**< Current delay counter (universal) */
  uint32_t u32DelayOnTarget;  /**< Startup delay target value */
  uint32_t u32DelayOffTarget; /**< Shutdown delay target value */

  /** @brief Phase control variables */
  int16_t s16PhaseOnDegrees;     /**< Startup phase angle (-1=immediate,
                                    0-360=specified angle) */
  int16_t s16PhaseOffDegrees;    /**< Shutdown phase angle (-1=immediate,
                                    0-360=specified angle) */
  uint32_t u32TargetPhaseOff;    /**< Shutdown target phase value */
  uint32_t u32PhaseOffIncrement; /**< Locked phase increment for Phase OFF state
                                    (prevents Ramp interference) */

  /** @brief Pre-calculated factor for phase increment optimization */
  float32_t f32PhaseStepFactor; /**< = 2^32 / (SampleRate * 100) */

} ST_DDS;

typedef ST_DDS *HAL_DDS;

/** @brief External declaration for 4096-point sine table in dedicated RAMGS4
 * memory */
extern uint16_t g_au16DdsSineTable[DDS_TABLE_SIZE];

/** @brief External DDS context instance */
extern ST_DDS sDDS;

/**
 * @brief Function declarations
 */
DDS_STAT initDDS(HAL_DDS hal);
void runDDS(void);

#endif /* DDS_DDS_COMMON_H_ */
