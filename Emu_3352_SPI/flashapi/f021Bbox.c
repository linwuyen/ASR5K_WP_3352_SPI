/**
 * @file f021Bbox.c
 * @brief Flash blackbox storage management implementation
 * @author Cody
 * @date Apr 16, 2024
 */

#include "common.h"
#include "f021Bbox.h"
#include <string.h>

/**
 * @brief Blackbox variable metadata structure
 */
typedef struct
{
    uint32_t u32Address; /**< Variable memory address */
    uint16_t u16Words;   /**< Variable size in words */
    uint16_t u16Chksum;  /**< Data integrity checksum */
} ST_BLACKBOX;

/** @brief Macro to define blackbox variable entry */
#define BBOX_VAR(name) {(uint32_t)&name, sizeof(name), 0}
/** @brief Macro to mark end of blackbox table */
#define END_OF_BBOX_VAR {0, 0, 0}

/**
 * @brief Firmware version structure with bit fields
 */
typedef union
{
    uint32_t u32All;
    struct
    {
        uint16_t u16Length : 8;     /**< Version data length */
        uint16_t u16Version : 8;    /**< Version number */
        uint16_t u16BuildDate : 16; /**< Build date */
    };
} REG_VERSION;

/** @brief Firmware version instance */
REG_VERSION u32Version = {
    .u16BuildDate = FW_BUILDDATE,
    .u16Version = FW_VERSION,
    .u16Length = 0};

/** @brief Flash write operation counter */
uint32_t u32FlashWriteCnts = 0;

/** @brief Blackbox variable table definition - constant template in Flash */
const ST_BLACKBOX bboxTableTemplate[] = {
     BBOX_VAR(u32Version.u32All),
     BBOX_VAR(u32FlashWriteCnts),
     BBOX_VAR(sDrv.u32SN.all),
     BBOX_VAR(sHwConfig.f32VoutScale),
     BBOX_VAR(sHwConfig.f32IoutScale),
     BBOX_VAR(sDrv.sRecord.u32all),
     END_OF_BBOX_VAR};

/** @brief Macro to calculate array size */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define SIZE_OF_BBOX ARRAY_SIZE(bboxTableTemplate)

/** @brief Blackbox variable table instance in GSRAM */
ST_BLACKBOX __attribute__((section(".bbox"))) sBBoxTable[SIZE_OF_BBOX];

/** @brief RAM storage for initial values */
DATA_128BITS __attribute__((section(".bbox"))) sBBoxInitValues[SIZE_OF_BBOX];
/** @brief RAM buffer for delayed write operations */
DATA_128BITS __attribute__((section(".bbox"))) sBBoxTempBuffer[SIZE_OF_BBOX];

/**
 * @brief Initializes the blackbox table by copying from Flash template to GSRAM
 */
void initBBoxTable(void)
{
    memcpy(sBBoxTable, bboxTableTemplate, sizeof(bboxTableTemplate));
}

/* Flash memory layout constants */
#define SECTION_1_BASE 0x0BC000 /**< Section 1 base address */
#define SECTION_2_BASE 0x0BE000 /**< Section 2 base address */
#define SECTION_COUNT 2         /**< Number of sections */
#define SECTION_SIZE 0x2000     /**< Section size in bytes */
#define PART_SIZE 0x400         /**< Part size in bytes */
#define PART_COUNT 8            /**< Parts per section */
#define START_OF_PART_ADDRESS (SECTION_1_BASE)
#define END_OF_PART_ADDRESS (START_OF_PART_ADDRESS + PART_COUNT * SECTION_COUNT * PART_SIZE)

/** @brief Macro to compute part address */
#define GET_PART_ADDRESS(section_base, part_index) (section_base + (PART_SIZE * (part_index)))
/** @brief Macro to get section base address */
#define GET_SECTION_ADDRESS(address) (address & ~(SECTION_SIZE - 1))

/** @brief Global FSM instance */
ST_BBOX sBbox = {
    .fsm = _INIT_FLASH_STORAGE,
    .u32WritePending = 0};

