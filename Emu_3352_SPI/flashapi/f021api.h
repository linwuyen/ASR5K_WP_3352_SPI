/*
 * f021api.h
 *
 *  Created on: Apr 12, 2024
 *      Author: User
 */

#ifndef FLASHAPI_F021API_H_
#define FLASHAPI_F021API_H_



#include "F021_F2838x_C28x.h"

typedef enum {
    _FAPI_NO_ACTION                  = (0x00000000),
    _FAPI_FREE                       = (0x00000001<<0),
    _FAPI_RELEASE_PUMP               = (0x00000001<<1),
    _FAPI_GAIN_PUMP                  = (0x00000001<<2),
    _DELAY_FAPI_PUMP_UP,
    _FAPI_INITIAL                    = (0x00000001<<3),
    _ACITVE_FPAI_BANK_0              = (0x00000001<<4),
    _ERASE_C28_STORE_SECTOR          = (0x00000001<<5),
    _WAIT_FOR_ERASE_SECTOR_READY,
    _PROGRAM_BLOCK_DATA              = (0x00000001<<6),
    _WAIT_FOR_PROGRAM_READY,
    _VARIFY_BLOCK_DATA,
    _WAIT_FOR_VARIFY_READY,
    _MARK_FAPI_ERROR                 = (0x80000000)
} FSM_FAPI;

typedef struct {
    FSM_FAPI fsm;
    FSM_FAPI action;
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t u32Address;
    uint32_t u32DelayCnt;
} ST_FAPI;

#define C28_STORE_ADDRESS        0x000BC000           //Refer to Linker Command File
#define ALIGN_WORD_ADDRESS_BY    8
#define FAPI_DELAY_CNTS          5
#define FLASH_PUMP_FOR_CPUX      FLASH_CPU1_WRAPPER

extern ST_FAPI sFapi;

extern uint16_t fapi_ram_LoadStart;
extern uint16_t fapi_ram_LoadEnd;
extern uint16_t fapi_ram_LoadSize;
extern uint16_t fapi_ram_RunStart;
extern uint16_t fapi_ram_RunEnd;
extern uint16_t fapi_ram_RunSize;


extern void runManualFlashApi(void);


#endif /* FLASHAPI_F021API_H_ */
