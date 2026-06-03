/*
 *  File Name: linkEeprom.h
 *
 *  Created on: 5/13/2026
 *  Author: POWER-532A86
 */

typedef struct {
    uint16_t size;
    void *ptr;
} EE_REG;
typedef EE_REG * HAL_EEREG;

#define _TABLE_ADVPARAMS    5
