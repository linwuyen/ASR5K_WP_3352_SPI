/*
 * timetask.c
 *
 *  Created on: July 30, 2024
 *      Author: fornyin_jiang
 */

#include "common.h"
#include "shareram.h"
#include "mb_slave/ModbusSlave.h"

void task25msec(void * s)
{

}

void task2D5msec(void * s)
{

    if(GET_STAT(_CSTAT_INIT_SUCCESS, sDrv)) {
        scanWarning();
    }

    measTimerLength(&sDrv.tpTaskLength);
}

void task1msec(void *s)
{

}

void asapTask(void *s)
{
    runManualFlashApi();
    runFlashStorage();

}

ST_TIMETASK time_task[] = {
        {task1msec,           0,    T_1MS},
        {task2D5msec,         0,  T_2D5MS},
        {task25msec,          0,   T_25MS},
        {asapTask,            0,        0},
        {0, 0, 0}
};


void pollTimeTask(void)
{
    scanTimeTask(time_task, (void *)0);
}
