/*
 * ctypedef.h
 *
 *  Created on: Dec 18, 2024
 *      Author: cody_chen
 *
 *  Description:
 *  1. Defines multiple types aligned to FORCE_ALIGN_BY (default: 4 bytes).
 *  2. Suitable for embedded systems and hardware-related development, ensuring data alignment for better performance.
 *  3. Provides flexible alignment rules for future adjustments, enhancing code maintainability.
 *  4. Supports various integer (signed and unsigned) and floating-point data types.
 */

#ifndef CTYPEDEF_H_
#define CTYPEDEF_H_

#include <stdbool.h>
#include <stdint.h>

#define FORCE_ALIGN_BY   4  // Define the alignment size (default: 4 bytes)

// Floating-point types (aligned to 4 bytes)
typedef float64_t __attribute__((aligned(FORCE_ALIGN_BY))) f64_t;  // 64-bit floating point
typedef float32_t __attribute__((aligned(FORCE_ALIGN_BY))) f32_t;  // 32-bit floating point

// Unsigned integer types (aligned to 4 bytes)
typedef uint64_t __attribute__((aligned(FORCE_ALIGN_BY))) u64_t;  // 64-bit unsigned integer
typedef uint32_t __attribute__((aligned(FORCE_ALIGN_BY))) u32_t;  // 32-bit unsigned integer
typedef uint16_t __attribute__((aligned(FORCE_ALIGN_BY))) u16_t;  // 16-bit unsigned integer
typedef uint8_t  __attribute__((aligned(FORCE_ALIGN_BY))) u8_t;   // 8-bit unsigned integer

// Signed integer types (aligned to 4 bytes)
typedef int64_t  __attribute__((aligned(FORCE_ALIGN_BY))) s64_t;  // 64-bit signed integer
typedef int32_t  __attribute__((aligned(FORCE_ALIGN_BY))) s32_t;  // 32-bit signed integer
typedef int16_t  __attribute__((aligned(FORCE_ALIGN_BY))) s16_t;  // 16-bit signed integer
typedef int8_t   __attribute__((aligned(FORCE_ALIGN_BY))) s8_t;   // 8-bit signed integer

#define FG_GET(x, src)   ((src & (x)) == (x))
#define FG_GETn(x, src)  (!FG_GET(x, src))
#define FG_AND(x, src)   (src & (x))
#define FG_SET(x, src)   ((src) |= (x))
#define FG_RST(x, src)   ((src) &= ~(x))
#define FG_SWTO(x, src)  (src = (x))
#define FG_UPDATE(flag, cond, status) \
    do { if (cond) FG_SET(flag, status); else FG_RST(flag, status); } while(0)


#define BIT(n) (1UL << (n))

typedef enum {
    _EV_WAIT = (0x0000),
    _EV_OK   = BIT(0),
    _EV_SKIP = BIT(1),
    _EV_INIT = BIT(2),
    _EV_ERROR = BIT(3),
    _EV_START = BIT(4),
    _EV_STOP = BIT(5),
    _EV_FAIL = BIT(15)
} EV_STAT;

#define Q15_TO_FLOAT_MULTIPLIER  0.000030517578125f  /* 1/32768 */
#define Q15_MAX   ((int16_t)0x7FFF)
#define Q15_MIN   ((int16_t)0x8000)

#define FLOAT_TO_Q15(v)  ( \
    ( (v) >= 0.999969482f ) ? Q15_MAX : \
    ( (v) <= -1.0f )        ? Q15_MIN : \
    ( (int16_t)( ( (v) * 32768.0f ) + ( (v) >= 0.0f ? 0.5f : -0.5f ) ) ) \
)

#define Q15_TO_FLOAT(q15_value) \
    ( (float)((int16_t)(q15_value)) * Q15_TO_FLOAT_MULTIPLIER )



#define UPU2SPU(upu)        (upu * 0.5f + 0.5f)
#define SPU2UPU(spu)        ((spu - 0.5f) * 2.0f)

#define INV_UPU_12BITS    (1.0f/4096.0f)
#define INV_UPU_16BITS    (1.0f/65536.0f)
#define INV_SPU_16BITS    (1.0f/32768.0f)

#define PU_EPS_CMD   (0.30517578125e-4f)      // tolerance for cmd vs target compare

#endif /* CTYPEDEF_H_ */
