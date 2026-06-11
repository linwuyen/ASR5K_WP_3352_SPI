/*
 * dds_ramp.h
 *
 *  Created on: Jan 12, 2026
 *      Author: cody_chen
 */

#ifndef DDS_DDS_RAMP_H_
#define DDS_DDS_RAMP_H_

#include <stdint.h>

typedef enum {
  _ENABLE_DDS_RAMP = (0x00000001 << 0),
  _DDS_RAMP_UP = (0x00000001 << 1),
  _DDS_RAMP_DOWN = (0x00000001 << 2),
  _DDS_RAMP_DONE = (0x00000001 << 3),
  _DDS_RAMP_LIMIT = (0x00000001 << 4),
} STAT_DDSRAMP;

typedef struct {
  uint32_t u32State;      // State flags (use STAT_DDSRAMP enum values)
  float32_t f32Target;    // Input: Target input (pu)
  float32_t f32StepUp;    // Parameter: Increase Step (pu)
  float32_t f32StepDown;  // Parameter: Decrease Step (pu)
  float32_t f32LowLimit;  // Parameter: Minimum limit (pu)
  float32_t f32HighLimit; // Parameter: Maximum limit (pu)
  float32_t f32Out;       // Output: Target output (pu)
} ST_DDSRAMP;

typedef ST_DDSRAMP *HAL_DDSRAMP;

static inline void calDdsRamp(HAL_DDSRAMP psRamp) {
  if (psRamp->u32State & _ENABLE_DDS_RAMP) {
    if (psRamp->f32Target > psRamp->f32Out) {
      psRamp->u32State = ((uint32_t)_DDS_RAMP_UP | (uint32_t)_ENABLE_DDS_RAMP);
      psRamp->f32Out += psRamp->f32StepUp;
      if (psRamp->f32Target < psRamp->f32Out) {
        psRamp->f32Out = psRamp->f32Target;
        psRamp->u32State |= _DDS_RAMP_DONE;
      }
      if (psRamp->f32Out > psRamp->f32HighLimit)
        psRamp->f32Out = psRamp->f32HighLimit;
    } else if (psRamp->f32Target < psRamp->f32Out) {
      psRamp->u32State =
          ((uint32_t)_ENABLE_DDS_RAMP | (uint32_t)_DDS_RAMP_DOWN);
      psRamp->f32Out -= psRamp->f32StepDown;
      if (psRamp->f32Target > psRamp->f32Out) {
        psRamp->f32Out = psRamp->f32Target;
        psRamp->u32State |= _DDS_RAMP_DONE;
      }
      if (psRamp->f32Out < psRamp->f32LowLimit)
        psRamp->f32Out = psRamp->f32LowLimit;
    }
  }
}

#endif /* DDS_DDS_RAMP_H_ */
