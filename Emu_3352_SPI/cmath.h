/*
 * cmath.h
 *
 *  Created on: 2025-07-18
 *      Author: User
 */

#ifndef CMATH_H_
#define CMATH_H_

#include <stdint.h>

#ifndef bool_t
/** @typedef bool_t
 *  @brief Boolean type definition.
 */
typedef bool bool_t;
#endif

#ifndef true
/** @brief True value for boolean. */
#define true  ((bool_t)1)
#endif

#ifndef false
/** @brief False value for boolean. */
#define false ((bool_t)0)
#endif

/** @brief Pi constant. */
#define _PI           (3.1415926f)
/** @brief 2 * Pi constant. */
#define _2PI          (2.0f*_PI)

/** @brief Minimum scale for float32. */
#define F32_MIN_SCALE (+0.0000305f)
/** @brief Maximum Scaled Per Unit value. */
#define F32_SPU_MAX   (+0.9999694f)
/** @brief Minimum Scaled Per Unit value. */
#define F32_SPU_MIN   (-1.000000f)
/** @brief Maximum Unscaled Per Unit value. */
#define F32_UPU_MAX   (+0.9999694f)
/** @brief Minimum Unscaled Per Unit value. */
#define F32_UPU_MIN   (+0.0000000f)

/**
 * @brief Low-pass filter structure.
 */
typedef volatile struct {
    float32_t f32Input;  /**< Latest input value. */
    float32_t f32K1;     /**< Filter coefficient (alpha), range: 0 < f32K1 < 1. */
    float32_t f32Err;    /**< Difference between input and output (optional debug use). */
    float32_t f32Out;    /**< Filtered output value. */
} ST_LPF;

/** @typedef HAL_LPF
 *  @brief Low-pass filter handle type.
 */
typedef ST_LPF * HAL_LPF;

/**
 * @brief Compute the Low-Pass Filter coefficient alpha (K1) from cutoff frequency and sampling frequency.
 *        This form guarantees stability: 0 < alpha < 1.
 *
 * @param Fc_hz Cutoff frequency in Hz.
 * @param Fs_hz Sampling frequency in Hz.
 * @return Computed alpha (K1) value.
 */
#define CAL_LPF_K1(Fc_hz, Fs_hz)  ((float32_t)((_2PI * (Fc_hz) / (Fs_hz)) / (1.0f + _2PI * (Fc_hz) / (Fs_hz))))

/**
 * @brief Initialize the Low-Pass Filter structure with cutoff and sampling frequency.
 *
 * @param v Pointer to Low-Pass Filter instance.
 * @param Fc_hz Cutoff frequency in Hz.
 * @param Fs_hz Sampling frequency in Hz.
 */
static inline void mLPF_Init(HAL_LPF v, float32_t Fc_hz, float32_t Fs_hz)
{
    if (!v || Fs_hz <= 0.0f) return;

    v->f32K1 = CAL_LPF_K1(Fc_hz, Fs_hz);
    v->f32Out = 0.0f;
    v->f32Input = 0.0f;
    v->f32Err = 0.0f;
}

/**
 * @brief Apply one-step Low-Pass Filter update using the direct calculation method.
 *        This avoids cumulative floating-point error over time.
 *
 * @param f32Input New input value.
 * @param v Pointer to Low-Pass Filter instance.
 */
static inline void mLPF(float32_t f32Input, HAL_LPF v)
{
    if (!v) return;

    v->f32Input = f32Input;

    // Apply Low-Pass Filter: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    v->f32Out = v->f32K1 * v->f32Input + (1.0f - v->f32K1) * v->f32Out;

    // Store error for observation (optional)
    v->f32Err = v->f32Input - v->f32Out;
}

#endif /* CMATH_H_ */
