/*
 * ModbusCommon.h
 *
 *  Created on: 2018¦~7¤ë9¤é
 *      Author: user
 */

#ifndef MODBUS_MODBUSCOMMON_H_
#define MODBUS_MODBUSCOMMON_H_

#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "common.h"
#include "CRC16.h"

typedef enum {
	MBUS_FREE                         =  (0x0001<<2),
    MBUS_RESET                        =  (0x0001<<1),
	MBUS_SUCCESS                      =	 (0x0001<<0),
	MBUS_WAIT	                      =	  0,
	MBUS_FAIL	                      = ~(0x0001<<0),
	MBUS_SEND_ERROR_MSG_BACK          = ~(0x0001<<1),

} MODBUS_STATUS;

// Error codes
typedef enum {
	MB_NO_ERROR = 0,
	MB_ERROR_ILLEGALFUNC = 1,
	MB_ERROR_ILLEGALADDR,
	MB_ERROR_ILLEGALDATA,
	MB_ERROR_SLVFAILURE,
	MB_ERROR_ACKNOWLEDGE,
	MB_ERROR_SLVBUSY,
	MB_ERROR_NEGACKNOWLEDGE,
	MB_ERROR_MEMPARITY,
	MB_ERROR_GATEWAYPATHUN,
	MSG_ERROR_TIMEOUT = 9000
} MSG_ERROR_ID;

typedef enum {
	MB_READ_COIL_STATUS = 0x01,
	MB_READ_INPUT_STATUS = 0x02,
	MB_READ_HOLDING_REGISTERS = 0x03,
	MB_READ_INTPUT_REGISTERS = 0x04,
	MB_FORCE_SINGLE_COIL = 0x05,
	MB_PRESET_SINGLE_REGISTER = 0x06,
	MB_READ_EXCEPTION_STATUS = 0x07,
	MB_DIAGNOSTICS = 0x08,
	MB_PROGRAM_484 = 0x09,
	MB_POLL_484 = 0x0A,
	MB_FETCH_COMM_EVENT_CTR = 0x0B,
	MB_FETCH_COMM_EVENT_LOG = 0x0C,
	MB_PROGRAM_CONTROLLER = 0x0D,
	MB_POLL_CONTROLLER = 0x0E,
	MB_FORCE_MULTIPLE_COILS = 0x0F,
	MB_PRESET_MULTIPLE_REGISTERS = 0x10,
	MB_REPORT_SLAVE_ID = 0x11,
	MB_PROGRAM_884_M84 = 0x12,
	MB_RESET_COMM_LINK = 0x13,
	MB_READ_GENERAL_REFERENCE = 0x14,
	MB_WRITE_GENERAL_REFERENCE = 0x15,
	MB_MASK_WRITE_4X_REGISTER = 0x16,
	MB_READ_WRITE_4X_REGISTERS = 0x17,
	MB_READ_FIFO_QUEUE = 0x18,
	MB_END_OF_FUNCID,
	MB_ERROR_MASK = 0X80
} MB_FUNC_ID;

#define END_OF_FUNCTION			(MB_END_OF_FUNCID-1)

#define BROADCAST_ID            0

#define mbT35 (unsigned long) 5000  //1750
#define mbT15 (unsigned long) 750


#define MB_Q15toSF(q15)   ((float32_t)((int16_t)(q15)) * 0.000030517578125f)

#define MB_SFtoQ15(sf)  ((int16_t)( \
    ((sf) >= 0.999969482421875f) ?  32767.0f : \
    ((sf) <= -1.0f)           ? -32768.0f : \
    ((sf) *  32768.0f + ((sf) >= 0.0f ? 0.5f : -0.5f)) \
))						

#endif /* MODBUS_MODBUSCOMMON_H_ */
