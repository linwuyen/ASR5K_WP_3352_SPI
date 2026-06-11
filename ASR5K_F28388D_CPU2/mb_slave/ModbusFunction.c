/*
 * ModbusFunction.c
 *
 *  Created on: 2018-03-28
 *      Author: user
 */

#include "ModbusCommon.h"
#include "ModbusSlave.h"
#include "ModbusVSCI.h"

#define HBYTE(x) ((x >> 8) & 0x00FF)
#define LBYTE(x) (x & 0x00FF)
#define MAX_MODBUS_REGISTERS 125 // Modbus standard max for multiple registers



int16_t ReservedFunction(SCI_MODBUS *mbus)
{
    mbus->errmsg = MB_ERROR_ILLEGALFUNC;
    return MBUS_FAIL;
}

int16_t readHoldingRegister(SCI_MODBUS *mbus)
{
    HEADER_READ_HOLDINGREG *pack = (HEADER_READ_HOLDINGREG *)mbus->pHeader;
    uint16_t u16Word = 0;

    switch (mbus->evFunc)
    {
    case _SEND_ADDRESS:
        mbus->info.rwfrom = (pack->Address.HiByte << 8) | pack->Address.LoByte;

        mbus->errmsg = (MSG_ERROR_ID)chkValidAddress(mbus->info.rwfrom);
        if (MB_NO_ERROR == mbus->errmsg)
        {
            // The number of bytes convert to the number of words.
            mbus->info.words = (((pack->Points.HiByte << 8) & 0xFF00) | pack->Points.LoByte);
            // Check quantity range: ensure words is within Modbus limits (1-125)
            if (mbus->info.words < 1 || mbus->info.words > MAX_MODBUS_REGISTERS)
            {
                mbus->errmsg = MB_ERROR_ILLEGALDATA; // Quantity out of range
                return MBUS_FAIL;
            }
            // Check address range: ensure rwfrom + words - 1 does not exceed valid range
            if (chkValidAddress(mbus->info.rwfrom + mbus->info.words - 1) != MB_NO_ERROR)
            {
                mbus->errmsg = MB_ERROR_ILLEGALDATA; // Address range error
                return MBUS_FAIL;
            }
            pushModbusByte(pack->Slave, mbus);
            pushModbusByte(pack->Function, mbus);
            mbus->evFunc = _SEND_BYTE_COUNTS;
        }
        else
        {
            return MBUS_FAIL;
        }
        // Fall through to _SEND_BYTE_COUNTS

    case _SEND_BYTE_COUNTS:
        pushModbusByte(mbus->info.words * 2, mbus);
        mbus->getReg((void *)mbus);
        mbus->evFunc = _SEND_DATA_OUT;
        break;

    case _SEND_DATA_OUT:
        if (mbus->info.index < mbus->info.words)
        {
            u16Word = getModbusData(mbus->info.rwfrom);
            pushModbusByte(HBYTE(u16Word), mbus);
            mbus->info.crc = pushModbusByte(LBYTE(u16Word), mbus);
            mbus->info.index++;
            mbus->info.rwfrom++;
        }
        else
        {
            pushModbusByte(LBYTE(mbus->info.crc), mbus);
            pushModbusByte(HBYTE(mbus->info.crc), mbus);
            mbus->evFunc = _SEND_ADDRESS;
            return MBUS_SUCCESS;
        }
        break;

    default:
        mbus->info = DEFAULT_REG_INFO;
        mbus->evFunc = _SEND_ADDRESS;
        mbus->errmsg = MB_ERROR_SLVFAILURE;
        break;
    }

    if (MB_NO_ERROR != mbus->errmsg)
        return MBUS_FAIL;

    return MBUS_WAIT;
}

