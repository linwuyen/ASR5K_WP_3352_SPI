/*
 * cdebug.c
 *
 *  Created on: Jul 26, 2024
 *      Author: cody_chen
 */

#include "common.h"
#include "shareram.h"
#include "mb_slave/ModbusSlave.h"


typedef enum {
    _INIT_DEBUG = (0x00000001<<0),
    _RUN_MODBUS = (0x00000001<<1),
    _INIT_DEBUGIO = (0x00000001<<2),
    _RUN_DEBUGIO = (0x00000001<<3),

    _MARK_ERROR_OF_DEBUG = 0x80000000
}FSM_DEBUG;

typedef struct {

    FSM_DEBUG u32Fsm;
    uint32_t timetick;
    uint32_t timestamp;
    uint32_t timeout;
    uint32_t trycnt;
}ST_DEBUG;

ST_DEBUG sDebug = {
    .u32Fsm = _INIT_DEBUG,
};

void initSciGpio(void)
{
    GPIO_setControllerCore(DEBUG_SCI_SCIRX_GPIO, GPIO_CORE_CPU1);
    GPIO_setControllerCore(DEBUG_SCI_SCITX_GPIO, GPIO_CORE_CPU1);

    GPIO_setPinConfig(DEBUG_SCI_SCIRX_PIN_CONFIG);
    GPIO_setPadConfig(DEBUG_SCI_SCIRX_GPIO, GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(DEBUG_SCI_SCIRX_GPIO, GPIO_QUAL_ASYNC);

    GPIO_setPinConfig(DEBUG_SCI_SCITX_PIN_CONFIG);
    GPIO_setPadConfig(DEBUG_SCI_SCITX_GPIO, GPIO_PIN_TYPE_STD | GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(DEBUG_SCI_SCITX_GPIO, GPIO_QUAL_ASYNC);
}

void initSciModule(void){
    SCI_clearInterruptStatus(DEBUG_SCI_BASE, SCI_INT_RXFF | SCI_INT_TXFF | SCI_INT_FE | SCI_INT_OE | SCI_INT_PE | SCI_INT_RXERR | SCI_INT_RXRDY_BRKDT | SCI_INT_TXRDY);
    SCI_clearOverflowStatus(DEBUG_SCI_BASE);
    SCI_resetTxFIFO(DEBUG_SCI_BASE);
    SCI_resetRxFIFO(DEBUG_SCI_BASE);
    SCI_resetChannels(DEBUG_SCI_BASE);
    SCI_setConfig(DEBUG_SCI_BASE, DEVICE_LSPCLK_FREQ, DEBUG_SCI_BAUDRATE, (SCI_CONFIG_WLEN_8|SCI_CONFIG_STOP_ONE|SCI_CONFIG_PAR_EVEN));
    SCI_disableLoopback(DEBUG_SCI_BASE);
    SCI_performSoftwareReset(DEBUG_SCI_BASE);
    SCI_enableFIFO(DEBUG_SCI_BASE);
    SCI_enableModule(DEBUG_SCI_BASE);
}

void resetSciModule(void){
    SCI_disableModule(DEBUG_SCI_BASE);
    GPIO_setControllerCore(CPU1_LED852_GPIO135, GPIO_CORE_CPU1);
    GPIO_setControllerCore(CPU2_LED851_GPIO136, GPIO_CORE_CPU2);
    GPIO_setPadConfig(CPU1_LED852_GPIO135, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(CPU2_LED851_GPIO136, GPIO_PIN_TYPE_STD);
}

void initLedGpio(void){

    GPIO_setPinConfig(GPIO_135_GPIO135);
    GPIO_writePin(CPU1_LED852_GPIO135, 0);
    GPIO_setPadConfig(CPU1_LED852_GPIO135, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(CPU1_LED852_GPIO135, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(CPU1_LED852_GPIO135, GPIO_DIR_MODE_OUT);
    GPIO_setControllerCore(CPU1_LED852_GPIO135, GPIO_CORE_CPU1);


    GPIO_setPinConfig(GPIO_136_GPIO136);
    GPIO_writePin(CPU2_LED851_GPIO136, 0);
    GPIO_setPadConfig(CPU2_LED851_GPIO136, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(CPU2_LED851_GPIO136, GPIO_QUAL_SYNC);
    GPIO_setDirectionMode(CPU2_LED851_GPIO136, GPIO_DIR_MODE_OUT);
    GPIO_setControllerCore(CPU2_LED851_GPIO136, GPIO_CORE_CPU2);
}

void runDebug(void)
{
    switch(sDebug.u32Fsm) {
    case _INIT_DEBUG:
        initSciGpio();
        initSciModule();
        sDebug.timetick = sDebug.timestamp = U32_UPCNTS;
        sDebug.timeout = 0;
        sDebug.trycnt = 0;
        sDebug.u32Fsm = _RUN_MODBUS;
        break;
    case _RUN_MODBUS:
#if !(TEST_MODBUS_ONLY == TEST_MODE)
        if(isMbusBusy((SCI_MODBUS *)&mbcomm)) {
            sDebug.timetick = sDebug.timestamp = U32_UPCNTS;
            sDebug.timeout = 0;
            sDebug.trycnt = 0;
        }
        else {
            sDebug.timetick = U32_UPCNTS;
            if(sDebug.timestamp > sDebug.timetick) {
                sDebug.timeout = sDebug.timetick + SW_TIMER - sDebug.timestamp;
            }
            else {
                sDebug.timeout = sDebug.timetick - sDebug.timestamp;
            }

            if(sDebug.timeout >= T_500MS) {
                if(10>sDebug.trycnt) {
                    sDebug.trycnt++;
                    sDebug.timetick = sDebug.timestamp = U32_UPCNTS;
                    sDebug.timeout -= T_500MS;
                }
                else {
                    resetSciModule();
                    sDebug.u32Fsm = _INIT_DEBUGIO;
                }
            }
        }
#endif //TEST_MODBUS_ONLY

        exeModbusSlave((SCI_MODBUS *)&mbcomm);

        break;
    case _INIT_DEBUGIO:
        initLedGpio();
        sDebug.u32Fsm = _RUN_DEBUGIO;
        break;

    case _RUN_DEBUGIO:

#if (TEST_DUAL_CORE_COMM == TEST_MODE)
        sAccessCPU1.u16LED = (1 != sReadCPU2.u16LED);
        GPIO_writePin(CPU1_LED852_GPIO135, sAccessCPU1.u16LED);
#endif //(TEST_DUAL_CORE_COMM == TEST_MODE)

        break;

    default:
        sDebug.u32Fsm |= _MARK_ERROR_OF_DEBUG;
        break;
    }
}

