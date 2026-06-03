/*
 * comm_diag.h
 *
 * Created on: May 22, 2026
 * Author: Antigravity
 * Generic Communication Diagnostics Framework
 * ASCII-ONLY formatted for MS950 compilation safety.
 */

#ifndef COMM_DIAG_H_
#define COMM_DIAG_H_

#include <stdint.h>

/* Standardized Communication Error Types */
typedef enum {
    ERR_COMM_NONE = 0,
    ERR_COMM_TIMEOUT_ACK = 1,
    ERR_COMM_TIMEOUT_DATA = 2,
    ERR_COMM_CHECKSUM = 3,
    ERR_COMM_VAL_MISMATCH = 4,
    ERR_COMM_FIFO_OVERFLOW = 5,
    ERR_COMM_SILENCE_RESET = 6,
    ERR_COMM_CUSTOM_START = 100 /* Custom error types for specific protocols can start here */
} E_COMM_ERR_TYPE;

/* Generic Communication Diagnostic Latch Structure */
typedef struct {
    /* Statistical Counters */
    uint32_t u32TxTotal;        /* Cumulative packets transmitted */
    uint32_t u32RxTotal;        /* Cumulative packets received */
    uint32_t u32ErrCount;       /* Cumulative error count */
    uint32_t u32MaxQueueDepth;  /* Historical peak queue/buffer backlog depth */
    uint32_t u32ResetCount;     /* Historical module reset count */

    /* First-Error Latch Registers (Captured on first error occurrence) */
    uint16_t u16LastErrType;    /* First captured error type (E_COMM_ERR_TYPE) */
    uint16_t u16LastErrStep;    /* Protocol step / state ID where first error occurred */
    uint32_t u32LastErrTime;    /* Timestamp when first error occurred */
    
    /* Context Registers (Raw data associated with first error, e.g., expected/actual values) */
    uint16_t u16ErrDetail[4];   
} ST_COMM_DIAG;

/* API Functions */

/**
 * @brief Initialize or reset diagnostic counters and registers
 * @param diag Pointer to the diagnostic structure
 */
void CommDiag_Init(ST_COMM_DIAG *diag);

/**
 * @brief Report and log a communication error. Locks the first error context.
 * @param diag Pointer to the diagnostic structure
 * @param errType Error type (E_COMM_ERR_TYPE)
 * @param step Active step or state ID where error occurred
 * @param timestamp Current system timer tick count
 * @param d0..d3 Detailed context values (e.g. expected addr, data, checksum, etc.)
 */
void CommDiag_ReportError(ST_COMM_DIAG *diag, uint16_t errType, uint16_t step, 
                          uint32_t timestamp, uint16_t d0, uint16_t d1, 
                          uint16_t d2, uint16_t d3);

/**
 * @brief Clear the locked first-error registers to capture new errors
 * @param diag Pointer to the diagnostic structure
 */
void CommDiag_ClearLatch(ST_COMM_DIAG *diag);

/**
 * @brief Update peak queue depth tracker
 * @param diag Pointer to the diagnostic structure
 * @param currentDepth Current queue backlog count
 */
void CommDiag_UpdateQueueDepth(ST_COMM_DIAG *diag, uint32_t currentDepth);

#endif /* COMM_DIAG_H_ */
