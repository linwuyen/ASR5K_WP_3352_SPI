/*
 * initCLA.c
 *
 *  Created on: 2022�~3��29��
 *      Author: cody_chen
 */
#include "initCLA.h"

#pragma SET_DATA_SECTION("cla_shared")

volatile ST_CLA sCLA;

#pragma SET_DATA_SECTION()   // Reset section to default

void initClaCtrlParam(void)
{
    sCLA.u32HeartBeat = 0;

}

//
// CLA Initialization
//
// Description: This function will
// - copy over code and const from flash to CLA program and data ram
//   respectively
// - Initialize the task vectors (MVECTx)
// - setup each task's trigger
// - enable each individual task
// - map program and data spaces to the CLA
// - run any one-time initialization task
// Please note that the CLA can only run code and access data that is in RAM.
// the user must assign constants (tables) to FLASH, and copy them over to
// RAM at run-time. They must be copied to a RAM that lies in the address space
// of the CLA, and prior to giving the CLA control over that space
//
void initCLA(void)
{
    //
    // Copy the program and constants from FLASH to RAM before configuring
    // the CLA
    //
#if defined(_FLASH)
    memcpy((uint32_t *)&Cla1ProgRunStart, (uint32_t *)&Cla1ProgLoadStart,
           (uint32_t)&Cla1ProgLoadSize);
    memcpy((uint32_t *)&Cla1ConstRunStart, (uint32_t *)&Cla1ConstLoadStart,
        (uint32_t)&Cla1ConstLoadSize );
#endif //defined(_FLASH)

    //
    // CLA Program will reside in RAMLS0/1/2 and data in RAMLS3/4/5
    //
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS0, MEMCFG_LSRAMCONTROLLER_CPU_ONLY);
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS1, MEMCFG_LSRAMCONTROLLER_CPU_ONLY);
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS2, MEMCFG_LSRAMCONTROLLER_CPU_ONLY);
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS3, MEMCFG_LSRAMCONTROLLER_CPU_CLA1);
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS4, MEMCFG_LSRAMCONTROLLER_CPU_CLA1);
    MemCfg_setLSRAMControllerSel(MEMCFG_SECT_LS5, MEMCFG_LSRAMCONTROLLER_CPU_CLA1);
    //MemCfg_setCLAMemType(MEMCFG_SECT_LS0, MEMCFG_CLA_MEM_PROGRAM);
    //MemCfg_setCLAMemType(MEMCFG_SECT_LS1, MEMCFG_CLA_MEM_PROGRAM);
    //MemCfg_setCLAMemType(MEMCFG_SECT_LS2, MEMCFG_CLA_MEM_PROGRAM);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS3, MEMCFG_CLA_MEM_PROGRAM);
    MemCfg_setCLAMemType(MEMCFG_SECT_LS4, MEMCFG_CLA_MEM_PROGRAM);
#if defined(_FLASH)
    //
    // In Flash config, constants are loaded in Flash and then copied to LS3/4/5
    // CLA reads the constants from LS3/4/5
    //
    MemCfg_setCLAMemType(MEMCFG_SECT_LS5, MEMCFG_CLA_MEM_DATA);

#endif //defined(_FLASH)

//
// Suppressing #770-D conversion from pointer to smaller integer
// The CLA address range is 16 bits so the addresses passed to the MVECT
// registers will be in the lower 64KW address space. Turn the warning
// back on after the MVECTs are assigned addresses
//
#pragma diag_suppress=770

    //
    // Assign the task vectors and set the triggers for task 1
    //
    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_1, (uint16_t)&Cla1Task1);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_2, (uint16_t)&Cla1Task2);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_3, (uint16_t)&Cla1Task3);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_4, (uint16_t)&Cla1Task4);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_5, (uint16_t)&Cla1Task5);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_6, (uint16_t)&Cla1Task6);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_7, (uint16_t)&Cla1Task7);
//    CLA_mapTaskVector(CLA1_BASE, CLA_MVECT_8, (uint16_t)&Cla1Task8);

    CLA_setTriggerSource(CLA_TASK_1, CLA_TRIGGER_ADCA1);

#pragma diag_warning=770

    initClaCtrlParam();

    //
    // Enable Task 1
    //
    CLA_enableTasks(CLA1_BASE, CLA_TASKFLAG_1);
}
