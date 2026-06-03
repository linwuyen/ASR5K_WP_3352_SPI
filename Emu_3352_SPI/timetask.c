/*
 * timetask.c
 *
 *  Created on: May 17, 2024
 *      Author: User
 */

#include "common.h"
#include "./cla/initCLA.h"
#include "shareram.h"


void recalParameters(void)
{
//    initHwConfigAndDrvParams();
//    initParamsForCLA();

}

void updateFgStatus(HAL_DRV hal)
{

}

void pollSlowError(void);
void pollUpdateParamCLA(void);
void (*pollParams)(void) = pollSlowError;


void pollSlowError(void)
{

    pollParams = pollUpdateParamCLA;
}

void pollUpdateParamCLA(void)
{

    updateFgStatus(&sDrv);

    pollParams = pollSlowError;
}




void task25msec(void * s)
{
    pollParams();

    if(isCallbackReady()) {
        if(GETn_STAT(_CSTAT_INIT_PARAMS, sDrv)) {
            recalParameters();

            sAccessCPU1.f32Cpu1VinScale = sHwConfig.f32VoutScale;
            sAccessCPU1.f32Cpu1IoutScale = sHwConfig.f32IoutScale;

            SET_STAT(_CSTAT_INIT_PARAMS, sDrv);
        }
    }

    if((0 != sDrv.u32HeartBeat)&&(0 != sCLA.u32HeartBeat)) {
        SET_STAT(_CSTAT_THREAD_READY, sDrv);
    }


}

void task2D5msec(void * s)
{

    if(GET_STAT(_CSTAT_INIT_SUCCESS, sDrv)) {
        scanWarning();
    }

    measTimerLength(&sDrv.tpTaskLength);
}

void asapTask(void *s)
{
    runDebug();

    runManualFlashApi();
    runFlashStorage();

}

ST_TIMETASK time_task[] = {
        {task2D5msec,         0,   T_2D5MS},
        {task25msec,          0,   T_25MS},
        {asapTask,            0,        0},
        {0, 0, 0}
};


void pollTimeTask(void)
{
    scanTimeTask(time_task, (void *)0);
}
