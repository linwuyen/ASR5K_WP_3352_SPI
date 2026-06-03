/*
 * f021Bbox.h
 *
 *  Created on: Apr 16, 2024
 *      Author: User
 */

#ifndef F021BBOX_H_
#define F021BBOX_H_

/**
 * @brief Flash operation FSM states
 */
typedef enum
{
    _NO_ACTION_FOR_BBOX = (0x00000000),
    _BBOX_FREE,

    /* Initialization states */
    _INIT_FLASH_STORAGE = (0x00010000 << 0),
    _FIND_LATEST_PART,
    _FIND_OLDEST_SECTION,
    _FIND_EMPTY_PART,
    _CHECK_INIT_FLASH_READY,

    /* Read operation states */
    _READ_BACK_FROM_BBOX = (0x00010000 << 1),
    _GET_THE_AMMOUNT_OF_BBOX_VAR,
    _IS_VALID_BBOX_VAR_IN_FLASH,

    /* Write operation states */
    _BUFFER_WRITE_REQUEST = (0x00010000 << 2),
    _CHECK_PWM_STATUS_FOR_WRITE,
    _RESTORE_BUFFER_TO_VARS,
    _WRITE_BBOX_INTO_FLASH,
    _PREPARE_DATA_BLOCK_INFO,
    _ENABLE_FLASH_PUMP_FOR_BBOX,
    _WAIT_FOR_BBOX_PUMP_UP,
    _INIT_FLASH_FOR_BBOX,
    _INIT_BANK0_FOR_BBOX,
    _FORMAT_OLDEST_SECTION,
    _WAIT_FOR_ERASING_OLDEST_SECTION,
    _PROGRAM_BBOX_VAR_INTO_FLASH,
    _WAIT_FOR_PROGRAM_BBOX_READY,

    /* Format operation states */
    _FORMAT_ALL_SECTION = (0x00010000 << 3),
    _ERASE_THE_FLASH_SECTION_1,
    _WAIT_FOR_ERASE_THE_FLASH_SECTION_1,
    _DEFAULT_ADDRESS_AFTER_ERASE,

    /* Error state */
    _MARK_ERROR_FOR_BBOX = (0x80000000),

} FSM_BBOX;

/**
 * @brief 128-bit flash data block structure
 */
typedef union
{
    uint16_t all[8]; /**< Complete data array */
    struct
    {
        uint16_t chksum;  /**< Data checksum */
        uint16_t size;    /**< Block size */
        uint16_t data[4]; /**< Data payload */
        uint32_t address; /**< Associated address */
    };
} DATA_128BITS;

/** @brief Pointer type for 128-bit data block */
typedef DATA_128BITS *HAL_128BITS;

/**
 * @brief Blackbox FSM state and data structure
 */
typedef struct
{
    FSM_BBOX fsm;                 /**< Current FSM state */
    uint32_t u32Stat;             /**< Status flags */
    uint32_t u32Ammount;          /**< Data amount to process */
    uint32_t u32Index;            /**< Current processing index */
    HAL_128BITS pFlashData;       /**< Flash data pointer */
    DATA_128BITS regTempData;     /**< Temporary data buffer */
    uint32_t u32DelayCnt;         /**< Operation delay counter */
    uint32_t u32Address;          /**< Current address */
    uint32_t u32ReadFrom;         /**< Latest valid data address */
    uint32_t u32OldestSection;    /**< Oldest section address */
    uint32_t u32NextEmptyAddress; /**< Next empty flash address */
    uint32_t u32WritePending;     /**< Buffered write pending flag */
    Fapi_StatusType oReturnCheck; /**< Flash API return status */
    uint32_t u32Data;             /**< Remote Data */
    uint32_t u32Return;           /**< Return Data */
    uint32_t u32Done;             /**< Access Counter */
} ST_BBOX;

extern ST_BBOX sBbox;


extern void runFlashStorage(void);
extern uint16_t isCallbackReady(void);
extern void saveDataToFlash(void);
extern void readDataFromToFlash(uint32_t u32WriteCnt);
extern void formatAllSection(void);
extern void handleExportBlockBinaryToPC(void);
extern void handleImportBlockBinaryFromPC(void);

#endif /* F021BBOX_H_ */
