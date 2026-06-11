/*
 * dds_config.h
 *
 * DDS Configuration Constants
 *
 * Created on: January 7, 2026
 * Author: Cody Chen
 */

#ifndef DDS_CONFIG_H_
#define DDS_CONFIG_H_

#include <stdint.h>

/** @brief Performance tuning constants */
#define DDS_SAMPLE_RATE 100000UL /**< Sample rate in Hz */
#define DDS_TABLE_SIZE                                                         \
  4096U /**< Upgraded to 4096 points for higher precision */
#define DDS_MAX_FREQ_X100 100000UL /**< Maximum frequency: 1000.00 Hz */
#define DDS_MIN_FREQ_X100 100UL    /**< Minimum frequency: 1.00 Hz */
#define DDS_Q15_SCALE 32767        /**< Q15 format scale factor */
#define DDS_Q15_OFFSET 32768       /**< Q15 format offset (bipolar center) */

/** @brief Optimized constants for runtime performance (4096-point table) */
#define DDS_PHASE_SHIFT_BITS                                                   \
  20U                          /**< 32-12 = 20 for 4096-entry table (2^12)     \
                                */
#define DDS_TABLE_MASK 0xFFFU  /**< 4096-1 = 4095 mask for table index */
#define DDS_AMP_SHIFT_BITS 15U /**< Q15 format shift bits */
#define DDS_OUTPUT_MAX 65535U  /**< 16-bit maximum output value */

/**
 * @brief FSM State definitions
 * @note Each state represents a specific operational mode of the DDS system
 */
typedef enum {
  // Non-Output States (No waveform generation)
  DDS_STATE_IDLE = 0,
  DDS_STATE_INIT_TABLE = (1UL << 0),
  DDS_STATE_STOPPED = (1UL << 1),
  DDS_STATE_DELAY_ON = (1UL << 2), // Phase is 0, Output is Offset (DC)
  DDS_STATE_ERROR = (0x80000000),

  // Output States (Waveform generation, must be contiguous and at end)
  DDS_STATE_OUTPUT_START = (1UL << 3), // Marker for optimization
  DDS_STATE_RUNNING = DDS_STATE_OUTPUT_START,
  DDS_STATE_DELAY_OFF = (1UL << 4),
  DDS_STATE_PHASE_OFF = (1UL << 5),
  DDS_STATE_STARTED = (1UL << 6),
  DDS_STATE_AMP_RAMP_DOWN = (1UL << 7)
} DDS_STAT;

/** @brief Delay timing constants (in DDS_Step() calls) */
#define DDS_DELAY_ON_COUNT                                                     \
  100UL /**< Default startup delay count (configurable) */
#define DDS_DELAY_OFF_COUNT                                                    \
  50UL /**< Default shutdown delay count (configurable) */

/** @brief Phase control constants */
#define DDS_PHASE_IMMEDIATE -1 /**< Immediate execution flag (-1) */
#define DDS_DEGREES_TO_PHASE(deg)                                              \
  ((uint32_t)((deg) * 4294967296.0f / 360.0f)) /**< Convert degrees to phase   \
                                                */
#define DDS_PHASE_TOLERANCE                                                    \
  (DDS_DEGREES_TO_PHASE(1.0f)) /**< 1 degree tolerance for phase matching */

#endif /* DDS_CONFIG_H_ */
