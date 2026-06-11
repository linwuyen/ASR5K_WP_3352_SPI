/*
 * c28param.h
 *
 *  Created on: Mar 19, 2024
 *      Author: cody_chen
 */

#ifndef C28PARAM_H_
#define C28PARAM_H_

#include "c28math.h"
#include "cTimeMeas.h"

/** @brief Get specific status bit from fgStatus. */
#define GETn_STAT(x, src) FG_GETn(x, src.fgStatus)
/** @brief Get status from fgStatus. */
#define GET_STAT(x, src)  FG_GET(x, src.fgStatus)
/** @brief Set status in fgStatus. */
#define SET_STAT(x, src)  FG_SET(x, src.fgStatus)
/** @brief Reset status in fgStatus. */
#define RST_STAT(x, src)  FG_RST(x, src.fgStatus)

/** @brief Perform AND operation on fgError. */
#define AND_ERR(x, src)   FG_AND(x, src.fgError)
/** @brief Get error from fgError. */
#define GET_ERR(x, src)   FG_GET(x, src.fgError)
/** @brief Set error in fgError. */
#define SET_ERR(x, src)   FG_SET(x, src.fgError)
/** @brief Reset error in fgError. */
#define RST_ERR(x, src)   FG_RST(x, src.fgError)

/** @brief Get PWM control register status. */
#define FG_GETPWM(x)      FG_GET(x, sCLA.sPWM.u16CtrlReg)
/** @brief Set PWM control register. */
#define FG_SETPWM(x)      FG_SET(x, sCLA.sPWM.u16CtrlReg)
/** @brief Reset PWM control register. */
#define FG_RSTPWM(x)      FG_RST(x, sCLA.sPWM.u16CtrlReg)

/** @enum ID_CSTAT
 *  @brief Driver status definitions.
 */
typedef enum {
    _CSTAT_NO_ACTION      =  (0x0000U), /**< No action. */
    _CSTAT_INIT_DRV_PARAM =  BIT(0),    /**< Initialize driver parameters. */
    _CSTAT_INIT_CLA_PARAM =  BIT(1),    /**< Initialize CLA parameters. */
    _CSTAT_INIT_PWMADC    =  BIT(2),    /**< Initialize PWM and ADC. */
    _CSTAT_THREAD_READY   =  (_CSTAT_INIT_DRV_PARAM|_CSTAT_INIT_CLA_PARAM|_CSTAT_INIT_PWMADC), /**< Thread ready. */
    _CSTAT_INIT_PARAMS    =  BIT(3),    /**< Initialize parameters. */
    _CSTAT_INIT_SUCCESS   =  (_CSTAT_THREAD_READY|_CSTAT_INIT_PARAMS), /**< Initialization successful. */
    _CSTAT_CLA_READY      =  BIT(4),    /**< CLA ready. */
    _CSTAT_OUTPUT_ON      =  BIT(5),    /**< Output enabled. */
    _CSTAT_BROWN_IN       =  BIT(6),    /**< Brown-in condition. */
    _CSTAT_ENABLE_PWM     =  BIT(7),    /**< Enable PWM. */
    _CSTAT_OUTPUT_READY   =  (_CSTAT_BROWN_IN|_CSTAT_ENABLE_PWM), /**< Output ready. */
    _CSTAT_CALIBRATION    =  BIT(8),    /**< Calibration mode. */
    _CSTAT_BLEEDER_OFF    =  BIT(9),    /**< Bleeder off. */
    _CSTAT_CANBUS_MASTER  =  BIT(10),   /**< CAN bus master mode. */
    _CSTAT_CANBUS_READY   =  BIT(11),   /**< CAN bus ready. */
    _CSTAT_SOURCE_SINK    =  BIT(12),   /**< Source/sink mode. */
    _CSTAT_CC_MODE        =  BIT(13),   /**< Constant current mode. */
    _CSTAT_CR_MODE        =  BIT(14),   /**< Constant resistance mode. */
    _CSTAT_CP_MODE        =  BIT(15)    /**< Constant power mode. */
} ID_CSTAT;

/** @union REG_CSTAT
 *  @brief Driver status register.
 */
typedef union {
    uint16_t u16All; /**< Full 16-bit status. */
    struct {
        uint16_t b0_init_drv_param:1; /**< Driver parameter initialization. */
        uint16_t b1_init_cla_param:1; /**< CLA parameter initialization. */
        uint16_t b2_init_pwmadc:1;    /**< PWM and ADC initialization. */
        uint16_t b3_init_params:1;    /**< Parameter initialization. */
        uint16_t b4_cla_ready:1;      /**< CLA ready. */
        uint16_t b5_output_on:1;      /**< Output enabled. */
        uint16_t b6_brown_in:1;       /**< Brown-in condition. */
        uint16_t b7_enable_pwm:1;     /**< PWM enabled. */
        uint16_t b8_calibration:1;    /**< Calibration mode. */
        uint16_t b9_bleeder_off:1;    /**< Bleeder off. */
        uint16_t b10_can_master:1;    /**< CAN bus master mode. */
        uint16_t b11_can_ready:1;     /**< CAN bus ready. */
        uint16_t b12_source_sink:1;   /**< Source/sink mode. */
        uint16_t b13_cc_mode:1;       /**< Constant current mode. */
        uint16_t b14_cr_mode:1;       /**< Constant resistance mode. */
        uint16_t b15_cp_mode:1;       /**< Constant power mode. */
    };
} REG_CSTAT;

/** @typedef FG_CLASTAT
 *  @brief CLA status flag type.
 */
typedef int16_t FG_CLASTAT;

/** @enum MARK_CLAERR
 *  @brief CLA error definitions.
 */
