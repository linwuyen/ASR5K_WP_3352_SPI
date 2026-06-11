/*
 * initCLA.h
 *
 *  Created on: 2022¦~3¤ë29¤é
 *      Author: cody_chen
 */

#ifndef INITCLA_H
#define INITCLA_H

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memcfg.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "cla.h"
#include "memcfg.h"
#include "HwConfig.h"
#include "cla_task.h"

typedef struct {
   uint16_t fsm;
   uint16_t status;
   uint16_t error;
   uint16_t warning;
   uint32_t u32HeartBeat;
}ST_CLA;

extern void initCLA(void);

extern volatile ST_CLA sCLA;


#endif /* INITCLA_H */
