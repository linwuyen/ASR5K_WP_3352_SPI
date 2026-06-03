/*
 * cmd_parser.h
 *
 *  Created on: Jul 16, 2024
 *      Author: User
 */

#ifndef DRIVERLIB_WR_PARSER_H_
#define DRIVERLIB_WR_PARSER_H_

#include <stdint.h>
#include "cmd_id.h"
#include "shareram.h"  // for sAccessCPU1 (startupFlags)

// Define the FW version code if not provided elsewhere
#ifndef DSP_FW_Version_Code

#define DSP_FW_Version_Code_CPU1     0x8A90
#define DSP_FW_Version_Code_CPU2     0xBBAA

#define Output_ON_OFF_spi_addr  0x0900

#define startupFlags  0x5555

#endif

typedef volatile union
{
    uint32_t u32All;
    struct
    {
        uint32_t data :16;
        uint32_t id :8;
        uint32_t group :8;
    } bits;
    struct
    {
        uint16_t u16Data;
        uint16_t u16Address;
    };
} U32_PACK;

typedef U32_PACK *HAL_U32PACK;

typedef volatile struct
{
    int16_t (*wrfunc)(HAL_U32PACK p);
    int16_t (*rdfunc)(HAL_U32PACK p);
    HAL_U32PACK pRdata;
} ST_WR_PARSER;

typedef ST_WR_PARSER *HAL_WR_PARSER;

#define DEFAULT_WR_PARSER (ST_WR_PARSER){ \
                            .wrfunc = 0, \
                            .rdfunc = 0, \
                            .pRdata = 0 }

static inline void pushDataIntoTxD(uint16_t u16Data, HAL_WR_PARSER hal)
{
    if (0 == hal->wrfunc)
        return;
    U32_PACK u32Temp;
    u32Temp.u16Data = u16Data;
    u32Temp.u16Address = hal->pRdata->u16Address + (u16Data & 0xFF)+ ((u16Data >> 8) & 0xFF);
    hal->wrfunc(&u32Temp);
}

static inline void pushNullIntoTxD(HAL_WR_PARSER hal)
{
    if (0 == hal->wrfunc)
        return;
    U32_PACK u32Temp = { .u32All = 0xFFFF0000 };
    hal->wrfunc(&u32Temp);
}


extern volatile uint16_t OUTPUT_ON;
// -----------------------------------------------------------------------------
// [Agent Optimized]: STUB implementation for rapid porting without errors
// -----------------------------------------------------------------------------
static inline void parseGroup0x04(HAL_WR_PARSER hal)
{
    uint16_t u16Data = hal->pRdata->u16Data;

    switch(hal->pRdata->u16Address) {
 
     case C2000_Version_spi_addr:  // Read CPU1 Version
            pushDataIntoTxD(DSP_FW_Version_Code_CPU1, hal);
            break;
 
     case CPU2_Version_spi_addr:   // Read CPU2 Version
            pushDataIntoTxD(DSP_FW_Version_Code_CPU2, hal);
            break;

     case Startup_State_spi_addr:
//            pushDataIntoTxD(sAccessCPU1.startupFlags.ALL, hal);  // STARTUP_FLAGS_DEFAULT = 0x5555 (all PASS)
            pushDataIntoTxD(startupFlags, hal);
            break;
     default:
         pushDataIntoTxD(u16Data, hal);
         break;
     }
}

static inline void parseGroup0x07(HAL_WR_PARSER hal)
{
    pushNullIntoTxD(hal);
}
static inline void parseGroup0x08(HAL_WR_PARSER hal)
{
    pushNullIntoTxD(hal);
}

static inline void parseGroup0x09And0x0A(HAL_WR_PARSER hal)
{
    uint16_t u16Addr = hal->pRdata->u16Address;
    uint16_t u16Data = hal->pRdata->u16Data;

    switch (u16Addr)
    {
    case Output_ON_OFF_spi_addr:
        OUTPUT_ON = u16Data & 0x01;
        pushDataIntoTxD(OUTPUT_ON & 0x01, hal);
        break;

    default:
        pushNullIntoTxD(hal);
        break;
    }
}

static inline void parseGroup0x10_0x1F(HAL_WR_PARSER hal)
{
    pushNullIntoTxD(hal);
}
static inline void parseGroup0x20_0x2F(HAL_WR_PARSER hal)
{
    pushNullIntoTxD(hal);
}
static inline void parseGroup0x30_0x3F(HAL_WR_PARSER hal)
{
    pushNullIntoTxD(hal);
}

static inline void parseRemoteCommand(HAL_WR_PARSER hal)
{
    uint16_t group = (hal->pRdata->u16Address >> 8) & 0xFF;

    switch (group)
    {
    case 0x04:
        parseGroup0x04(hal);
        break;
    case 0x07:
        parseGroup0x07(hal);
        break;
    case 0x08:
        parseGroup0x08(hal);
        break;
    case 0x09:
    case 0x0A:
        parseGroup0x09And0x0A(hal);
        break;
    default:
        pushNullIntoTxD(hal);
        break;
    }
}

#ifdef SPI_ENABLE_U64_TEST
/* -----------------------------------------------------------------------
 * [64-bit Experimental] Direct Reception and Processing (No Queue Pollution)
 * ----------------------------------------------------------------------- */

/* Echo back exactly 4 x 16-bit words back to master via TX FIFO */
#endif /* SPI_ENABLE_U64_TEST */

#endif
