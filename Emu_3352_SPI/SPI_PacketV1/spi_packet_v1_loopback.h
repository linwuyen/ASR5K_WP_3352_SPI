/*
 * spi_packet_v1_loopback.h
 *
 * SPI Packet V1 - pure-C parser -> dispatcher -> response-encoder loopback (A5).
 *
 * Drives the complete pure-C request/response path on raw 16-bit words:
 *
 *   req_words[] -> SpiPacketV1_ParseWords (A2)
 *               -> ST_SPI_PACKET_V1
 *               -> SpiPacketV1_Dispatch    (A4)
 *               -> ST_PKTV1_DISPATCH_RSP
 *               -> SpiPacketV1_Encode      (A2 encoder, reused; no duplicate)
 *               -> rsp_words[]
 *
 * Board-verifiable: NO. This is a pure-C verification of the command path; it
 * is NOT a board / SPIB integration and does not touch any transport.
 *
 * Hard rules (see docs/SPI_PACKET_V1_A5_LOOPBACK.md):
 *   - pure C; depends only on <stdint.h> + the Packet V1 headers
 *   - no SPIB runtime / DMA / FIFO, no driverlib / device.h / board.h
 *   - does NOT include or modify cmd_id.h, does NOT call any legacy parser
 *   - reuses the existing A2 encoder/parser and A3/A4 dispatcher unchanged
 *   - no globals / no CCS-watch state; pure function of inputs -> outputs
 */

#ifndef SPI_PACKET_V1_LOOPBACK_H_
#define SPI_PACKET_V1_LOOPBACK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PKTV1_LOOPBACK_OK = 0,           /* a valid response frame was produced     */
    PKTV1_LOOPBACK_ERR_NULL,         /* a required pointer argument was NULL     */
    PKTV1_LOOPBACK_ERR_PARSE,        /* A2 parser rejected the request frame     */
    PKTV1_LOOPBACK_ERR_DISPATCH,     /* dispatcher could not produce a response  */
    PKTV1_LOOPBACK_ERR_RSP_OVERFLOW, /* response frame exceeds rsp_word_capacity */
    PKTV1_LOOPBACK_ERR_ENCODE        /* response encoder failed (unexpected)     */
} PKTV1_LOOPBACK_RESULT_e;

/*
 * Diagnostic view of one loopback call. All fields are caller-readable after
 * the call. Codes are stored as uint16_t copies of the underlying enums so the
 * struct stays a flat value type. Untouched stages stay 0.
 */
typedef struct
{
    PKTV1_LOOPBACK_RESULT_e result;          /* same as the return value         */
    uint16_t                parser_result;   /* SPI_PACKET_V1_RESULT_e of parse   */
    uint16_t                dispatch_result; /* PKTV1_DISPATCH_RESULT_e of dispatch */
    uint16_t                request_cmd;     /* parsed request command_id         */
    uint16_t                response_cmd;    /* encoded response command_id       */
    uint16_t                response_words;  /* encoded response frame word count */
} ST_PKTV1_LOOPBACK_DIAG;

/*
 * Run one request frame through the full pure-C path and emit the response
 * frame words. Caller owns all buffers.
 *
 *   req_words / req_word_count : raw request frame to parse (A2 wire format)
 *   rsp_words / rsp_word_capacity : destination for the response frame
 *   rsp_word_count : receives the response frame length (0 when no frame)
 *   diag : optional (may be NULL); zeroed then filled with per-stage detail
 *
 * Policy:
 *   - req_words / rsp_words / rsp_word_count NULL -> PKTV1_LOOPBACK_ERR_NULL
 *     (diag, if present, records ERR_NULL; *rsp_word_count set to 0 if non-NULL)
 *   - parser rejects the frame -> PKTV1_LOOPBACK_ERR_PARSE, no response,
 *     *rsp_word_count = 0, dispatcher is NOT called (malformed framing is an
 *     A2 concern, never dispatched).
 *   - parser OK: the dispatcher always yields a valid response MODEL (a normal
 *     reply on success, or a PKTV1_RSP_ERROR envelope on a semantic error such
 *     as forbidden / unsupported / bad-length / echo-overflow). That model is
 *     encoded into a valid response frame and the call returns
 *     PKTV1_LOOPBACK_OK; the semantic detail is in diag.dispatch_result and the
 *     response payload. (A dispatcher NULL result, which cannot occur after a
 *     successful parse, maps to PKTV1_LOOPBACK_ERR_DISPATCH with no frame.)
 *   - response frame does not fit rsp_word_capacity -> PKTV1_LOOPBACK_ERR_RSP_
 *     OVERFLOW, *rsp_word_count = 0, rsp_words left untouched (no overwrite).
 *
 * Pure: no globals, no I/O, no transport. Identical inputs -> identical outputs.
 */
PKTV1_LOOPBACK_RESULT_e SpiPacketV1_Loopback(const uint16_t         *req_words,
                                             uint16_t                req_word_count,
                                             uint16_t               *rsp_words,
                                             uint16_t                rsp_word_capacity,
                                             uint16_t               *rsp_word_count,
                                             ST_PKTV1_LOOPBACK_DIAG *diag);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_LOOPBACK_H_ */
