/*
 * dds_buildTable.h
 *
 *  Created on: Jan 12, 2026
 *      Author: cody_chen
 */

#ifndef DDS_DDS_BUILDTABLE_H_
#define DDS_DDS_BUILDTABLE_H_

#include "dds_config.h"
#include <math.h>
#include <stdint.h>


uint16_t calSine(uint16_t u16Index) {
  // Pre-calculate bipolar sine values to eliminate runtime conversion
  double angle = 2.0 * 3.141592653589793 * u16Index / (double)DDS_TABLE_SIZE;
  int16_t s16SineVal = (int16_t)(sin(angle) * DDS_Q15_SCALE);

  // Store as signed 16-bit value (bipolar format) cast to uint16_t
  return (uint16_t)s16SineVal;
}

uint16_t buildSineTable(uint16_t *pTable, uint16_t *pIndex) {
  // Only calculate ONE entry per call for true non-blocking behavior
  if (*pIndex < DDS_TABLE_SIZE) {
    pTable[*pIndex] = calSine(*pIndex);
    (*pIndex)++;
  }

  // Return 1 if complete, 0 if more work needed
  return (*pIndex >= DDS_TABLE_SIZE) ? 1 : 0;
}

#endif /* DDS_DDS_BUILDTABLE_H_ */
