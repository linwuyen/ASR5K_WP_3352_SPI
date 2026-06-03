/*
 *  File Name: tbmenu.c
 *
 *  Created on: 5/13/2026
 *  Author: POWER-532A86
 */

#include "ModbusSlave.h"
#include "mbcmd.h"


REG_MBUSDATA regMbusData;

int chkValidAddress(uint16_t addr) {
    if (addr < _size_of_mbslave_id) {
        return 0;
    } else {
        return MB_ERROR_ILLEGALADDR;
    }
}

uint16_t getModbusData(uint16_t addr) {
    if (addr < _size_of_mbslave_id) {
        return regMbusData.u16MbusData[addr];
    } else {
        return 0xFFFF;
    }
}

uint16_t setModbusData(uint16_t addr, uint16_t data) {
    if (addr < _size_of_mbslave_id) {
        regMbusData.u16MbusData[addr] = data;
        return data;
    } else {
        return 0xFFFF;
    }
}



