//===========================================================================
// TI F2838x Linker Command File for CPU1 FLASH Configuration
//===========================================================================
// The user must define CLA_C in the project linker settings if using the
// CLA C compiler
// Project Properties -> C2000 Linker -> Advanced Options -> Command File
// Preprocessing -> --define
#ifdef CLA_C
// Define a size for the CLA scratchpad area that will be used
// by the CLA compiler for local symbols and temps
// Also force references to the special symbols that mark the
// scratchpad area.
CLA_SCRATCHPAD_SIZE = 0x100;
--undef_sym=__cla_scratchpad_end
--undef_sym=__cla_scratchpad_start
#endif //CLA_C

MEMORY
{
    PAGE 0 : /* Program Memory */
             /* Memory (RAM/FLASH) blocks can be moved to PAGE1 for data allocation */
             /* BEGIN is used for the "boot to Flash" bootloader mode */

        /* Boot and Reset Vectors */
        BEGIN               : origin = 0x080000, length = 0x000002
        RESET               : origin = 0x3FFFC0, length = 0x000002

        /* RAM Memory Blocks */
        RAMM0               : origin = 0x0001B1, length = 0x00024F
        RAMD0               : origin = 0x00C000, length = 0x000800
        RAMLS01             : origin = 0x008000, length = 0x001000    /* CPU1 Ram Function */
        RAMLS45             : origin = 0x00A000, length = 0x001000    /* CLA Ram Function */
        RAMGS0              : origin = 0x00D000, length = 0x001000
        RAMGS1              : origin = 0x00E000, length = 0x001000
        RAMGS14             : origin = 0x01B000, length = 0x001000    /* .fapi_ram for CPU1 */
        RAMGS15             : origin = 0x01C000, length = 0x000FF8

        /* Flash Memory Blocks */
        FLASH0              : origin = 0x080002, length = 0x001FFE    /* .cinit .pinit */
        FLASH1              : origin = 0x082000, length = 0x002000    /* .text */
        FLASH2              : origin = 0x084000, length = 0x002000    /* .text */
        FLASH3              : origin = 0x086000, length = 0x002000    /* .text */
        FLASH4              : origin = 0x088000, length = 0x008000    /* .text */
        FLASH5              : origin = 0x090000, length = 0x008000    /* .TI.ramfunc */
        FLASH6              : origin = 0x098000, length = 0x008000    /* .switch */
        FLASH7              : origin = 0x0A0000, length = 0x008000    /* .init_array */
        FLASH8              : origin = 0x0A8000, length = 0x008000    /* .const */
        FLASH9              : origin = 0x0B0000, length = 0x008000    /* Cla1Prog */
        FLASH10             : origin = 0x0B8000, length = 0x002000
        FLASH11             : origin = 0x0BA000, length = 0x002000    /* .fapi_ram */
        FLASH12             : origin = 0x0BC000, length = 0x002000    /* C28_STORE_ADDRESS */
        FLASH13             : origin = 0x0BE000, length = 0x001FF0    /* C28_STORE_ADDRESS_PART2 */

    PAGE 1 : /* Data Memory */
             /* Memory (RAM/FLASH) blocks can be moved to PAGE0 for program allocation */

        /* Boot Reserved Memory */
        BOOT_RSVD           : origin = 0x000002, length = 0x0001AF    /* Part of M0, BOOT rom will use this for stack */

        /* RAM Memory Blocks */
        RAMM1               : origin = 0x000400, length = 0x0003F8    /* on-chip RAM block M1 */
        RAMD1               : origin = 0x00C800, length = 0x000800
        RAMLS23             : origin = 0x009000, length = 0x001000    /* CPU1 Ram Data */
        RAMLS67             : origin = 0x00A800, length = 0x001000    /* CLA Ram Data */

        /* Global Shared RAM */
        RAMGS2              : origin = 0x00F000, length = 0x001000    /* CPU1 .comm_interface, BBOX table */
        RAMGS3              : origin = 0x010000, length = 0x001000    /* .bss:output, .bss:cio, .sysmem */
        RAMGS4              : origin = 0x011000, length = 0x001000    /* DDS Table (4096 points) */
        RAMGS10_11          : origin = 0x017000, length = 0x002000    /* RD_CPU2 */
        RAMGS12_13          : origin = 0x019000, length = 0x002000    /* RW_CPU1 */



        /* Inter-Processor Communication RAM */
        CPU1TOCPU2RAM       : origin = 0x03A000, length = 0x000800
        CPU2TOCPU1RAM       : origin = 0x03B000, length = 0x000800
        CPUTOCMRAM          : origin = 0x039000, length = 0x000800
        CMTOCPURAM          : origin = 0x038000, length = 0x000800

        /* CAN Message RAM */
        CANA_MSG_RAM        : origin = 0x049000, length = 0x000800
        CANB_MSG_RAM        : origin = 0x04B000, length = 0x000800
}

