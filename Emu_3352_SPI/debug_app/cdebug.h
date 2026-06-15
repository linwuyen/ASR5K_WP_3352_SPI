/*
 * cdebug.h
 *
 *  Created on: 2022�~9��5��
 *      Author: cody_chen
 */

#ifndef CDEBUG_H_
#define CDEBUG_H_

#if ENABLE_DEBUG_DAC
// Debug unsigned int
#define DAC_U16(u16value, base)          DAC_setShadowValue(base, u16value);
// Debug unsigned pu
#define DAC_UPU(f32value, base)          DAC_setShadowValue(base, (uint16_t)(4095.0f * f32value));
// Debug signed pu
#define DAC_SPU(f32value, base)          DAC_setShadowValue(base, (uint16_t)(4095.0f * ((f32value*0.5f)+0.5f)));
// Debug absolute pu
#define DAC_ABS(f32value, base)          DAC_setShadowValue(base, (uint16_t)(4095.0f * f32value *(0.0 <= f32value? 1.0f: -1.0f)));

#else
// Debug unsigned int
#define DAC_U16(u16value, base)
// Debug unsigned pu
#define DAC_UPU(f32value, base)
// Debug signed pu
#define DAC_SPU(f32value, base)
// Debug absolute pu
#define DAC_ABS(f32value, base)

#endif //ENABLE_DEBUG_DAC

#define CPU1_LED852_GPIO135        135
#define CPU2_LED851_GPIO136        136

#define GPIO_PIN_SCIRXDA           136
#define DEBUG_SCI_SCIRX_GPIO       136
#define DEBUG_SCI_SCIRX_PIN_CONFIG GPIO_136_SCIA_RX
#define GPIO_PIN_SCITXDA           135
#define DEBUG_SCI_SCITX_GPIO       135
#define DEBUG_SCI_SCITX_PIN_CONFIG GPIO_135_SCIA_TX

#define DEBUG_SCI_BASE             SCIA_BASE
#define DEBUG_SCI_BAUDRATE         115200
#define DEBUG_SCI_CONFIG_WLEN      SCI_CONFIG_WLEN_8
#define DEBUG_SCI_CONFIG_STOP      SCI_CONFIG_STOP_ONE
#define DEBUG_SCI_CONFIG_PAR       SCI_CONFIG_PAR_EVEN

#define TEST_DUAL_CORE_COMM     0
#define TEST_TIMETASK_STABLE    (0x00000001 << 0)
#define TEST_MODBUS_ONLY        (0x00000001 << 2)
#define TEST_MODE               (TEST_MODBUS_ONLY)


#define ENABLE_TEST_ISR_TASK      0
#define ENABLE_TEST_DELAY_ON_OFF  0
#define ENABLE_TEST_SLEWRATE      0

#define ENABLE_TEST_VIN_VOUT      0

typedef struct {
    uint32_t u32InitCount;
    uint32_t u32RxByteCount;
    uint32_t u32TxByteCount;
    uint32_t u32ValidFrameCount;
    uint32_t u32RxTimeoutCount;
    uint16_t u16LastRxByte;
    uint16_t u16LastTxByte;
} ST_SCIA_DEBUG_DIAG;

#define ENABLE_CLA_DAC            (ENABLE_TEST_ISR_TASK+ENABLE_TEST_DELAY_ON_OFF+ENABLE_TEST_SLEWRATE)
#if ENABLE_CLA_DAC
#define CLA_DAC_H                 sCLA.f32DacOutA = 1.0f
#define CLA_DAC_L                 sCLA.f32DacOutA = 0.0f
#define CLA_DAC(upu)              sCLA.f32DacOutA = upu
#else
#define CLA_DAC_H
#define CLA_DAC_L
#define CLA_DAC(upu)
#endif

#if ENABLE_TEST_ISR_TASK
#define TEST_ISR_TASK_H()         CLA_DAC_H
#define TEST_ISR_TASK_L()         CLA_DAC_L
#else
#define TEST_ISR_TASK_H()
#define TEST_ISR_TASK_L()
#endif

#if ENABLE_TEST_DELAY_ON_OFF
#define TEST_ON_DELAY_H()         CLA_DAC_H
#define TEST_ON_DELAY_L()         CLA_DAC_L
#define TEST_OFF_DELAY_H()        CLA_DAC_H
#define TEST_OFF_DELAY_L()        CLA_DAC_L
#else
#define TEST_ON_DELAY_H()
#define TEST_ON_DELAY_L()
#define TEST_OFF_DELAY_H()
#define TEST_OFF_DELAY_L()
#endif

#if ENABLE_TEST_SLEWRATE
#define TEST_SLEWRATE(x)         CLA_DAC(x)
#else
#define TEST_SLEWRATE(x)         CLA_DAC(x)
#endif

extern void runDebug(void);
extern volatile ST_SCIA_DEBUG_DIAG g_sciaDebugDiag;


#endif /* CDEBUG_H_ */
