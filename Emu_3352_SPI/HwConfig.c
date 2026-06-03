/*
 * HwConfig.c
 *
 *  Created on: 2025-08-18
 *      Author: User
 */

#include "board.h"
#include "ctypedef.h"
#include "Hwconfig.h"


#pragma DATA_SECTION(sHwConfig, "cla_shared")
ST_HW_CONFIG sHwConfig = {
                          .f32VoutScale = 84.053f,
                          .f32IoutScale = 42.874f,
};

#pragma DATA_SECTION(sHwDerive, "cla_shared")
ST_HW_DERIVE sHwDerive;


void initHwConfig(ST_HW_CONFIG * p)
{
    // Calculate inverse scales for voltage, current, and input voltage
    sHwDerive.f32InvVoutScale = 1.0f / p->f32VoutScale;


}