#ifdef _FLASH
#pragma SET_CODE_SECTION(".fapi_ram")
#endif //_FLASH

/**
 * @brief Backs up current variable values to temporary buffer
 */
void backupCurrentValuesToBuffer(void)
{
    for (uint32_t i = 0; (sBBoxTable[i].u32Address != 0); i++)
    {
        uint16_t *src = (uint16_t *)sBBoxTable[i].u32Address;

        for (uint16_t j = 0; j < sBBoxTable[i].u16Words; j++)
        {
            sBBoxTempBuffer[i].data[j] = src[j];
        }

        sBBoxTempBuffer[i].size = sBBoxTable[i].u16Words;
        sBBoxTempBuffer[i].address = sBBoxTable[i].u32Address;
        sBBoxTempBuffer[i].chksum = sBBoxTempBuffer[i].size + sBBoxTable[i].u32Address;
    }
}

/**
 * @brief Restores buffered values to variables
 */
void restoreBufferToVariables(void)
{
    for (uint32_t i = 0; (sBBoxTable[i].u32Address != 0); i++)
    {
        uint16_t *dst = (uint16_t *)sBBoxTable[i].u32Address;

        for (uint16_t j = 0; j < sBBoxTable[i].u16Words; j++)
        {
            dst[j] = sBBoxTempBuffer[i].data[j];
        }
    }
}

/**
 * @brief Restores buffered values to variables
 */
int16_t isPwmOFF(void)
{
    return FG_GETn(_CSTAT_OUTPUT_ON, sDrv.fgStatus);
}

/**
 * @brief Handles buffered write request FSM
 */