typedef enum {
    _NO_ERROR       = (0x0000U), /**< No error. */
    _ERR_VO_OVP     = BIT(0),    /**< Output overvoltage error. */
    _ERR_VIN_UVP    = BIT(1),    /**< Input undervoltage error. */
    _ERR_IO_OCP     = BIT(2),    /**< Output overcurrent error. */
    _ERR_VIN_OVP    = BIT(3),    /**< Input overvoltage error. */
    _ERR_ISR_MARGIN = BIT(6),    /**< Interrupt Service Routine margin error. */
    _ERR_M0_STAT    = BIT(7),    /**< M0 status error. */
    _ALL_ERROR      = (_ERR_VO_OVP|_ERR_VIN_UVP|_ERR_IO_OCP|_ERR_VIN_OVP|_ERR_ISR_MARGIN|_ERR_M0_STAT), /**< All errors. */
    _WARNING_VO     = BIT(8),    /**< Output voltage warning. */
    _WARNING_VIN    = BIT(9),    /**< Input voltage warning. */
    _WARNING_IO     = BIT(10),   /**< Output current warning. */
    _ALL_WARNING    = (_WARNING_VO|_WARNING_VIN|_WARNING_IO), /**< All warnings. */
    _CLAERR_ERROR   = 0xFFFF     /**< CLA error. */
} MARK_CLAERR;

/** @union REG_CLAERR
 *  @brief CLA error register.
 */
typedef union {
    uint16_t all; /**< Full 16-bit error. */
    MARK_CLAERR name; /**< Error name. */
    struct {
        uint16_t b00_07_buck_error:8; /**< Buck errors. */
        uint16_t b08_15_reserved:8;   /**< Reserved bits. */
    } group;
    struct {
        uint16_t b00_vo_ovp:1;        /**< Output overvoltage error. */
        uint16_t b01_vin_uvp:1;       /**< Input undervoltage error. */
        uint16_t b02_iout_ocp:1;      /**< Output overcurrent error. */
        uint16_t b03_vin_ovp:1;       /**< Input overvoltage error. */
        uint16_t b04_otp:1;           /**< Over-temperature error. */
        uint16_t b05_fan:1;           /**< Fan error. */
        uint16_t b06_buck_system:1;   /**< Buck system error. */
        uint16_t b07_m0_error:1;      /**< M0 error. */
        uint16_t b08_15_reserved:8;   /**< Reserved bits. */
    };
} REG_CLAERR;

/** @typedef HAL_CLAERR
 *  @brief Pointer to CLA error register.
 */
typedef REG_CLAERR * HAL_CLAERR;

/** @typedef FG_CLAERR
 *  @brief CLA error flag type.
 */
typedef uint16_t FG_CLAERR;

/** @struct ST_ADCLPF
 *  @brief ADC with low-pass filter structure.
 */
typedef struct {
    float32_t  f32Adc;  /**< ADC value. */
    ST_LPF     sLPF;    /**< Low-pass filter. */
    ST_CAL     sCali;   /**< Calibration. */
} ST_ADCLPF;

/** @struct ST_ADC
 *  @brief ADC structure.
 */
typedef struct {
    float32_t  f32Adc;  /**< ADC value. */
    ST_CAL     sCali;   /**< Calibration. */
} ST_ADC;


/** @union REG_SN
 *  @brief Serial number register.
 */
typedef union {
    uint32_t all; /**< Full 32-bit serial number. */
    struct {
        uint32_t number:24;   /**< Serial number. */
        uint32_t module:4;    /**< Module: 0:Invalid, 3:System, 7:Buck, 10:DAB, 13:PFC. */
        uint32_t reserved:2;  /**< Reserved bits. */
        uint32_t type:2;      /**< Type: 00:Invalid, 01:Test, 10:Valid, 11:Normal. */
    } bit;
} REG_SN;

/** @union REG32_RECODE
 *  @brief 32-bit record register.
 */
typedef union {
    uint32_t u32all; /**< Full 32-bit record. */
    struct {
        FG_CLASTAT  fgStatus; /**< Status flag. */
        FG_CLAERR   fgError;  /**< Error flag. */
    };
    struct {
        REG_CSTAT  regStatus; /**< Status register. */
        REG_CLAERR regError;  /**< Error register. */
    } bit;
} REG32_RECODE;


/** @struct ST_DRV
 *  @brief Main driver structure.
 */
typedef volatile struct {
    FG_CLASTAT  fgStatus;          /**< Status flag. */
    FG_CLAERR   fgError;           /**< Error flag. */
    REG_CLAERR  fgErrorMark;       /**< Error mark. */
    REG_CLAERR  fgErrorResult;     /**< Error result. */

    REG32_RECODE sRecord;          /**< Record register. */

    REG_SN u32SN;                  /**< Serial number. */

    uint32_t u32HeartBeat;         /**< Heartbeat counter. */

    ST_TIMER_MEAS tpIsrCost;       /**< Interrupt Service Routine cost timer. */
    ST_TIMER_MEAS tpIsrLength;     /**< Interrupt Service Routine length timer. */
    ST_TIMER_MEAS tpMainCost;      /**< Main cost timer. */
    ST_TIMER_MEAS tpTaskLength;    /**< Task length timer. */

} ST_DRV;

/** @typedef HAL_DRV
 *  @brief Pointer to driver structure.
 */
typedef ST_DRV * HAL_DRV;

/** @brief Global driver structure instance. */
extern ST_DRV sDrv;

/**
 * @brief Initialize hardware configuration and driver parameters.
 */
extern void initHwConfigAndDrvParams(void);

#endif /* C28PARAM_H_ */
