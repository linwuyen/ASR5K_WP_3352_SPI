/*
 * ModbusVSCI.h
 *
 *  Created on: Mar 5, 2024
 *      Author: cody_chen
 */

#ifndef MODBUSVSCI_H_
#define MODBUSVSCI_H_

#include "shareram.h"

static inline int16_t VSCI_base(uint32_t base)
{
    return 0;
}


static inline int16_t VSCI_getRxFIFOStatus(uint32_t base)
{
    return (sReadCPU1.pushRcnts != sAccessCPU2.popRcnts);
}

static inline uint16_t VSCI_readCharNonBlocking(uint32_t base)
{
    uint16_t u16Temp = sReadCPU1.u16RxRAM[sAccessCPU2.popRcnts];
    sAccessCPU2.popRcnts++;
    if(MBUS_SHARERAM_SIZE == sAccessCPU2.popRcnts) sAccessCPU2.popRcnts = 0;
    return u16Temp;
}

static inline int16_t VSCI_getTxFIFOStatus(uint32_t base)
{
    return (sReadCPU1.pushTcnts != sAccessCPU2.popTcnts);
}

static inline void VSCI_writeCharNonBlocking(uint32_t base, uint16_t data)
{
    sAccessCPU2.u16TxRAM[sAccessCPU2.pushTcnts] = data;
    sAccessCPU2.pushTcnts++;
    if(MBUS_SHARERAM_SIZE == sAccessCPU2.pushTcnts) sAccessCPU2.pushTcnts = 0;
}



#endif /* MODBUSVSCI_H_ */
