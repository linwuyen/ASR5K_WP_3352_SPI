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


#define SET_DEBUG_MAIN_WHILE()          //GPIO_writePin(DEBUG_0, 1);
#define RST_DEBUG_MAIN_WHILE()          //GPIO_writePin(DEBUG_0, 0);

#define SET_DEBUG_TIME_TASK()           //GPIO_writePin(LP_J2_GPIO123, 1);
#define RST_DEBUG_TIME_TASK()           //GPIO_writePin(LP_J2_GPIO123, 0);
#define TGE_DEBUG_TIME_TASK()           //GPIO_togglePin(LP_J2_GPIO123);

#define SET_DEBUG_ADC_EOC()             //GPIO_writePin(CPU1_D10_GPIO31, 1);
#define RST_DEBUG_ADC_EOC()             //GPIO_writePin(CPU1_D10_GPIO31, 0);
#define TGE_DEBUG_ADC_EOC()             //GPIO_togglePin(CPU1_D10_GPIO31);

#define SET_DEBUG_CLA_TASK1()           //GPIO_writePin(24, 1);
#define RST_DEBUG_CLA_TASK1()           //GPIO_writePin(24, 0);



#endif /* CDEBUG_H_ */
