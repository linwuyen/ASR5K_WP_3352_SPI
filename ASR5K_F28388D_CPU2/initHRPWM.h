/*
 * initHRPWM.h
 *
 *  Created on: Mar 18, 2024
 *      Author: cody_chen
 */

#ifndef INIT_HRPWM_H
#define INIT_HRPWM_H

#include "common.h"
#include "SFO_V8.h"

/** @brief ADC trigger source. */
#define ADC_TRG_SRC     EPWM_SOC_A
/** @brief PWM base address. */
#define PWM_BASE        MEAS_CNV_BASE

/** @brief Deadband time in nanoseconds. */
#define T_DB_NSEC       T_BUCK_PWM_DEADBAND_NSEC
/** @brief PWM counts (half period). */
#define PWM_CNTS        (uint16_t)(((uint32_t)EPWMCLK_FREQ/(uint32_t)PWM_FREQ_HZ)>>1)
/** @brief High-Resolution PWM counts. */
#define HRPWM_CNTS      (uint32_t)(((uint32_t)PWM_CNTS)<<8U)

/** @brief PWM deadband counts. */
#define PWM_DB_CNTS     (uint16_t)(EPWMCLK_FREQ/1000*T_DB_NSEC/1000000)

/** @brief Maximum counting value. */
#define COUNTING_MAX    (HRPWM_CNTS-(1U<<8))

/** @brief Default Compare A value. */
#define DEFAULT_CMPA    (HRPWM_CNTS>>1)

// ===== Write CMPA (Q8 format) =====
/** @brief Write Compare A in Q8 format.
 *  @param cmp Compare A value in Q8 format (shifted before write).
 *  @param base PWM base address.
 */
#define SET_HRCMPA(cmp, base)   HWREG((base) + HRPWM_O_CMPA) = (cmp) << 8U

// ===== Deadband coarse/HR settings (Half-cycle mode) =====
/** @brief Set rising edge deadband coarse count.
 *  @param cnt Deadband count.
 *  @param base PWM base address.
 */
#define SET_DBRED_COARSE(cnt, base)    HWREGH((base) + EPWM_O_DBRED) = (uint16_t)(cnt)
/** @brief Set falling edge deadband coarse count.
 *  @param cnt Deadband count.
 *  @param base PWM base address.
 */
#define SET_DBFED_COARSE(cnt, base)    HWREGH((base) + EPWM_O_DBFED) = (uint16_t)(cnt)

// ===== Common DB settings =====
/** @brief Half-period coarse count for deadband. */
#define DB_COARSE_HALF_PERIOD   (PWM_CNTS)
/** @brief Normal deadband coarse count. */
#define DB_COARSE_NORMAL        (PWM_DB_CNTS)

/** @brief Clamp uint16_t value.
 *  @param v Value to clamp.
 *  @param lo Lower bound.
 *  @param hi Upper bound.
 *  @return Clamped value.
 */
#define CLAMP_U16(v, lo, hi)  ( (uint16_t)((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v))) )

/** @brief Safe deadband coarse value (enforce DB >= 3 and <= PWM_CNTS). */
#define SAFE_DB_COARSE(v)     CLAMP_U16((v), 3u, PWM_CNTS)

/** @brief Set High-Resolution PWM duty cycle.
 *  @param cmp Compare value.
 *  @param base PWM base address.
 */
#define SET_HRPWMAB_DUTY(cmp, base)                 \
    do {                                            \
        uint16_t _db = SAFE_DB_COARSE(DB_COARSE_NORMAL); \
        SET_HRCMPA((cmp), (base));                  \
        SET_DBRED_COARSE(_db, (base));              \
        SET_DBFED_COARSE(_db, (base));              \
        EPWM_clearTripZoneFlag(base, EPWM_TZ_FLAG_OST); \
        FG_SET(_CSTAT_OUTPUT_ON, sDrv.fgStatus); \
    } while (0)

/** @brief Reset High-Resolution PWM duty cycle.
 *  @param base PWM base address.
 */
#define RST_HRPWMAB_DUTY(base)                      \
    do {                                            \
        uint16_t _db = SAFE_DB_COARSE(DB_COARSE_HALF_PERIOD); \
        SET_HRCMPA((HRPWM_CNTS >> 1), (base));      \
        SET_DBRED_COARSE(_db, (base));              \
        SET_DBFED_COARSE(_db, (base));              \
        EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST); \
        FG_RST(_CSTAT_OUTPUT_ON, sDrv.fgStatus); \
    } while (0)

/** @brief Set PWM duty for ECAP (1% min/max for keep running).
 *  @param duty Duty cycle.
 *  @return PWM value.
 */
#define SET_PWM_FOR_ECAP(duty)           (uint32_t)(((float32_t) COUNTING_MAX) * (duty*0.99f+0.005f))

