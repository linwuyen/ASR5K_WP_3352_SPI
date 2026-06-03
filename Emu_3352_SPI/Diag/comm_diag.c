/*
 * comm_diag.c
 *
 * Created on: May 22, 2026
 * Author: Antigravity
 * Generic Communication Diagnostics Framework Implementation
 * ASCII-ONLY formatted for MS950 compilation safety.
 */

#include "comm_diag.h"

void CommDiag_Init(ST_COMM_DIAG *diag)
{
    if (diag != 0)
    {
        diag->u32TxTotal = 0U;
        diag->u32RxTotal = 0U;
        diag->u32ErrCount = 0U;
        diag->u32MaxQueueDepth = 0U;
        diag->u32ResetCount = 0U;

        diag->u16LastErrType = (uint16_t)ERR_COMM_NONE;
        diag->u16LastErrStep = 0U;
        diag->u32LastErrTime = 0U;

        diag->u16ErrDetail[0] = 0U;
        diag->u16ErrDetail[1] = 0U;
        diag->u16ErrDetail[2] = 0U;
        diag->u16ErrDetail[3] = 0U;
    }
}

void CommDiag_ReportError(ST_COMM_DIAG *diag, uint16_t errType, uint16_t step, 
                          uint32_t timestamp, uint16_t d0, uint16_t d1, 
                          uint16_t d2, uint16_t d3)
{
    if (diag != 0)
    {
        diag->u32ErrCount++;

        /* Latch only if there is no active error currently locked */
        if (diag->u16LastErrType == (uint16_t)ERR_COMM_NONE)
        {
            diag->u16LastErrType = errType;
            diag->u16LastErrStep = step;
            diag->u32LastErrTime = timestamp;
            diag->u16ErrDetail[0] = d0;
            diag->u16ErrDetail[1] = d1;
            diag->u16ErrDetail[2] = d2;
            diag->u16ErrDetail[3] = d3;
        }
    }
}

void CommDiag_ClearLatch(ST_COMM_DIAG *diag)
{
    if (diag != 0)
    {
        diag->u16LastErrType = (uint16_t)ERR_COMM_NONE;
        diag->u16LastErrStep = 0U;
        diag->u32LastErrTime = 0U;
        diag->u16ErrDetail[0] = 0U;
        diag->u16ErrDetail[1] = 0U;
        diag->u16ErrDetail[2] = 0U;
        diag->u16ErrDetail[3] = 0U;
    }
}

void CommDiag_UpdateQueueDepth(ST_COMM_DIAG *diag, uint32_t currentDepth)
{
    if (diag != 0)
    {
        if (currentDepth > diag->u32MaxQueueDepth)
        {
            diag->u32MaxQueueDepth = currentDepth;
        }
    }
}
