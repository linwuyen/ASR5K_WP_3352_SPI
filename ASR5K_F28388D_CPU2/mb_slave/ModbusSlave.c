#include "common.h"
#include "ModbusSlave.h"
#include "shareram.h"
#include <string.h>

int16_t cntModbusTimeout(SCI_MODBUS *mbus)
{
    if (0 < mbus->sFiFo.pushRcnts)
    {
        mbus->timetick = SW_TIMER - (uint32_t)CPUTimer_getTimerCount(SWTIRMER_BASE);

        if (mbus->timestamp > mbus->timetick)
        {
            mbus->timeout = mbus->timetick + SW_TIMER - mbus->timestamp;
        }
        else
        {
            mbus->timeout = mbus->timetick - mbus->timestamp;
        }

        if (mbus->timeout >= MBUS_TIMEOUT)
        {
            mbus->timestamp = mbus->timetick;
            return -1;
        }
        return 1;
    }

    return 0;
}

void rstModbusTimeout(SCI_MODBUS *mbus)
{
    mbus->timestamp = mbus->timetick = SW_TIMER - (uint32_t)CPUTimer_getTimerCount(SWTIRMER_BASE);
}

void rstModbusError(SCI_MODBUS *mbus)
{
    memset(mbus->errorlog, 0, sizeof(mbus->errorlog));
    mbus->amount_of_error = 0;
}

int16_t initMbusConfig(SCI_MODBUS *mbus)
{
    rstModbusTimeout(mbus);
    mbus->sFiFo = (ST_FIFO){
        .pushRcnts = 0,
        .popRcnts = 0,
        .pushTcnts = 0,
        .popTcnts = 0,
        .u16RAM[0] = 0xFFFF};

    mbus->crcdata = DEFAULT_CRC;
    mbus->crc = 0x0000;
    mbus->info = DEFAULT_REG_INFO;
    mbus->state = MBUS_FREE;
    mbus->evstep = _RECEIVE_DATA_FROM_FIFO;
    mbus->initReg(mbus);
    mbus->getReg((void *)mbus);

    return 1;
}

int16_t getMbusRxFIFO(SCI_MODBUS *mbus)
{
    if (0 < mbus->getRsize(mbus->sci))
    {
        uint16_t u16Temp = (mbus->rdfunc(mbus->sci) & 0x00FF);


        if (0 == mbus->sFiFo.pushRcnts)
        {
            // If index is first one, then record the slave ID
            mbus->rmtSlaveID = u16Temp;
        }

        if (((mbus->rmtSlaveID == mbus->slaveid) || (BROADCAST_ID == mbus->rmtSlaveID)) && (MBUS_BUFFER_SIZE > mbus->sFiFo.pushRcnts))
        {
            rstModbusTimeout(mbus);

            mbus->sFiFo.u16RAM[mbus->sFiFo.pushRcnts] = u16Temp;
            mbus->crc = ucMBCRC16(u16Temp, &mbus->crcdata);
            mbus->sFiFo.pushRcnts++;
        }
    }

    if ((0 == mbus->crc) && (3 < mbus->sFiFo.pushRcnts))
    {
        return mbus->sFiFo.pushRcnts;
    }
    else
    {
        int16_t s16CheckTimeout = cntModbusTimeout(mbus);
        if (0 > s16CheckTimeout)
        {
            mbus->crcdata = DEFAULT_CRC;
            mbus->crc = 0x0000;
            mbus->sFiFo.pushRcnts = mbus->sFiFo.popRcnts = 0;
        }
        return 0;
    }
}

int16_t isMbusBusy(SCI_MODBUS *mbus)
{
    return (mbus->sFiFo.pushRcnts != mbus->sFiFo.popRcnts);
}

int16_t setMbusTxData(SCI_MODBUS *mbus)
{
    if (mbus->sFiFo.popTcnts < mbus->sFiFo.pushTcnts) {
        mbus->wrfunc(mbus->sci, mbus->sFiFo.u16RAM[mbus->sFiFo.popTcnts]);
        mbus->sFiFo.popTcnts++;
    }
    else {
        mbus->state = MBUS_FREE;
        mbus->info = DEFAULT_REG_INFO;
        mbus->sFiFo.popTcnts = mbus->sFiFo.pushTcnts = 0;
    }
    return (mbus->sFiFo.pushTcnts != mbus->sFiFo.popTcnts);
}

int16_t exeModbusSlave(SCI_MODBUS *mbus)
{
    switch (mbus->evstep)
    {
    case _WAIT_FOR_START_MODBUS:
        return 0;

    case _INIT_MODBUS_INFO:
        initMbusConfig(mbus);
        break;

    case _RECEIVE_DATA_FROM_FIFO:
        if (0 < getMbusRxFIFO(mbus))
        {
            mbus->u16Debug = 0xFFFF;
            
            //_PROCESS_RECEIVING_DATA
            mbus->pHeader = (MODBUS_HEADER *)&mbus->sFiFo.u16RAM[0];
            if (mbus->pHeader->Function < MB_END_OF_FUNCID)
            {
                mbus->state = (MODBUS_STATUS)ModbusFunc[mbus->pHeader->Function](mbus);
            }
            else
            {
                mbus->state = MBUS_FAIL;
            }
            if (MBUS_WAIT != mbus->state)
            {
                mbus->crcdata = DEFAULT_CRC;
                mbus->sFiFo.popRcnts = mbus->sFiFo.pushRcnts = 0;
                ModbusCommError(mbus);
            }
        }
        else
        {
            if(0<mbus->u16Debug) mbus->u16Debug--;
            setMbusTxData(mbus);
        }

#if (TEST_DUAL_CORE_COMM == TEST_MODE)
        if(0 == mbus->u16Debug) {
            sAccessCPU2.u16LED = (1 == sReadCPU1.u16LED);
        }
#endif //TEST_DUAL_CORE_COMM

        break;

    default:
        break;
    }

    return mbus->state;
}
