/*
 * shareram.h
 *
 *  Created on: Mar 4, 2024
 *      Author: cody_chen
 */

#ifndef SHARERAM_H_
#define SHARERAM_H_

#define MBUS_SHARERAM_SIZE    256


/* --- Startup Self-Check Flags ---
 * Each 2-bit field: 00=FAIL, 01=PASS, 10=WARN, 11=reserved
 * Default after init: all fields = 01b (0x5555)
 */
#define STARTUP_FLAGS_DEFAULT  ((uint16_t)0x5555U)

typedef struct {
    uint16_t AC_Input_Check:2;    // 0-1
    uint16_t Fan_Check:2;         // 2-3
    uint16_t PFC_Check:2;         // 4-5
    uint16_t DCDC_Check:2;        // 6-7
    uint16_t DCAC_Check:2;        // 8-9
    uint16_t SDRAM_Check:2;       // 10-11
    uint16_t EEPROM_Check:2;      // 12-13
    uint16_t _reserved:2;         // 14-15 (padding to full 16-bit)
} STARTUP_STATE_FLAG_BITS;

typedef union {
    uint16_t                u16;   // SPI read/write (raw 16-bit)
    STARTUP_STATE_FLAG_BITS bits;  // field access
} STARTUP_STATE_FLAG_U16;

typedef volatile struct {
    uint16_t u16RxRAM[MBUS_SHARERAM_SIZE];
    uint16_t u16TxRAM[MBUS_SHARERAM_SIZE];
    uint16_t pushRcnts;
    uint16_t popRcnts;
    uint16_t pushTcnts;
    uint16_t popTcnts;

    uint32_t u32IndexInUse;
    float32_t f32Cpu1Vin;
    float32_t f32Cpu1VinScale;
    float32_t f32Cpu1Iout;
    float32_t f32Cpu1IoutScale;

    /* Task 4.1: M_Ref fields */
    uint16_t u16L1_Ref;
    uint16_t u16L2_Ref;
    uint16_t u16L3_Ref;
    uint16_t u16FuncID;
    uint16_t u16Data_H;
    uint16_t u16Data_L;

    /* Task 4.2: Heartbeats */
    uint32_t u32HeartBeat_CPU1;
    uint32_t u32HeartBeat_CPU2;

    uint32_t u32Fsm;
    uint32_t u32Stat;
    uint32_t u32Warning;
    uint32_t u32Error;

    uint16_t u16LED;
    STARTUP_STATE_FLAG_U16 startupFlags;  // Startup self-check flags (init to 0x5555)
}ST_SHARERAM;

extern ST_SHARERAM sAccessCPU1;
extern ST_SHARERAM sReadCPU2;


#endif /* SHARERAM_H_ */