void handleBufferWriteRequest(void)
{
    switch (sBbox.fsm)
    {
    case _BUFFER_WRITE_REQUEST:
        if (0 == sBbox.u32WritePending)
        {
            backupCurrentValuesToBuffer();
            sBbox.u32WritePending = 1;
        }
        sBbox.fsm = _CHECK_PWM_STATUS_FOR_WRITE;
        break;

    case _CHECK_PWM_STATUS_FOR_WRITE:
        if (0 == isPwmOFF())
        {
            sBbox.fsm = _BBOX_FREE;
        }
        else
        {
            sBbox.fsm = _RESTORE_BUFFER_TO_VARS;
        }
        break;

    case _RESTORE_BUFFER_TO_VARS:
        if (sBbox.u32WritePending)
        {
            restoreBufferToVariables();

            sBbox.u32WritePending = 0;
            sBbox.fsm = _WRITE_BBOX_INTO_FLASH;
        }
        else
        {
            sBbox.fsm = _BBOX_FREE;
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Triggers flash write operation
 * @details Writes immediately if PWM is OFF, otherwise buffers the request
 */
void saveDataToFlash(void)
{
    if (isPwmOFF())
    {
        sBbox.fsm = _WRITE_BBOX_INTO_FLASH;
    }
    else
    {
        sBbox.fsm = _BUFFER_WRITE_REQUEST;
    }
}

/**
 * @brief Formats all flash sections
 */
void formatAllSection(void)
{
    sBbox.fsm = _FORMAT_ALL_SECTION;
}

/**
 * @brief Checks if callback operation completed
 * @return 1 if ready, 0 otherwise
 */
uint16_t isCallbackReady(void)
{
    return ((sBbox.u32Stat & (_READ_BACK_FROM_BBOX)) == (_READ_BACK_FROM_BBOX));
}

/**
 * @brief Validates flash data integrity
 * @param pFlashData Flash data pointer
 * @param expectedSize Expected data size
 * @return 1 if valid, 0 otherwise
 */
uint16_t isFlashDataValid(HAL_128BITS pFlashData, uint32_t expectedSize)
{
    if ((0xFF != pFlashData->size) && (0 < pFlashData->size))
    {
        uint16_t u16chksum = pFlashData->chksum;
        for (uint16_t i = 1; i < 8; i++)
        {
            u16chksum -= pFlashData->all[i];
        }
        return (u16chksum == 0 && pFlashData->size == expectedSize);
    }
    return 0;
}

/**
 * @brief Retrieves valid data from flash
 * @param pu16 Destination buffer
 * @param pFlashData Flash data source
 */
void getFlashValidData(uint16_t *pu16, HAL_128BITS pFlashData)
{
    for (uint16_t i = 0; i < pFlashData->size; i++)
    {
        pu16[i] = pFlashData->data[i];
    }
}

/**
 * @brief Reads data with specific write count
 * @param u32WriteCnt Target write count
 */
void readDataFromToFlash(uint32_t u32WriteCnt)
{

    for (uint32_t partAddress = START_OF_PART_ADDRESS; partAddress < END_OF_PART_ADDRESS; partAddress += PART_SIZE)
    {
        HAL_128BITS pFlashData = &((HAL_128BITS)partAddress)[1];

        if (isFlashDataValid(pFlashData, sBBoxTable[1].u16Words))
        {
            uint32_t flashWriteCnts = 0;
            getFlashValidData((uint16_t *)&flashWriteCnts, pFlashData);

            if (flashWriteCnts == u32WriteCnt)
            {
                sBbox.u32ReadFrom = partAddress;
                sBbox.fsm = _READ_BACK_FROM_BBOX;
                sBbox.u32Stat = (_BBOX_FREE | _READ_BACK_FROM_BBOX);
                return;
            }
        }
    }

    sBbox.u32Stat = (_READ_BACK_FROM_BBOX | _MARK_ERROR_FOR_BBOX);
}

/**
 * @brief Finds latest flash data part
 * @return Address of latest data
 */
uint32_t findLatestFlashData(void)
{
    if (sBbox.u32ReadFrom == 0)
    {
        uint32_t maxCnts = 0;

        for (uint32_t partAddress = START_OF_PART_ADDRESS; partAddress < END_OF_PART_ADDRESS; partAddress += PART_SIZE)
        {
            HAL_128BITS pFlashData = &((HAL_128BITS)partAddress)[1];

            if (isFlashDataValid(pFlashData, sBBoxTable[1].u16Words))
            {
                uint32_t flashWriteCnts = 0;
                getFlashValidData((uint16_t *)&flashWriteCnts, pFlashData);

                if (flashWriteCnts > maxCnts)
                {
                    maxCnts = flashWriteCnts;
                    sBbox.u32ReadFrom = partAddress;
                }
            }
        }
    }
    return sBbox.u32ReadFrom;
}

/**
 * @brief Finds oldest flash section
 * @return Address of oldest section
 */
uint32_t findOldestSection(void)
{
    if (sBbox.u32OldestSection == 0)
    {
        uint32_t minWriteCnts = UINT32_MAX;

        for (uint32_t partAddress = START_OF_PART_ADDRESS; partAddress < END_OF_PART_ADDRESS; partAddress += PART_SIZE)
        {
            HAL_128BITS pFlashData = &((HAL_128BITS)partAddress)[1];

            if (isFlashDataValid(pFlashData, sBBoxTable[1].u16Words))
            {
                uint32_t flashWriteCnts = 0;
                getFlashValidData((uint16_t *)&flashWriteCnts, pFlashData);

                if (flashWriteCnts < minWriteCnts)
                {
                    minWriteCnts = flashWriteCnts;
                    sBbox.u32OldestSection = GET_SECTION_ADDRESS(partAddress);
                }
            }
        }
    }
    return sBbox.u32OldestSection;
}

/**
 * @brief Finds next empty flash part
 * @return Address of empty part, 0 if none found
 */
uint32_t getEmptyAddress(void)
{
    if (sBbox.u32NextEmptyAddress == 0)
    {
        for (uint32_t partAddress = START_OF_PART_ADDRESS; partAddress < END_OF_PART_ADDRESS; partAddress += PART_SIZE)
        {
            uint32_t *pData = (uint32_t *)partAddress;

            if (*pData == 0xFFFFFFFF)
            {
                sBbox.u32NextEmptyAddress = partAddress;
                return sBbox.u32NextEmptyAddress;
            }
        }
        return 0;
    }
    return sBbox.u32NextEmptyAddress;
}

/**
 * @brief Prepares flash data block for writing
 * @param pu16 Source data pointer
 * @param regTempData Temporary data structure
 * @param pBBox Blackbox table entry
 */
void prepareFlashData(uint16_t *pu16, DATA_128BITS *regTempData, ST_BLACKBOX *pBBox)
{
    regTempData->address = pBBox->u32Address;
    regTempData->size = pBBox->u16Words;
    regTempData->chksum = regTempData->size + regTempData->address;

    for (uint16_t i = 0; i < 4; i++)
    {
        if (i < pBBox->u16Words)
        {
            regTempData->data[i] = pu16[i];
            regTempData->chksum += pu16[i];
        }
        else
        {
            regTempData->data[i] = 0;
        }
    }
}

/**
 * @brief Backs up initial values to RAM
 */
void backupInitialValuesToRAM(void)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(sBBoxTable); i++)
    {
        uint16_t *src = (uint16_t *)sBBoxTable[i].u32Address;
        for (uint32_t j = 0; j < sBBoxTable[i].u16Words; j++)
        {
            sBBoxInitValues[i].data[j] = src[j];
        }
        sBBoxInitValues[i].size = sBBoxTable[i].u16Words;
        sBBoxInitValues[i].chksum = sBBoxInitValues[i].size + sBBoxTable[i].u32Address;
    }
}

/**
 * @brief Restores initial values from RAM
 */
void restoreInitialValuesToRAM(void)
{
    for (uint32_t i = 0; i < ARRAY_SIZE(sBBoxTable); i++)
    {
        DATA_128BITS *pInitData = &sBBoxInitValues[i];

        uint16_t *pVariable = (uint16_t *)sBBoxTable[i].u32Address;
        for (uint16_t j = 0; j < sBBoxTable[i].u16Words; j++)
        {
            pVariable[j] = pInitData->data[j];
        }
    }
}

/**
 * @brief Handles export block binary to PC operation
 * @details Backs up current values to buffer and handles sequential data export via Modbus
 */
void handleExportBlockBinaryToPC(void)
{

    if (1 == sBbox.u32Return)
    {
        sBbox.u32Done = 0;
        backupCurrentValuesToBuffer();
    }

    int32_t index = sBbox.u32Return - 1; // Convert 1-based index to 0-based (1 -> TABLE0)

    if (index >= 0 && index < SIZE_OF_BBOX)
    {
        // Get data based on variable size (supports 16-bit or 32-bit)
        uint16_t low_word = sBBoxTempBuffer[index].data[0];
        uint16_t high_word = (sBBoxTable[index].u16Words > 1) ? sBBoxTempBuffer[index].data[1] : 0;

        // Combine into 32-bit for Modbus transmission
        sBbox.u32Data = ((uint32_t)high_word << 16) | low_word;

        // Set u32Return to 0 to indicate data is ready
        sBbox.u32Return = 0;
        sBbox.u32Done++;
    }
}

/**
 * @brief Handles import block binary from PC operation
 * @details Prepares buffer and handles sequential data import from Modbus to buffer
 */
void handleImportBlockBinaryFromPC(void)
{
    if (1 == sBbox.u32Return)
    {
        sBbox.u32Done = 0;
    }

    // Use u32Return as the 1-based index set by PC
    int32_t index = sBbox.u32Return - 1;

    if (index >= 0 && index < SIZE_OF_BBOX)
    {
        // Write the data from u32Data to buffer (supports 16-bit or 32-bit)
        uint32_t data = sBbox.u32Data;
        sBBoxTempBuffer[index].data[0] = data & 0xFFFF; // Low word

        if (sBBoxTable[index].u16Words > 1)
        {
            sBBoxTempBuffer[index].data[1] = (data >> 16) & 0xFFFF; // High word
        }
        // Set u32Return to 0 to indicate data has been written
        sBbox.u32Done++;
        sBbox.u32Return = 0;

        if (index == (SIZE_OF_BBOX - 2))
        {
            // Last variable, restore buffer to variables
            restoreBufferToVariables();
        }
    }
}

/**
 * @brief Checks flash FSM ready status
 * @param nextState Next state on ready
 */
void checkFlashReady(FSM_BBOX nextState)
{
    sBbox.oReturnCheck = Fapi_checkFsmForReady();
    if (Fapi_Status_FsmReady == sBbox.oReturnCheck)
    {
        sBbox.fsm = nextState;
    }
}

/**
 * @brief Initializes flash storage system
 */
void initFlashStorage(void)
{
    switch (sBbox.fsm)
    {
    case _NO_ACTION_FOR_BBOX:
    case _INIT_FLASH_STORAGE:
        initBBoxTable(); // Initialize table from Flash template
        backupInitialValuesToRAM();

        sBbox.u32Ammount = 0;
        sBbox.u32DelayCnt = FAPI_DELAY_CNTS;
        sBbox.pFlashData = (HAL_128BITS)START_OF_PART_ADDRESS;
        sBbox.u32ReadFrom = 0;
        sBbox.u32OldestSection = 0;
        sBbox.u32NextEmptyAddress = 0;

        sBbox.fsm = _FIND_LATEST_PART;
        break;

    case _FIND_LATEST_PART:
        findLatestFlashData();
        sBbox.fsm = _FIND_OLDEST_SECTION;
        break;

    case _FIND_OLDEST_SECTION:
        findOldestSection();
        sBbox.fsm = _FIND_EMPTY_PART;
        break;

    case _FIND_EMPTY_PART:
        getEmptyAddress();
        sBbox.fsm = _CHECK_INIT_FLASH_READY;
        break;

    case _CHECK_INIT_FLASH_READY:
        if (sBbox.u32ReadFrom != 0 && sBbox.u32OldestSection != 0 && sBbox.u32NextEmptyAddress != 0)
        {
            sBbox.fsm = _READ_BACK_FROM_BBOX;
        }
        else
        {
            sBbox.fsm = _MARK_ERROR_FOR_BBOX;
            sBbox.u32Stat = (_CHECK_INIT_FLASH_READY | _MARK_ERROR_FOR_BBOX);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Reads data from flash memory
 */
void readDataFromFlash(void)
{
    switch (sBbox.fsm)
    {
    case _READ_BACK_FROM_BBOX:
        sBbox.u32Address = findLatestFlashData();
        sBbox.pFlashData = (HAL_128BITS)sBbox.u32Address;
        sBbox.fsm = _GET_THE_AMMOUNT_OF_BBOX_VAR;
        break;

    case _GET_THE_AMMOUNT_OF_BBOX_VAR:
        getFlashValidData((uint16_t *)sBBoxTable[0].u32Address, &sBbox.pFlashData[0]);
        sBbox.u32Ammount = u32Version.u16Length;
        sBbox.u32Index = 1;
        sBbox.fsm = _IS_VALID_BBOX_VAR_IN_FLASH;
        break;

    case _IS_VALID_BBOX_VAR_IN_FLASH:
        if (sBbox.u32Index < sBbox.u32Ammount)
        {
            if (isFlashDataValid(&sBbox.pFlashData[sBbox.u32Index], sBBoxTable[sBbox.u32Index].u16Words))
            {
                getFlashValidData((uint16_t *)sBBoxTable[sBbox.u32Index].u32Address, &sBbox.pFlashData[sBbox.u32Index]);
            }
            sBbox.u32Index++;
        }
        else
        {
            sBbox.fsm = _BBOX_FREE;
            sBbox.u32Stat = (_BBOX_FREE | _READ_BACK_FROM_BBOX);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Writes data to flash memory
 */
void writeDataToFlash(void)
{
    EALLOW;
    switch (sBbox.fsm)
    {
    case _WRITE_BBOX_INTO_FLASH:
        u32Version.u16BuildDate = FW_BUILDDATE;
        u32Version.u16Version = FW_VERSION;

        sBbox.u32Address = getEmptyAddress();
        sBbox.pFlashData = (HAL_128BITS)sBbox.u32Address;
        sBbox.fsm = _INIT_FLASH_FOR_BBOX;
        break;

    case _INIT_FLASH_FOR_BBOX:
        sBbox.oReturnCheck = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, CPUCLK_FREQUENCY);
        if (Fapi_Status_Success == sBbox.oReturnCheck)
        {
            sBbox.fsm = _INIT_BANK0_FOR_BBOX;
        }
        break;

    case _INIT_BANK0_FOR_BBOX:
        sBbox.oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);
        if (Fapi_Status_Success == sBbox.oReturnCheck)
        {
            sBbox.fsm = _PREPARE_DATA_BLOCK_INFO;
        }
        break;

    case _PREPARE_DATA_BLOCK_INFO:
        for (sBbox.u32Ammount = 0; 0 != sBBoxTable[sBbox.u32Ammount].u32Address; sBbox.u32Ammount++)
            ;
        sBbox.u32Index = 0;
        u32FlashWriteCnts++;
        u32Version.u16Length = sBbox.u32Ammount;
        sBbox.fsm = _PROGRAM_BBOX_VAR_INTO_FLASH;
        break;

    case _PROGRAM_BBOX_VAR_INTO_FLASH:
        if (sBbox.u32Index < sBbox.u32Ammount)
        {
            prepareFlashData((uint16_t *)sBBoxTable[sBbox.u32Index].u32Address, &sBbox.regTempData, &sBBoxTable[sBbox.u32Index]);

            Fapi_issueProgrammingCommand(
                (uint32 *)&sBbox.pFlashData[sBbox.u32Index],
                (uint16 *)sBbox.regTempData.all,
                ALIGN_WORD_ADDRESS_BY,
                0, 0, Fapi_AutoEccGeneration);

            sBbox.u32Index++;
            sBbox.fsm = _WAIT_FOR_PROGRAM_BBOX_READY;
        }
        else
        {
            sBbox.u32ReadFrom = sBbox.u32NextEmptyAddress;
            sBbox.u32NextEmptyAddress += PART_SIZE;
            if (END_OF_PART_ADDRESS == sBbox.u32NextEmptyAddress)
            {
                sBbox.u32NextEmptyAddress = START_OF_PART_ADDRESS;
            }

            uint32_t *pData = (uint32_t *)sBbox.u32NextEmptyAddress;
            if (*pData != 0xFFFFFFFF)
            {
                sBbox.u32OldestSection = GET_SECTION_ADDRESS(sBbox.u32NextEmptyAddress);
                sBbox.fsm = _FORMAT_OLDEST_SECTION;
            }
            else
            {
                sBbox.fsm = _READ_BACK_FROM_BBOX;
            }
        }
        break;

    case _WAIT_FOR_PROGRAM_BBOX_READY:
        checkFlashReady(_PROGRAM_BBOX_VAR_INTO_FLASH);
        break;

    case _FORMAT_OLDEST_SECTION:
        sBbox.oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32 *)sBbox.u32OldestSection);
        if (Fapi_Status_Success == sBbox.oReturnCheck)
        {
            sBbox.fsm = _WAIT_FOR_ERASING_OLDEST_SECTION;
        }
        break;

    case _WAIT_FOR_ERASING_OLDEST_SECTION:
        sBbox.oReturnCheck = Fapi_checkFsmForReady();
        if (Fapi_Status_FsmReady == sBbox.oReturnCheck)
        {
            sBbox.fsm = _WRITE_BBOX_INTO_FLASH;
        }
        break;

    default:
        break;
    }

    EDIS;
}

/**
 * @brief Formats all flash sections
 */
void formatFlashSection(void)
{
    EALLOW;
    switch (sBbox.fsm)
    {
    case _FORMAT_ALL_SECTION:
        sBbox.fsm = _INIT_FLASH_FOR_BBOX;
        break;


    case _INIT_FLASH_FOR_BBOX:
        sBbox.oReturnCheck = Fapi_initializeAPI(F021_CPU0_BASE_ADDRESS, CPUCLK_FREQUENCY);
        if (Fapi_Status_Success == sBbox.oReturnCheck)
        {
            sBbox.fsm = _INIT_BANK0_FOR_BBOX;
        }
        break;

    case _INIT_BANK0_FOR_BBOX:
        sBbox.oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);
        if (Fapi_Status_Success == sBbox.oReturnCheck)
        {
            sBbox.u32Address = START_OF_PART_ADDRESS;
            sBbox.fsm = _ERASE_THE_FLASH_SECTION_1;
        }
        break;

    case _ERASE_THE_FLASH_SECTION_1:
        sBbox.oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32 *)sBbox.u32Address);
        sBbox.fsm = _WAIT_FOR_ERASE_THE_FLASH_SECTION_1;
        break;

    case _WAIT_FOR_ERASE_THE_FLASH_SECTION_1:
        sBbox.oReturnCheck = Fapi_checkFsmForReady();
        if (Fapi_Status_FsmReady == sBbox.oReturnCheck)
        {
            if (sBbox.u32Address < END_OF_PART_ADDRESS)
            {
                sBbox.u32Address += SECTION_SIZE;
                sBbox.fsm = _ERASE_THE_FLASH_SECTION_1;
            }
            else
            {
                sBbox.fsm = _DEFAULT_ADDRESS_AFTER_ERASE;
            }
        }

        break;

    case _DEFAULT_ADDRESS_AFTER_ERASE:
        u32FlashWriteCnts = 0;
        sBbox.u32ReadFrom = START_OF_PART_ADDRESS;
        sBbox.u32OldestSection = START_OF_PART_ADDRESS;
        sBbox.u32NextEmptyAddress = START_OF_PART_ADDRESS;
        restoreInitialValuesToRAM();
        recalParameters();

        sBbox.fsm = _WRITE_BBOX_INTO_FLASH;
        break;

    default:
        break;
    }

    EDIS;
}

