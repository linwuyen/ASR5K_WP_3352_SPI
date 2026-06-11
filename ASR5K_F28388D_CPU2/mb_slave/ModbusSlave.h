/*
 * ModbusSlave.h
 *
 *  Created on: 2018¦~3¤ë26¤é
 *      Author: user
 */

#ifndef MODBUSSLAVE_H_
#define MODBUSSLAVE_H_

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memcfg.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"

#include "ModbusCommon.h"
#include "mbcmd.h"

typedef enum {
	_SEND_ADDRESS = 0,
	_SEND_MEMADDR,
	_SEND_MEMSIZE,
	_SAVE_BYTE_COUNTS,
	_SEND_BYTE_COUNTS,
	_SEND_DATA_OUT,
	_SEND_ERROR_FUNCTION,
	_SEND_ERROR_ID,
	_SEND_CRC
} MB_EVENT_ID;

#define MBUS_TIMEOUT                    T_10MS

#define MBUS_BUFFER_SIZE		        128

typedef struct {
	uint16_t HiByte;
	uint16_t LoByte;
} U16REG;

typedef struct {
	uint16_t Slave;
	uint16_t Function;
	U16REG Address;
} MODBUS_HEADER;

typedef struct {
	uint16_t Slave;
	uint16_t Function;
	U16REG Address;
	U16REG Points;
} HEADER_READ_HOLDINGREG;


typedef struct {
	uint16_t Slave;
	uint16_t Function;
	U16REG Address;
	U16REG Data;
} HEADER_WRITE_HOLDINGREG;

typedef struct {
	uint16_t Slave;
	uint16_t Function;
	U16REG Address;
	U16REG Points;
	uint16_t Bytes;
	U16REG Data;
} HEADER_WRITE_N_HOLDINGREG;

typedef struct {
	ST_CRC tbcrc;
	uint16_t crc;
	uint16_t words;
	uint16_t index;
	uint16_t addr;
	U16REG *pData;
	int16_t group;
	int16_t memrange;
	uint16_t rwfrom;
} REG_FUNC_INFO;

#define DEFAULT_REG_INFO (REG_FUNC_INFO) { \
							.tbcrc = DEFAULT_CRC, \
							.crc = 0, \
							.words = 0, \
							.index = 0, \
							.addr = 0, \
							.pData = 0, \
							.group = 0, \
							.memrange = 0, \
							.rwfrom = 0 }

typedef enum{
    _WAIT_FOR_START_MODBUS = 0,
	_INIT_MODBUS_INFO,
	_RECEIVE_DATA_FROM_FIFO,
	_RECEIVING_TIMEOUT,
	_PROCESS_RECEIVING_DATA
} EV_MODBUS;

typedef enum{
    _INIT_SCI_CONFIG = 0,
    _RECEIVE_DATA_FROM_SCI
} EV_PORT;

typedef struct {
    uint16_t pushRcnts;
    uint16_t popRcnts;
    uint16_t pushTcnts;
    uint16_t popTcnts;
    uint16_t u16RAM[MBUS_BUFFER_SIZE];
}ST_FIFO;


typedef enum {
    T_S16 = 0,
    T_U16,
    T_S32,
    T_U32,
    T_F32,
    T_D64,
    T_STR,
    T_FUNC,
    UNDEFINE_CMD_TPYE
} TYP_CMD;


typedef struct {
   uint16_t id;         //Id Range: 0 ~ 0xFFFF
   TYP_CMD foramt;
   uint16_t mem_i;      //Memory Index
   uint16_t mem_size;   //Memory Size
} ST_CMD;

typedef struct {
    ST_CMD * src;
    uint16_t d_size;
}ST_GROUP;


#define SIZE_OF_ERRORLOG 	16
typedef struct _SCI_MODBUS {
	int16_t errno;
	ST_CRC crcdata;
	uint16_t crc;
	EV_MODBUS evstep;	//Event Step;
	EV_PORT evPort;
	MODBUS_STATUS state;
	uint16_t slaveid;
    ST_FIFO sFiFo;

	MODBUS_HEADER *pHeader;
	MSG_ERROR_ID errmsg;

	REG_FUNC_INFO info;

	MB_EVENT_ID evFunc;
	MB_EVENT_ID evError;

	MSG_ERROR_ID errorlog[SIZE_OF_ERRORLOG];
	int16_t amount_of_error;

	REG_MBUSDATA *pReg;

	void (*initReg)(void *p);
	void (*getReg)(void *p);
	void (*setReg)(void *p);
	void (*setRegN)(void *p);

	uint32_t timetick;
	uint32_t timestamp;
	uint32_t timecnt;
	uint32_t timeout;

	uint16_t rmtSlaveID;

	uint32_t sci;
	int16_t (*getRsize)(uint32_t base);
	uint16_t (*rdfunc)(uint32_t base);
	int16_t (*getWsize)(uint32_t base);
	void (*wrfunc)(uint32_t base, uint16_t data);

	uint16_t u16Debug;

} SCI_MODBUS;
static inline int16_t pushModbusByte(uint16_t u16Data, SCI_MODBUS *mbus)
{
	mbus->sFiFo.u16RAM[mbus->sFiFo.pushTcnts] = u16Data;
	mbus->sFiFo.pushTcnts++;
	return ucMBCRC16(u16Data, &mbus->info.tbcrc);
}

extern int16_t (*ModbusFunc[MB_END_OF_FUNCID])(SCI_MODBUS *mbus);
extern int16_t ModbusCommError(SCI_MODBUS *mbus);

extern int16_t exeModbusSlave(SCI_MODBUS *ref);
extern int16_t isrMbusRxData(SCI_MODBUS *mbus);

extern int16_t isMbusBusy(SCI_MODBUS *mbus);

extern void updateModbusParameter(SCI_MODBUS *p);

extern volatile SCI_MODBUS mbcomm;


#endif /* MODBUSSLAVE_H_ */