/** @brief Enable sync output. */
#define ENABLE_SYNCOUT()                  HRPWM_setActionQualifierAction(SYNC_PWM_BASE, \
                                          EPWM_AQ_OUTPUT_A, \
                                          EPWM_AQ_OUTPUT_HIGH, \
                                          EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
/** @brief Disable sync output. */
#define DISABLE_SYNCOUT()                 HRPWM_setActionQualifierAction(SYNC_PWM_BASE, \
                                          EPWM_AQ_OUTPUT_A, \
                                          EPWM_AQ_OUTPUT_LOW, \
                                          EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);

/** @brief PWM period in nanoseconds. */
#define PWM_PERIOD_NSEC                  (1000000000UL / PWM_FREQ_HZ) // (nsec) PWM period
/** @brief Minimum duty cycle (Per Unit). */
#define MIN_DUTY_PU                      ((float32_t)(T_MIN_DUTY_NSEC + T_BUCK_PWM_DEADBAND_NSEC) / (float32_t)PWM_PERIOD_NSEC)
/** @brief Set duty off value. */
#define SET_DUTY_OFF                     ((float32_t)(T_BUCK_PWM_DEADBAND_NSEC) / (float32_t)PWM_PERIOD_NSEC)
/** @brief Maximum duty cycle (Per Unit). */
#define MAX_DUTY_PU                      (1.0f - MIN_DUTY_PU)

/**
 * @brief Enable ADC trigger by PWM.
 * @param base PWM base address.
 */
static inline void enableAdcTriggerbyPWM(uint32_t base)
{
    EPWM_disableADCTrigger(base, ADC_TRG_SRC);
    EPWM_setADCTriggerSource(base, ADC_TRG_SRC, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(base, ADC_TRG_SRC, 1);
    EPWM_enableADCTrigger(base, ADC_TRG_SRC);
}

/**
 * @brief Initialize High-Resolution PWM for channels A and B.
 * @param base PWM base address.
 */
static inline void initHRPWMxAB(uint32_t base)
{
    uint16_t dead_time_ticks = (T_DB_NSEC * EPWMCLK_FREQ / 2) / 1000000000;
    HRPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_STOP_FREEZE);
    HRPWM_disablePhaseShiftLoad(base);
    HRPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    HRPWM_setPeriodLoadMode(base, EPWM_PERIOD_SHADOW_LOAD);
    HRPWM_selectPeriodLoadEvent(base, EPWM_SHADOW_LOAD_MODE_COUNTER_ZERO);
    HRPWM_setTimeBasePeriod(base, HRPWM_CNTS);
    HRPWM_setPhaseShift(base, 0U);
    HRPWM_setTimeBaseCounter(base, 0U);
    RST_HRPWMAB_DUTY(base);
    HRPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    HRPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);
    HRPWM_setCounterCompareShadowLoadEvent(base, HRPWM_CHANNEL_A, HRPWM_LOAD_ON_CNTR_ZERO);
    HRPWM_setCounterCompareShadowLoadEvent(base, HRPWM_CHANNEL_B, HRPWM_LOAD_ON_CNTR_ZERO);
    HRPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    HRPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    HRPWM_setMEPEdgeSelect(base, HRPWM_CHANNEL_A, HRPWM_MEP_CTRL_RISING_AND_FALLING_EDGE);
    HRPWM_setMEPControlMode(base, HRPWM_CHANNEL_A, HRPWM_MEP_DUTY_PERIOD_CTRL);
    HRPWM_setMEPEdgeSelect(base, HRPWM_CHANNEL_B, HRPWM_MEP_CTRL_RISING_AND_FALLING_EDGE);
    HRPWM_setMEPControlMode(base, HRPWM_CHANNEL_B, HRPWM_MEP_DUTY_PERIOD_CTRL);
    HRPWM_enableAutoConversion(base);
    HRPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN);
    HRPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED, EPWM_DB_POLARITY_ACTIVE_LOW);
    HRPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    HRPWM_setRisingEdgeDelayCountShadowLoadMode(base, EPWM_RED_LOAD_ON_CNTR_ZERO);
    HRPWM_setRisingEdgeDelayCount(base, dead_time_ticks);
    HRPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    HRPWM_setFallingEdgeDelayCountShadowLoadMode(base, EPWM_FED_LOAD_ON_CNTR_ZERO);
    HRPWM_setFallingEdgeDelayCount(base, dead_time_ticks);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
}


/**
 * @brief Initialize PWM.
 */
static inline void initPWM(void)
{
    uint16_t u16Status;
    do {
        u16Status = SFO();
    } while(u16Status == SFO_INCOMPLETE);
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
//    enableAdcTriggerbyPWM(PWM_BASE);
}

#endif /* INIT_HRPWM_H */
