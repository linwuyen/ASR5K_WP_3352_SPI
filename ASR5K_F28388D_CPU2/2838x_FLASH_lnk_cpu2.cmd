/*
 * F2838x CPU2 Flash Linker Command File
 * Define CLA_C in project settings if using CLA C compiler
 */

#ifdef CLA_C
// CLA scratchpad size for local symbols and temps
CLA_SCRATCHPAD_SIZE = 0x100;
--undef_sym=__cla_scratchpad_end
--undef_sym=__cla_scratchpad_start
#endif

MEMORY
{
   /*========================================================================*/
   /* PAGE 0: Program Memory                                                 */
   /*========================================================================*/
   PAGE 0 :
   
   /* Boot and Reset Vectors */
   BEGIN            : origin = 0x080000, length = 0x000002  // Boot entry point
   RESET            : origin = 0x3FFFC0, length = 0x000002  // Reset vector
   
   /* RAM Blocks for Program */
   RAMM0            : origin = 0x0001A9, length = 0x000257  // M0 RAM
   RAMD0            : origin = 0x00C000, length = 0x000800  // D0 RAM
   RAMLS01          : origin = 0x008000, length = 0x001000  // CPU1 RAM functions
   RAMLS45          : origin = 0x00A000, length = 0x001000  // CLA RAM functions
   RAMGS5           : origin = 0x012000, length = 0x001000  // GS5 RAM
   RAMGS6           : origin = 0x013000, length = 0x001000  // GS6 RAM
   RAMGS15          : origin = 0x01C000, length = 0x000FF8  // GS15 RAM for FAPI
   
   /* Flash Memory Sectors */
   FLASH0           : origin = 0x080002, length = 0x001FFE  // Init sections
   FLASH1           : origin = 0x082000, length = 0x002000  // Program code
   FLASH2           : origin = 0x084000, length = 0x002000  // Program code
   FLASH3           : origin = 0x086000, length = 0x002000  // Program code
   FLASH4           : origin = 0x088000, length = 0x008000  // Program code
   FLASH5           : origin = 0x090000, length = 0x008000  // RAM functions
   FLASH6           : origin = 0x098000, length = 0x008000  // Switch tables
   FLASH7           : origin = 0x0A0000, length = 0x008000  // Init arrays
   FLASH8           : origin = 0x0A8000, length = 0x008000  // Constants
   FLASH9           : origin = 0x0B0000, length = 0x008000  // CLA program
   FLASH10          : origin = 0x0B8000, length = 0x002000  // Reserved
   FLASH11          : origin = 0x0BA000, length = 0x002000  // FAPI functions
   FLASH12          : origin = 0x0BC000, length = 0x002000  // User storage
   FLASH13          : origin = 0x0BE000, length = 0x001FF0  // User storage


   /*========================================================================*/
   /* PAGE 1: Data Memory                                                   */
   /*========================================================================*/
   PAGE 1 :
   
   /* Boot Reserved and M1 RAM */
   BOOT_RSVD        : origin = 0x000002, length = 0x0001A7  // Boot ROM stack
   RAMM1            : origin = 0x000400, length = 0x0003F8  // M1 RAM block
   RAMD1            : origin = 0x00C800, length = 0x000800  // D1 RAM
   
   /* Local Shared RAM */
   RAMLS23          : origin = 0x009000, length = 0x001000  // CPU1 data
   RAMLS67          : origin = 0x00A800, length = 0x000800  // CLA data
   
   /* Global Shared RAM */
   RAMGS7           : origin = 0x014000, length = 0x001000  // Communication interface
   RAMGS8           : origin = 0x015000, length = 0x001000  // System memory
   RAMGS9           : origin = 0x016000, length = 0x001000  // General purpose
   RAMGS10_11       : origin = 0x017000, length = 0x002000  // CPU2 read/write
   RAMGS12_13       : origin = 0x019000, length = 0x002000  // CPU1 read only
   
   /* Inter-CPU Communication */
   CPU1TOCPU2RAM    : origin = 0x03A000, length = 0x000800  // CPU1 to CPU2 message
   CPU2TOCPU1RAM    : origin = 0x03B000, length = 0x000800  // CPU2 to CPU1 message
   
   /* CPU to CM Communication */
   CPUTOCMRAM       : origin = 0x039000, length = 0x000800  // CPU to CM message
   CMTOCPURAM       : origin = 0x038000, length = 0x000800  // CM to CPU message
}

/*
 * Section Allocation
 */