/**
 * @brief Error handler for flash operations
 */
void getErrorHandler(void)
{
}

/** @brief Flash operation function pointer */
void (*pfnRunFlash)(void) = initFlashStorage;

/**
 * @brief Main flash storage execution function
 * @details Entry point for all flash operations. Handles pending writes when PWM is OFF.
 */
void runFlashStorage(void)
{
    if (_BBOX_FREE == sBbox.fsm)
    {
        if (sBbox.u32WritePending && (isPwmOFF()))
        {
            sBbox.fsm = _CHECK_PWM_STATUS_FOR_WRITE;
        }
        else
        {
            return;
        }
    }

    switch (sBbox.fsm)
    {
    case _INIT_FLASH_STORAGE:
        pfnRunFlash = initFlashStorage;
        break;

    case _READ_BACK_FROM_BBOX:
        pfnRunFlash = readDataFromFlash;
        break;

    case _BUFFER_WRITE_REQUEST:
    case _CHECK_PWM_STATUS_FOR_WRITE:
    case _RESTORE_BUFFER_TO_VARS:
        pfnRunFlash = handleBufferWriteRequest;
        break;

    case _WRITE_BBOX_INTO_FLASH:
        pfnRunFlash = writeDataToFlash;
        break;

    case _FORMAT_ALL_SECTION:
        pfnRunFlash = formatFlashSection;
        break;

    case _MARK_ERROR_FOR_BBOX:
        pfnRunFlash = getErrorHandler;
        break;

    default:
        break;
    }

    pfnRunFlash();
}

#ifdef _FLASH
#pragma SET_CODE_SECTION()
#endif //_FLASH
//
// End of File
//