int16_t writeHoldingRegister(SCI_MODBUS *mbus)
{
    HEADER_WRITE_HOLDINGREG *pack = (HEADER_WRITE_HOLDINGREG *)mbus->pHeader;
    uint16_t u16Word = 0;

    switch (mbus->evFunc)
    {
    case _SEND_ADDRESS:
        mbus->info.rwfrom = (pack->Address.HiByte << 8) | pack->Address.LoByte;

        mbus->errmsg = (MSG_ERROR_ID)chkValidAddress(mbus->info.rwfrom);
        if (MB_NO_ERROR == mbus->errmsg)
        {
            pushModbusByte(pack->Slave, mbus);
            pushModbusByte(pack->Function, mbus);
            mbus->evFunc = _SEND_MEMADDR;
            mbus->info.words = 1;
            // Get the start of Data.
            mbus->info.pData = &pack->Data;
        }
        else
        {
            return MBUS_FAIL;
        }

    case _SEND_MEMADDR:
        pushModbusByte(pack->Address.HiByte, mbus);
        pushModbusByte(pack->Address.LoByte, mbus);
        mbus->evFunc = _SEND_DATA_OUT;
        // Fall through to _SEND_DATA_OUT

    case _SEND_DATA_OUT:
        if (mbus->info.index < mbus->info.words)
        {
            u16Word = (uint16_t)((mbus->info.pData[mbus->info.index].HiByte << 8) & 0xFF00) | (mbus->info.pData[mbus->info.index].LoByte & 0x00FF);
            setModbusData(mbus->info.rwfrom, u16Word);
            pushModbusByte(mbus->info.pData[mbus->info.index].HiByte, mbus);
            mbus->info.crc = pushModbusByte(mbus->info.pData[mbus->info.index].LoByte, mbus);
            mbus->info.index++;
        }
        else
        {
            pushModbusByte(LBYTE(mbus->info.crc), mbus);
            pushModbusByte(HBYTE(mbus->info.crc), mbus);
            mbus->evFunc = _SEND_ADDRESS;
            mbus->setReg((void *)mbus);
            return MBUS_SUCCESS;
        }
        break;

    default:
        mbus->info = DEFAULT_REG_INFO;
        mbus->evFunc = _SEND_ADDRESS;
        mbus->errmsg = MB_ERROR_SLVFAILURE;
        break;
    }

    if (MB_NO_ERROR != mbus->errmsg)
        return MBUS_FAIL;

    return MBUS_WAIT;
}

int16_t writeHoldingNRegister(SCI_MODBUS *mbus)
{
    HEADER_WRITE_N_HOLDINGREG *pack = (HEADER_WRITE_N_HOLDINGREG *)mbus->pHeader;
    uint16_t u16Word = 0;

    switch (mbus->evFunc)
    {
    case _SEND_ADDRESS:

        mbus->info.rwfrom = (pack->Address.HiByte << 8) | pack->Address.LoByte;

        mbus->errmsg = (MSG_ERROR_ID)chkValidAddress(mbus->info.rwfrom);
        if (MB_NO_ERROR == mbus->errmsg)
        {
            // Rigorous check: validate Bytes field equals Points * 2
            mbus->info.words = (((pack->Points.HiByte << 8) & 0xFF00) | pack->Points.LoByte);
            if (pack->Bytes != mbus->info.words * 2)
            {
                mbus->errmsg = MB_ERROR_ILLEGALDATA; // Data error
                return MBUS_FAIL;
            }
            // Check quantity range: ensure words is within Modbus limits
            if (mbus->info.words > MAX_MODBUS_REGISTERS || mbus->info.words == 0)
            {
                mbus->errmsg = MB_ERROR_ILLEGALDATA; // Quantity out of range
                return MBUS_FAIL;
            }
            // Check address range: ensure rwfrom + words - 1 does not exceed valid range
            if (chkValidAddress(mbus->info.rwfrom + mbus->info.words - 1) != MB_NO_ERROR)
            {
                mbus->errmsg = MB_ERROR_ILLEGALDATA; // Address range error
                return MBUS_FAIL;
            }
            pushModbusByte(pack->Slave, mbus);
            pushModbusByte(pack->Function, mbus);
            mbus->evFunc = _SEND_MEMADDR;
            //The number of bytes convert to the number of words.
            mbus->info.pData = &pack->Data;
        }
        else
        {
            return MBUS_FAIL;
        }

    case _SEND_MEMADDR:
        pushModbusByte(pack->Address.HiByte, mbus);
        pushModbusByte(pack->Address.LoByte, mbus);
        mbus->evFunc = _SEND_MEMSIZE;
        // Fall through to _SEND_MEMSIZE

    case _SEND_MEMSIZE:
        pushModbusByte(pack->Points.HiByte, mbus);
        mbus->info.crc = pushModbusByte(pack->Points.LoByte, mbus);
        mbus->evFunc = _SAVE_BYTE_COUNTS;
        // Fall through to _SAVE_BYTE_COUNTS

    case _SAVE_BYTE_COUNTS:
        if (mbus->info.index < mbus->info.words)
        {
            u16Word = (uint16_t)((mbus->info.pData[mbus->info.index].HiByte << 8) & 0xFF00) | (mbus->info.pData[mbus->info.index].LoByte & 0x00FF);
            setModbusData(mbus->info.rwfrom, u16Word);
            mbus->info.index++;
            mbus->info.rwfrom++;
        }
        else
        {
            pushModbusByte(LBYTE(mbus->info.crc), mbus);
            pushModbusByte(HBYTE(mbus->info.crc), mbus);
            mbus->evFunc = _SEND_ADDRESS;
            mbus->setRegN((void *)mbus);
            return MBUS_SUCCESS;
        }
        break;

    default:
        mbus->info = DEFAULT_REG_INFO;
        mbus->evFunc = _SEND_ADDRESS;
        mbus->errmsg = MB_ERROR_SLVFAILURE;
        break;
    }

    if (MB_NO_ERROR != mbus->errmsg)
        return MBUS_FAIL;

    return MBUS_WAIT;
}