SECTIONS
{
   /*========================================================================*/
   /* Program Sections                                                      */
   /*========================================================================*/
   
   /* Initialization Sections */
   .cinit              : > FLASH0,     PAGE = 0, ALIGN(8)    // C initialization
   .pinit              : > FLASH0,     PAGE = 0, ALIGN(8)    // C++ initialization
   .init_array         : > FLASH7,     PAGE = 0, ALIGN(8)    // C++ constructor array
   codestart           : > BEGIN,      PAGE = 0, ALIGN(8)    // Boot entry point
   
   /* Program Code */
   .text               : >> FLASH1 | FLASH2 | FLASH3 | FLASH4, PAGE = 0, ALIGN(8)
   .switch             : > FLASH6,     PAGE = 0, ALIGN(8)    // Switch tables
   .reset              : > RESET,      PAGE = 0, TYPE = DSECT // Reset vector (unused)
   
   /* Constants */
   .const              : > FLASH8,     PAGE = 0, ALIGN(8)    // Read-only data
   
   /*========================================================================*/
   /* Data Sections                                                         */
   /*========================================================================*/
   
   /* Uninitialized Data */
   .stack              : > RAMM1,      PAGE = 1              // Program stack
   .bss                : > RAMLS23,    PAGE = 1              // Uninitialized variables
   .data               : > RAMLS23,    PAGE = 1              // Initialized variables
   .bss:output         : > RAMGS8,     PAGE = 1              // Output buffers
   .bss:cio            : > RAMGS8,     PAGE = 1              // C I/O buffers
   .sysmem             : > RAMGS8,     PAGE = 1              // Dynamic memory heap

   /*========================================================================*/
   /* Load-Time Copy Sections                                               */
   /*========================================================================*/
   
   /* RAM Functions (copied from Flash to RAM at startup) */
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

   /* Flash API Functions */
   .fapi_ram : {
       -l F2838x_C28x_FlashAPI.lib
      *(.fapi_ram)
   } LOAD = FLASH11,
     RUN = RAMGS15,
     LOAD_START(fapi_ram_LoadStart),
     LOAD_SIZE(fapi_ram_LoadSize),
     LOAD_END(fapi_ram_LoadEnd),
     RUN_START(fapi_ram_RunStart),
     RUN_SIZE(fapi_ram_RunSize),
     RUN_END(fapi_ram_RunEnd),
     PAGE = 0, ALIGN(8)
   
   /*========================================================================*/
   /* Inter-CPU Communication Sections                                      */
   /*========================================================================*/
   
   /* CPU Communication Areas */
   RW_CPU2                 : > RAMGS10_11,     PAGE = 1      // CPU2 read/write data
   RD_CPU1                 : > RAMGS12_13,     PAGE = 1      // CPU1 read-only data
   
   /* Message RAM Sections */
   MSGRAM_CPU1_TO_CPU2     : > CPU1TOCPU2RAM,  type=NOINIT   // CPU1 to CPU2 messages
   MSGRAM_CPU2_TO_CPU1     : > CPU2TOCPU1RAM,  type=NOINIT   // CPU2 to CPU1 messages
   MSGRAM_CPU_TO_CM        : > CPUTOCMRAM,     type=NOINIT   // CPU to CM messages
   MSGRAM_CM_TO_CPU        : > CMTOCPURAM,     type=NOINIT   // CM to CPU messages
   /*========================================================================*/
   /* CLA (Control Law Accelerator) Sections                               */
   /*========================================================================*/
   
   /* CLA Program (copied from Flash to RAM at startup) */
   Cla1Prog : {
   } LOAD = FLASH9,
     RUN = RAMLS45,
     LOAD_START(Cla1ProgLoadStart),
     RUN_START(Cla1ProgRunStart),
     LOAD_SIZE(Cla1ProgLoadSize),
     PAGE = 0, ALIGN(8)

#ifdef CLA_C
   /* CLA C Compiler Sections */
   CLAscratch : {
       *.obj(CLAscratch)
       . += CLA_SCRATCHPAD_SIZE;
       *.obj(CLAscratch_end)
   } > RAMLS67, PAGE = 1

   .scratchpad         : > RAMLS67,        PAGE = 1          // CLA scratchpad
   .bss_cla            : > RAMLS67,        PAGE = 1          // CLA uninitialized data
   cla_shared          : > RAMLS67,        PAGE = 1          // CLA shared data
   
   .const_cla : {
   } LOAD = FLASH9,
     RUN = RAMLS67,
     RUN_START(Cla1ConstRunStart),
     LOAD_START(Cla1ConstLoadStart),
     LOAD_SIZE(Cla1ConstLoadSize),
     PAGE = 1, ALIGN(8)
#endif

   /*========================================================================*/
   /* Application Specific Sections                                         */
   /*========================================================================*/
   
   .comm_interface     : > RAMGS7,         PAGE = 1          // Communication interface
   .bbox               : > RAMGS7,         PAGE = 1          // Black box data
}

/*
 * End of file
 */
