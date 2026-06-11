/*
 * c28math.h
 *
 *  Created on: Mar 22, 2024
 *      Author: User
 */

#ifndef C28MATH_H_
#define C28MATH_H_

#include "cmath.h"


static inline bool_t isOverMinScale(float32_t data)
{
    if(0.0 < data) {
        return (F32_MIN_SCALE < data);
    }
    else if(0.0 > data) {
        return (-F32_MIN_SCALE > data);
    }
    else {
        return false;
    }
}


typedef volatile struct {
    float32_t x;
    float32_t y;
} ST_POINT;

#define DEFAULT_POINT   (ST_POINT){.x = 0, .y = 0}

typedef volatile struct {
    ST_POINT p1;
    ST_POINT p2;
} ST_LINE;

#define DEFAULT_LINE   (ST_LINE){.p1 = DEFAULT_POINT, .p2 = DEFAULT_POINT}

typedef ST_LINE * HAL_LINE;

typedef struct {
    float32_t f32Gain;
    float32_t f32Offset;
} ST_GAIN_OFFSET;

typedef volatile struct {
    float32_t f32Input;
    float32_t f32Gain;
    float32_t f32Offset;
    float32_t f32Out;
    float32_t  f32Base;
    float32_t  f32Real;
}  ST_CAL;

typedef ST_CAL * HAL_CAL;

typedef volatile struct {
    float32_t  f32Out;           // Output: controller output
    float32_t  f32Max;           // Parameter: Limit the controller output by Maximum
    float32_t  f32Min;           // Parameter: Limit the controller output by Minimum
    float32_t  f32Kp;            // Parameter: proportional loop gain
    float32_t  f32Ki;            // Parameter: integral gain (0.0~1.0)
    float32_t  f32Err;           // Data: (f32) Error between the feedback and reference
    float32_t  f32Pdata;         // Data: (f32) Save the proportional term results
    float32_t  f32Idata;         // Data: (f32) Save the integral term
    float32_t  f32Itemp;         // Data: (F32) Temporarily save the integral term i(k-1)
    float32_t  f32Sum;           // Data: Sum of proportional term and integral term
    uint16_t   u16Reserved;
    uint16_t   u16StopUi;        // Parameter: Used to stop the integral term.
}   ST_PI;
typedef ST_PI * HAL_PI;

static inline void getAcCaliData(float32_t f32Input,  HAL_CAL v)
{
    v->f32Input = f32Input;
    v->f32Out = (v->f32Input - v->f32Offset) * v->f32Gain;
    v->f32Real = v->f32Out * v->f32Base;
}

static inline void getCaliData(float32_t f32Input, HAL_CAL v)
{
    v->f32Input = f32Input;
    v->f32Out = v->f32Input * v->f32Gain + v->f32Offset;
    v->f32Real = v->f32Out * v->f32Base;
}

static inline void getCaliGainAndOffset(HAL_LINE d, HAL_CAL v)
{
    v->f32Gain = (d->p2.y-d->p1.y)/v->f32Base/(d->p2.x-d->p1.x);
    v->f32Offset = d->p2.y/v->f32Base - v->f32Gain*d->p2.x;
}


static inline float32_t csatf(float32_t data, float32_t max, float32_t min)
{
    if(max < data) return max;
    if(min > data) return min;
    return data;
}

static inline float32_t csatSpu(float32_t data)
{
    return csatf(data, F32_SPU_MAX, F32_SPU_MIN);
}

static inline float32_t csatUpu(float32_t data)
{
    return csatf(data, F32_UPU_MAX, F32_UPU_MIN);
}

static inline float32_t cnvSpu2Upu(float32_t data)
{
    return (data * 0.5f + 0.5f);
}

static inline float32_t cnvUpu2Spu(float32_t data)
{
    return ((data - 0.5f) * 2.0f);
}

static inline void mPI(volatile ST_PI *p)
{
    if(false == isOverMinScale(p->f32Err)) {
        p->f32Err = 0.0f;
    }

    /* integral term */
    if((p->f32Sum == p->f32Out)&&(false == p->u16StopUi)) {
        p->f32Idata = p->f32Itemp + p->f32Ki * p->f32Err;
    }
    else {
        p->f32Idata = p->f32Itemp;
    }
    p->f32Itemp = p->f32Idata;

    /* proportional term */
    p->f32Pdata = p->f32Kp * p->f32Err;

    /* control output */
    p->f32Sum = p->f32Pdata + p->f32Idata;
    p->f32Out = csatf(p->f32Sum, p->f32Max, p->f32Min);
}

typedef struct {    float32_t    f32Target;     // Input: Target input (pu)
                    float32_t    f32Step;       // Parameter: Increase/Decrease Step (pu)
                    float32_t    f32LowLimit;   // Parameter: Minimum limit (pu)
                    float32_t    f32HighLimit;  // Parameter: Maximum limit (pu)
                    float32_t    f32Out;        // Output: Target output (pu)
}  ST_RAMP;
typedef ST_RAMP * HAL_RAMP;

static inline void mRamp(HAL_RAMP v)
{
    if (v->f32Target > v->f32Out)  {
        v->f32Out += v->f32Step;
        if (v->f32Target < v->f32Out)  v->f32Out = v->f32Target;
        if (v->f32Out > v->f32HighLimit) v->f32Out = v->f32HighLimit;
    }
    else if (v->f32Target < v->f32Out){
        v->f32Out -= v->f32Step;
        if (v->f32Target > v->f32Out)  v->f32Out = v->f32Target;
        if (v->f32Out < v->f32LowLimit) v->f32Out = v->f32LowLimit;
    }
}

#endif /* C28MATH_H_ */
