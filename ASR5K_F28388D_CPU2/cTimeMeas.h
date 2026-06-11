/*
 * cTimeMeas.h
 *
 *  Created on: Dec 19, 2024
 *      Author: cody_chen
 */

#ifndef CTIMEMEAS_H
#define CTIMEMEAS_H

#include <stdint.h>

// Enable time measurement functionality
#define ENABLE_TIMEMEAS

// ===============================================
// Timing Constants
// ===============================================

// Free-running timer rollover count (adjust according to system)
#define TIMER_ROLLOVER   50000000.0f

// Microsecond scaling factor (precomputed for efficiency)
#define USEC_SCALE       (1000000.0f / TIMER_ROLLOVER)

#define TIMER_MEAS_TICK  U32_UPCNTS


// ===============================================
// Timer Measurement Structure
// ===============================================
typedef volatile union{
    uint32_t u32all;
    struct {
        uint32_t b00_toggle:1;
        uint32_t reserved:31;
    };

}REG32_TSTATUS;

typedef volatile struct {
    uint32_t  u32StartTime;         // Start timestamp
    uint32_t  u32EndTime;           // End timestamp
    uint32_t  u32ElapsedTime;       // Elapsed timer ticks
    float32_t f32ElapsedUsec;       // Elapsed time in microseconds
    float32_t f32MaxElapsedUsec;    // Maximum recorded time in microseconds
    REG32_TSTATUS  u32Status;       // Timer Status
} ST_TIMER_MEAS;


// ===============================================
// Timer Control Functions
// ===============================================

/**
 * @brief Start time measurement.
 *
 * @param pTimer Pointer to timer structure.
 * @param u32CurrentCount Current value of the timer counter.
 */
static inline void startTimerMeasure(ST_TIMER_MEAS *pTimer) {
#ifdef ENABLE_TIMEMEAS
    pTimer->u32StartTime = U32_UPCNTS;
#endif
}


/**
 * @brief Stop time measurement and calculate elapsed time.
 *
 * @param pTimer Pointer to timer structure.
 * @param u32CurrentCount Current value of the timer counter.
 */
static inline void stopTimerMeasure(ST_TIMER_MEAS *pTimer) {
#ifdef ENABLE_TIMEMEAS
    pTimer->u32EndTime = U32_UPCNTS;

    // Calculate elapsed time, considering rollover
    if (pTimer->u32EndTime >= pTimer->u32StartTime) {
        pTimer->u32ElapsedTime = pTimer->u32EndTime - pTimer->u32StartTime;
    } else {
        pTimer->u32ElapsedTime = (uint32_t)(TIMER_ROLLOVER - pTimer->u32StartTime) + pTimer->u32EndTime;
    }

    // Convert elapsed time to microseconds
    pTimer->f32ElapsedUsec = USEC_SCALE * (float32_t)pTimer->u32ElapsedTime;

    // Update maximum if needed
    if (pTimer->f32ElapsedUsec > pTimer->f32MaxElapsedUsec) {
        pTimer->f32MaxElapsedUsec = pTimer->f32ElapsedUsec;
    }
#endif
}


/**
 * @brief Measure execution time using a toggle bitmask.
 *
 * This function toggles between start and stop timing based on the bitmask.
 *
 * @param pTimer Pointer to timer structure.
 * @param u32TpMask Bitmask identifying timing section.
 */
static inline void measTimerLength(ST_TIMER_MEAS *pTimer) {
#ifdef ENABLE_TIMEMEAS
    if (0 == pTimer->u32Status.b00_toggle) {
        // Start timing
        startTimerMeasure(pTimer);
        pTimer->u32Status.b00_toggle = 1;
    } else {
        // Stop timing
        stopTimerMeasure(pTimer);
        pTimer->u32Status.b00_toggle = 0;
    }
#endif
}


#endif // CTIMEMEAS_H