SECTIONS
{
    //===========================================================================
    // Program Memory Sections
    //===========================================================================
    
    /* Boot and Initialization */
    codestart           : > BEGIN,      PAGE = 0, ALIGN(8)
    .cinit              : > FLASH0,     PAGE = 0, ALIGN(8)
    .pinit              : > FLASH0,     PAGE = 0, ALIGN(8)
    .reset              : > RESET,      PAGE = 0, TYPE = DSECT /* not used */
    
    /* Program Code */
    .text               : >> FLASH1 | FLASH2 | FLASH3 | FLASH4, PAGE = 0, ALIGN(8)
    .switch             : > FLASH6,     PAGE = 0, ALIGN(8)
    .init_array         : > FLASH7,     PAGE = 0, ALIGN(8)
    
    /* Constants */
    .const              : > FLASH8,     PAGE = 0, ALIGN(8)

    //===========================================================================
    // Data Memory Sections
    //===========================================================================
    
    /* Stack and System Memory */
    .stack              : > RAMM1,      PAGE = 1
    .sysmem             : > RAMGS3,     PAGE = 1
    
    /* Uninitialized Data */
    .bss                : > RAMLS23,    PAGE = 1
    .bss:output         : > RAMGS3,     PAGE = 1
    .bss:cio            : > RAMGS3,     PAGE = 1
    
    /* Initialized Data */
    .data               : > RAMLS23,    PAGE = 1

    //===========================================================================
    // Special Purpose Sections
    //===========================================================================
    
    /* General Purpose RAM Sections */
    ramgs0              : > RAMGS0,     type=NOINIT
    ramgs1              : > RAMGS1,     type=NOINIT
    
    /* Inter-Processor Communication */
    MSGRAM_CPU1_TO_CPU2 : > CPU1TOCPU2RAM, type=NOINIT
    MSGRAM_CPU2_TO_CPU1 : > CPU2TOCPU1RAM, type=NOINIT
    MSGRAM_CPU_TO_CM    : > CPUTOCMRAM,    type=NOINIT
    MSGRAM_CM_TO_CPU    : > CMTOCPURAM,    type=NOINIT
    
    /* Application Specific Sections */
    RD_CPU2             : > RAMGS10_11, PAGE = 1
    RW_CPU1             : > RAMGS12_13, PAGE = 1
    .comm_interface     : > RAMGS2,     PAGE = 1
    .bbox               : > RAMGS2,     PAGE = 1
    
    /* DDS High-Precision Table Section (4096 points = 8KB, perfect fit for RAMGS4) */
    .dds_table : {
        *(.dds_sine_table)
    } > RAMGS4, PAGE = 1, type=NOINIT, ALIGN(8)

    //===========================================================================
    // RAM Function Section (Copy from Flash to RAM at runtime)
    //===========================================================================
    
    .TI.ramfunc : {
    } LOAD = FLASH5,
      RUN = RAMLS01,
      LOAD_START(RamfuncsLoadStart),
      LOAD_SIZE(RamfuncsLoadSize),
      LOAD_END(RamfuncsLoadEnd),
      RUN_START(RamfuncsRunStart),
      RUN_SIZE(RamfuncsRunSize),
      RUN_END(RamfuncsRunEnd),
      PAGE = 0, ALIGN(8)

    //===========================================================================
    // Flash API Section (Copy from Flash to RAM at runtime)
    //===========================================================================
    
    .fapi_ram : {
        -l F2838x_C28x_FlashAPI.lib
        *(.fapi_ram)
    } LOAD = FLASH11,
      RUN = RAMGS14,
      LOAD_START(fapi_ram_LoadStart),
      LOAD_SIZE(fapi_ram_LoadSize),
      LOAD_END(fapi_ram_LoadEnd),
      RUN_START(fapi_ram_RunStart),
      RUN_SIZE(fapi_ram_RunSize),
      RUN_END(fapi_ram_RunEnd),
      PAGE = 0, ALIGN(8)

    //===========================================================================
    // CLA (Control Law Accelerator) Sections
    //===========================================================================
    
    /* CLA Program Section */
    Cla1Prog : {
    } LOAD = FLASH9,
      RUN = RAMLS45,
      LOAD_START(Cla1ProgLoadStart),
      RUN_START(Cla1ProgRunStart),
      LOAD_SIZE(Cla1ProgLoadSize),
      PAGE = 0, ALIGN(8)

#ifdef CLA_C
    /* CLA C Compiler Sections */
    /* Must be allocated to memory the CLA has write access to */
    CLAscratch : {
        *.obj(CLAscratch)
        . += CLA_SCRATCHPAD_SIZE;
        *.obj(CLAscratch_end)
    } > RAMLS67, PAGE = 1

    .scratchpad         : > RAMLS67,    PAGE = 1
    .bss_cla            : > RAMLS67,    PAGE = 1
    cla_shared          : > RAMLS67,    PAGE = 1
    
    .const_cla : {
    } LOAD = FLASH9,
      RUN = RAMLS67,
      RUN_START(Cla1ConstRunStart),
      LOAD_START(Cla1ConstLoadStart),
      LOAD_SIZE(Cla1ConstLoadSize),
      PAGE = 1, ALIGN(8)
#endif //CLA_C
}

/*
//===========================================================================
// End of file.
//===========================================================================
*/
