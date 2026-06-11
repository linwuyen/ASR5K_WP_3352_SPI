/*
 * cali_system.h
 *
 *  Created on: Apr 9, 2024
 *      Author: User
 */

#ifndef CALI_SYSTEM_H_
#define CALI_SYSTEM_H_

#include "common.h"

#define CALI_LPF_K1           0.001f
#define VALID_OFFSET_RANGE    0.01f
#define VALID_POS_OFFSET      (VALID_OFFSET_RANGE)
#define VALID_NEG_OFFSET      (-VALID_OFFSET_RANGE)

#define DEFAULT_ST_CALI_SYSTEM (ST_CALI_SYSTEM) {  .fsm = _CALI_NO_ACTION, \
                                                   .sLine = DEFAULT_LINE, \
                                                   .sPoint = DEFAULT_POINT \
                                                }


#if 0 // if define CALI_AC_PARAM(IL), must be disable this function
static inline void caliAcParamIL(HAL_CAL p, HAL_CALI_SYSTEM v)
{
    switch(v->fsm)
    {
    case _RUN_IL_CALIBRATION:
    case _INIT_IL_PARAMETERS:
        p->f32Gain = 1.0f;
        p->f32Offset = 1.0f;
        v->sPoint = DEFAULT_POINT;
        FG_SWTO(_GET_IL_ADC_OFFSET, v->fsm);
    case _GET_IL_ADC_OFFSET:
        v->f32Temp = (p->f32Input - p->f32Offset);
        p->f32Offset += CALI_LPF_K1 * v->f32Temp;
        break;

    case _RECORD_IL_ADC_OFFSET:
        if((VALID_POS_OFFSET < p->f32Offset)||(VALID_NEG_OFFSET > p->f32Offset)) {
            FG_SET(_MARK_CALI_ERROR, v->fsm);
        }
        else {
            FG_SWTO(_GET_IL_ADC_GAIN, v->fsm);
        }
    case _GET_IL_ADC_GAIN:
        v->sPoint.x += CALI_LPF_K1 * (p->f32Out - v->sPoint.x);
        break;
    case _RECORD_IL_ADC_GAIN:
        p->f32Gain = (v->sPoint.y / p->f32Base) / v->sPoint.x;
        // User need to keyin the real value into sPoint.y
        FG_SWTO(_WAIT_FOR_OVP_SET_ZERO, v->fsm);
        break;

    case _WAIT_FOR_IL_SET_ZERO:
        break;
    case _EXIT_IL_CALIBRATION:
        break;
    default:
        break;
    }
}

static inline void caliDcParamVO(HAL_CAL p, HAL_CALI_SYSTEM v)
{
    switch(v->fsm)
    {
    case _RUN_VO_CALIBRATION:
    case _INIT_VO_PARAMETERS:
        p->f32Gain = 1.0f;
        p->f32Offset = 0.0f;
        v->sLine = DEFAULT_LINE;
        FG_SWTO(_GET_VO_POINT1, v->fsm);
    case _GET_VO_POINT1:
        v->f32Temp = (p->f32Input - v->sLine.p1.x);
        v->sLine.p1.x += CALI_LPF_K1 * v->f32Temp;
        //Keyin the real value to v->sLine.p1.y
        break;

    case _RECORD_VO_POINT1:
        FG_SWTO(_GET_VO_POINT2, v->fsm);

    case _GET_VO_POINT2:
        v->f32Temp = (p->f32Input - v->sLine.p2.x);
        v->sLine.p2.x += CALI_LPF_K1 * v->f32Temp;
        //Keyin the real value to v->sLine.p2.y
        break;
    case _RECORD_VO_POINT2:
        p->f32Gain = (v->sLine.p2.y-v->sLine.p1.y)/p->f32Base/(v->sLine.p2.x-v->sLine.p1.x);
        p->f32Offset = v->sLine.p2.y/p->f32Base - p->f32Gain*v->sLine.p2.x;
        FG_SWTO(_WAIT_FOR_VO_SET_ZERO, v->fsm);
        break;

    case _WAIT_FOR_VO_SET_ZERO:
        break;
    case _EXIT_VO_CALIBRATION:
        break;
    default:
        break;
    }
}
#else
    CALI_AC_PARAM(IO)
    CALI_DC_PARAM(VO)
    CALI_DC_PARAM(OVP)
    CALI_DC_PARAM(VIN)
#endif //


static inline void runCalibration(HAL_CALI_SYSTEM v)
{
    caliDcParamOVP(&sDrv.sOVP.sCali, v);
}


#endif /* CALI_SYSTEM_H_ */