int16_t (*ModbusFunc[MB_END_OF_FUNCID])(SCI_MODBUS *mbus) = {
    ReservedFunction,      // 0x00
    ReservedFunction,      // 0x01
    ReservedFunction,      // 0x02
    readHoldingRegister,   // 0x03		Read Holding Registers
    ReservedFunction,      // 0x04
    ReservedFunction,      // 0x05
    writeHoldingRegister,  // 0x06		Write Single Register
    ReservedFunction,      // 0x07
    ReservedFunction,      // 0x08
    ReservedFunction,      // 0x09
    ReservedFunction,      // 0x0A
    ReservedFunction,      // 0x0B
    ReservedFunction,      // 0x0C
    ReservedFunction,      // 0x0D
    ReservedFunction,      // 0x0E
    ReservedFunction,      // 0x0F
    writeHoldingNRegister, // 0x10		Write Multiple Register
    ReservedFunction,      // 0x11
    ReservedFunction,      // 0x12
    ReservedFunction,      // 0x13
    ReservedFunction,      // 0x14
    ReservedFunction,      // 0x15
    ReservedFunction,      // 0x16
    ReservedFunction,      // 0x17
    ReservedFunction       // 0x18

};

int16_t ModbusCommError(SCI_MODBUS *mbus)
{
    MODBUS_HEADER *pack = (MODBUS_HEADER *)mbus->pHeader;
    uint16_t u16ErrFunc = 0;

    if (MB_NO_ERROR == mbus->errmsg)
        return MB_NO_ERROR;

    mbus->info.tbcrc = DEFAULT_CRC;
    mbus->info.crc = 0;
    pushModbusByte(pack->Slave, mbus);

    u16ErrFunc = pack->Function | MB_ERROR_MASK;
    pushModbusByte(u16ErrFunc, mbus);

    mbus->errorlog[mbus->amount_of_error & (SIZE_OF_ERRORLOG - 1)] = mbus->errmsg;
    mbus->amount_of_error++;
    mbus->info.crc = pushModbusByte(mbus->errmsg, mbus);

    pushModbusByte(mbus->info.crc & 0x00FF, mbus);
    pushModbusByte((mbus->info.crc >> 8) & 0x00FF, mbus);
    mbus->errmsg = MB_NO_ERROR;
    mbus->evError = _SEND_ADDRESS;

    return MBUS_SEND_ERROR_MSG_BACK;
}

void initRegN(void *p);
void readRegN(void *p);
void writeReg(void *p);
void writeRegN(void *p);


/** @brief mbcomm variables definition
 *         Note: If there are some initial variables, CAN NOT place in GSRAM.*/
volatile SCI_MODBUS mbcomm = {
        .evstep = _INIT_MODBUS_INFO,
		.pHeader = (MODBUS_HEADER *)&mbcomm.sFiFo.u16RAM[0],
		//Configure Serial Port
		.sci = (uint32_t)VSCI_base,
		.getRsize = (int16_t (*)(uint32_t) )VSCI_getRxFIFOStatus,
	    .rdfunc = VSCI_readCharNonBlocking,
	    .getWsize = (int16_t (*)(uint32_t)) VSCI_getTxFIFOStatus,
	    .wrfunc = VSCI_writeCharNonBlocking,
	    //Configure Modbus
		.slaveid = 0x06,
		.pReg = &regMbusData,
		.initReg = initRegN,
		.getReg = readRegN,
		.setReg = writeReg,
		.setRegN = writeRegN,
		.u16Debug = 0xFFFF

};
