/*
 * spi_packet_v1_dispatch.h
 *
 * SPI Packet V1 - pure-C command dispatcher mock (A4).
 *
 * The dispatcher consumes a packet that the A2 parser has ALREADY validated
 * (framing + CRC) and turns it into a pure-C response model based solely on
 * command_id / payload length / payload content. It is a "mock": it computes a
 * deterministic response object; it does NOT touch any transport.
 *
 * Hard rules (see docs/SPI_PACKET_V1_A4_DISPATCHER_MOCK.md):
 *   - pure C; depends only on <stdint.h> + spi_packet_v1.h + spi_packet_v1_cmd.h
 *   - no SPIB runtime / DMA / FIFO, no driverlib / device.h / board.h
 *   - does NOT include cmd_id.h, does NOT call any legacy register parser
 *   - does NOT verify CRC (that is the A2 parser/encoder layer)
 *   - no globals / no CCS-watch state; pure function of (req) -> (*rsp)
 */

#ifndef SPI_PACKET_V1_DISPATCH_H_
#define SPI_PACKET_V1_DISPATCH_H_

#include <stdint.h>
#include "spi_packet_v1.h"      /* ST_SPI_PACKET_V1, SPI_PACKET_V1_MAX_PAYLOAD_WORDS */
#include "spi_packet_v1_cmd.h"  /* command catalog / error codes               */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dispatcher response payload capacity, in 16-bit words. This is the A4 mock
 * dispatcher's max command/response payload, and is deliberately <= the
 * wire/parser limit SPI_PACKET_V1_MAX_PAYLOAD_WORDS (4096). ECHO accepts up to
 * this many request words; a longer request yields ERR_RSP_OVERFLOW. GET_CAPS
 * advertises this value as its max payload.
 */
#define PKTV1_DISPATCH_MAX_PAYLOAD_WORDS  64U

/* Granular dispatch status / error codes. */
typedef enum
{
    PKTV1_DISPATCH_OK = 0,
    PKTV1_DISPATCH_ERR_NULL,            /* req or rsp NULL, or payload NULL w/ len>0 */
    PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,   /* command_id in a forbidden range          */
    PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD, /* command_id >= 0x8000 but not in catalog   */
    PKTV1_DISPATCH_ERR_BAD_LENGTH,      /* payload length wrong for this command     */
    PKTV1_DISPATCH_ERR_RSP_OVERFLOW     /* response would exceed buffer capacity     */
} PKTV1_DISPATCH_RESULT_e;

/*
 * Pure-C response object. Caller-owned; the dispatcher writes here and keeps no
 * pointer to it. `payload` is a fixed internal buffer - the dispatcher never
 * aliases the request payload (ECHO copies). Valid words are
 * payload[0 .. payload_length_words - 1]. `status` mirrors the return value.
 */
typedef struct
{
    uint16_t                command_id;
    uint16_t                payload_length_words;
    uint16_t                payload[PKTV1_DISPATCH_MAX_PAYLOAD_WORDS];
    PKTV1_DISPATCH_RESULT_e status;
} ST_PKTV1_DISPATCH_RSP;

/*
 * Dispatch one already-parsed, already-CRC-validated packet view (A2 output)
 * into *rsp. Returns the granular status. On any semantic error (forbidden /
 * unsupported / bad length / overflow) it also fills *rsp as a PKTV1_RSP_ERROR
 * response carrying { original command_id, wire error code }.
 *
 * NULL rule (fixed): if rsp == NULL -> ERR_NULL (nothing written). If
 * req == NULL -> ERR_NULL and *rsp is zeroed. If req->payloadWords > 0 but
 * req->payload == NULL -> ERR_NULL and *rsp is zeroed (no error envelope, since
 * the request is malformed at the API level rather than a valid command).
 *
 * Pure: no globals, no I/O, no transport. Identical (req) always yields an
 * identical (*rsp).
 */
PKTV1_DISPATCH_RESULT_e SpiPacketV1_Dispatch(const ST_SPI_PACKET_V1 *req,
                                             ST_PKTV1_DISPATCH_RSP  *rsp);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_DISPATCH_H_ */
