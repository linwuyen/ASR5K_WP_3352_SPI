/*
 * HwConfig.h
 *
 *  Created on: Mar 18, 2024
 *      Author: cody_chen
 *      BuildDate_Version: Comment
 *      729_60001: Updated on 2025/12/30 by Cody
 *                 First Empty Project
 */

#ifndef HWCONFIG_H_
#define HWCONFIG_H_

#define FW_BUILDDATE                     729               // Start from 2024/1/1
#define FW_VERSION                       60001             // CPU1 Version Start from 10000

#define CPUCLK_FREQUENCY                 (DEVICE_SYSCLK_FREQ/1000000UL)       /* 200 MHz System frequency */
#define EPWMCLK_FREQ                     (DEVICE_SYSCLK_FREQ / 2)
#define PWM_FREQ_HZ                      100000.0f

#define T_BUCK_PWM_DEADBAND_NSEC         500UL   // (nsec)
#define T_MIN_DUTY_NSEC                  500UL   // (nsec)

// @ ADC_RESULT_Xy(X, y)
// X := ADCA, ADCB, ADCC (ADC module)
// y := SOC0 ~ SOC15 (Start-of-Conversion channel index)
#define ADC_12BITS_RESULT_Xy(X, y)       (((float)HWREGH(ADC ## X ##RESULT_BASE + y))*INV_UPU_12BITS)
#define ADC_16BITS_SPU_Xy(X, y)          (((float)HWREGH(ADC ## X ##RESULT_BASE + y))*INV_SPU_16BITS-1.0f)
#define ADC_16BITS_UPU_Xy(X, y)          (((float)HWREGH(ADC ## X ##RESULT_BASE + y))*INV_UPU_16BITS)
#define ADC_RESULT_Xy(X,y)               ADC_12BITS_RESULT_Xy(X, y)


/**
 * @brief Structure of hardware configuration parameters.
 * These parameters can be loaded from Flash or set to factory defaults.
 */
typedef struct {
    float32_t f32VoutScale;     ///< VOUT scaling factor (V/ADC_Unit)
    float32_t f32IoutScale;     ///< IOUT scaling factor (A/ADC_Unit)
} ST_HW_CONFIG;

/**
 * @brief Structure of derived hardware parameters (per-unit, scaling factors).
 *
 * This structure contains scaling factors and normalized (per-unit) values
 * derived from hardware configuration, used for control and protection logic.
 */
typedef struct {
    float32_t f32InvVoutScale;     /**< Inverse VOUT scaling factor. */
    float32_t f32InvRoutScale;      /**< Inverse resistance scale factor (ADC_Unit/Ohm). Used to convert ADC readings to Ohms (Ohm = ADC_Reading / f32InvRoutScale). */
} ST_HW_DERIVE;

extern void initHwConfig(ST_HW_CONFIG * p);
extern ST_HW_CONFIG sHwConfig;
extern ST_HW_DERIVE sHwDerive;

#endif /* HWCONFIG_H_ */
