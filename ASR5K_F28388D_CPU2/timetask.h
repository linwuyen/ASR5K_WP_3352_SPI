/*
 * timetask.h
 *
 *  Created on: Mar 1, 2024
 *      Author: User
 */

#ifndef TIMETASK_H_
#define TIMETASK_H_

#define SW_TIMER        50000000
#define U32_COUNTER     (uint32_t)CPUTimer_getTimerCount(SWTIRMER_BASE)

#define U32_UPCNTS      (SW_TIMER - U32_COUNTER)

#define T_10US          (SW_TIMER/100000)
#define T_20US          (SW_TIMER/50000)
#define T_50US          (SW_TIMER/20000)
#define T_100US         (SW_TIMER/10000)
#define T_200US         (SW_TIMER/5000)
#define T_500US         (SW_TIMER/2000)
#define T_1MS           (SW_TIMER/1000)
#define T_2MS           (SW_TIMER/500)
#define T_2D5MS         (SW_TIMER/400)
#define T_5MS           (SW_TIMER/200)
#define T_10MS          (SW_TIMER/100)
#define T_20MS          (SW_TIMER/50)
#define T_25MS          (SW_TIMER/40)
#define T_50MS          (SW_TIMER/20)
#define T_100MS         (SW_TIMER/10)
#define T_200MS         (SW_TIMER/5)
#define T_500MS         (SW_TIMER/2)
#define T_1S            (SW_TIMER/1)

typedef struct _ST_TIMETASK{
    void (*fn) (void * s);
    uint32_t cnt;
    uint32_t max;
} ST_TIMETASK;

typedef  ST_TIMETASK * HAL_TIMETASK;

static inline void scanTimeTask(HAL_TIMETASK t, void *s)
{
    static HAL_TIMETASK  pBegin = 0;
    static HAL_TIMETASK  pNext = 0;

    static uint32_t delta_t;
    static uint32_t u32Cnt = 0;

    if(0 == pBegin) {
        pBegin = pNext = t;
    }
    else {
        u32Cnt = SW_TIMER - U32_COUNTER;

        if(pNext->cnt > u32Cnt) {
            delta_t = u32Cnt + SW_TIMER - pNext->cnt;
        }
        else {
            delta_t = u32Cnt - pNext->cnt;
        }

        if(delta_t >= pNext->max) {
            pNext->fn(s);
            pNext->cnt = u32Cnt;
        }

        pNext++;
        if(0 == pNext->fn) pNext = pBegin;
    }

}

extern void pollTimeTask(void);

#endif /* TIMETASK_H_ */
