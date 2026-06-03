/*
 * f021api.c
 *
 *  Created on: Apr 12, 2024
 *      Author: Cody
 */
#include "common.h"



ST_FAPI sFapi = {
    .fsm =  _FAPI_FREE,
    .action = _PROGRAM_BLOCK_DATA,
    .u32Address = C28_STORE_ADDRESS,
    .u32DelayCnt = FAPI_DELAY_CNTS
};


uint16 au16DataBuffer[8] = {0x0001, 0x0203, 0x0405, 0x0607, 0x0809, 0x0A0B, 0x0C0D, 0x0E0F};
uint32 *DataBuffer32 = (uint32 *)au16DataBuffer;

#ifdef _FLASH
#pragma SET_CODE_SECTION(".fapi_ram")
#endif //_FLASH

void runManualFlashApi(void)
{
    EALLOW;
    switch(sFapi.fsm){
    case _FAPI_NO_ACTION:
        sFapi.fsm = _FAPI_FREE;
        break;

    case _FAPI_RELEASE_PUMP:
        Flash_releasePumpSemaphore();
        sFapi.fsm = _FAPI_FREE;
        break;

    case _FAPI_GAIN_PUMP:
//        Flash_claimPumpSemaphore(FLASHPUMPSEMAPHORE_BASE);
        sFapi.u32DelayCnt = FAPI_DELAY_CNTS;
        sFapi.fsm = _DELAY_FAPI_PUMP_UP;
        break;

    case _DELAY_FAPI_PUMP_UP:
        if(0<sFapi.u32DelayCnt) {
            sFapi.u32DelayCnt--;
        }
        else {
            sFapi.fsm = _FAPI_INITIAL;
        }
        break;

    case _FAPI_INITIAL:
        sFapi.oReturnCheck = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, CPUCLK_FREQUENCY);
        if(Fapi_Status_Success == sFapi.oReturnCheck) {
            sFapi.fsm = _ACITVE_FPAI_BANK_0;
        }
        break;

    case _ACITVE_FPAI_BANK_0:
        sFapi.oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);
        if(Fapi_Status_Success == sFapi.oReturnCheck) {
            sFapi.fsm = sFapi.action;
        }
        break;

    case _ERASE_C28_STORE_SECTOR:
        sFapi.oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32 *)C28_STORE_ADDRESS);
        sFapi.fsm = _WAIT_FOR_ERASE_SECTOR_READY;
    case _WAIT_FOR_ERASE_SECTOR_READY:
        sFapi.oReturnCheck = Fapi_checkFsmForReady();
        if(Fapi_Status_FsmReady == sFapi.oReturnCheck) {
            sFapi.u32Address = C28_STORE_ADDRESS;
            sFapi.action = _PROGRAM_BLOCK_DATA;
            sFapi.fsm = _FAPI_RELEASE_PUMP;
        }
        break;

    case _PROGRAM_BLOCK_DATA:
        sFapi.oReturnCheck = Fapi_issueProgrammingCommand((uint32 *)sFapi.u32Address, au16DataBuffer, ALIGN_WORD_ADDRESS_BY,
        0, 0, Fapi_AutoEccGeneration);
        sFapi.fsm = _WAIT_FOR_PROGRAM_READY;
    case _WAIT_FOR_PROGRAM_READY:
        sFapi.oReturnCheck = Fapi_checkFsmForReady();
        if(Fapi_Status_FsmReady == sFapi.oReturnCheck) {
            sFapi.fsm = _VARIFY_BLOCK_DATA;
        }
        break;

    case _VARIFY_BLOCK_DATA:
        sFapi.oReturnCheck = Fapi_doVerify((uint32 *)sFapi.u32Address, 4, (uint32*)au16DataBuffer, &sFapi.oFlashStatusWord);
        if(Fapi_Status_Success == sFapi.oReturnCheck) {
            sFapi.u32Address += ALIGN_WORD_ADDRESS_BY;
            sFapi.fsm = _FAPI_RELEASE_PUMP;
        }
        break;

    default:
        break;
    }
    EDIS;
}



#ifdef _FLASH
#pragma SET_CODE_SECTION()
#endif //_FLASH
//
// End of File
//
