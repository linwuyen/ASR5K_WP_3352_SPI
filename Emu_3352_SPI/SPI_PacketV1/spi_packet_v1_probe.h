/*
 * spi_packet_v1_probe.h
 *
 * SPI Packet V1 - board-observable PING probe (A6-1, pure C).
 *
 * Builds a Packet V1 PING request in memory, runs it through the proven A2/A4/A5
 * path (encode -> loopback[parse->dispatch->encode] -> parse response), and
 * validates the PONG reply. The result is a small caller-owned struct that the
 * existing SPI selftest can surface through g_asr5kSpiSelfTest (UART / CCS Watch).
 *
 * Purpose: prove the Packet V1 logic executes correctly on the TARGET C28x CPU
 * (real TI compiler, CHAR_BIT==16) and is board-observable.
 *
 * Limitation: this validates Packet V1 LOGIC ON TARGET ONLY. It does NOT test
 * SPI wire transport and does NOT route through the SPIB RX/DMA path or the
 * existing 0xA55A runtime packet state machine.
 *
 * Hard rules:
 *   - pure C; depends only on <stdint.h> and the Packet V1 A2/A4/A5 headers
 *   - no driverlib / device.h / board.h, no SPIB/SPIA include, no cmd_id.h
 *   - no globals; caller owns the output struct
 */

#ifndef SPI_PACKET_V1_PROBE_H_
#define SPI_PACKET_V1_PROBE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PKTV1_PROBE_OK = 0,
    PKTV1_PROBE_ERR_NULL,         /* out == NULL                              */
    PKTV1_PROBE_ERR_ENCODE,       /* could not encode the PING request        */
    PKTV1_PROBE_ERR_LOOPBACK,     /* loopback did not return OK               */
    PKTV1_PROBE_ERR_PARSE_RSP,    /* response frame failed to parse           */
    PKTV1_PROBE_ERR_BAD_RSP_CMD,  /* response command_id != PKTV1_CMD_PING    */
    PKTV1_PROBE_ERR_BAD_RSP_LEN,  /* response payload length != 1             */
    PKTV1_PROBE_ERR_BAD_PONG      /* response payload[0] != PKTV1_PONG_WORD   */
} PKTV1_PROBE_RESULT_e;

/*
 * Compact, caller-owned probe result. Code fields are uint16_t copies of the
 * underlying enums so the struct is a flat value type (easy to read in a
 * debugger / encode into a selftest result word).
 */
typedef struct
{
    PKTV1_PROBE_RESULT_e result;
    uint16_t             loopback_result;        /* PKTV1_LOOPBACK_RESULT_e   */
    uint16_t             parser_result;          /* request parse (A2)        */
    uint16_t             dispatch_result;        /* PKTV1_DISPATCH_RESULT_e   */
    uint16_t             response_cmd;           /* expect PKTV1_CMD_PING      */
    uint16_t             response_payload_words; /* expect 1                  */
    uint16_t             pong_word;              /* expect PKTV1_PONG_WORD     */
    uint16_t             request_words;          /* PING request frame length */
    uint16_t             response_words;         /* PONG response frame length */
} ST_PKTV1_PROBE_RESULT;

/*
 * Run the in-memory PING probe. Returns PKTV1_PROBE_OK on full success and
 * fills *out with the per-stage detail. If out == NULL, returns
 * PKTV1_PROBE_ERR_NULL and writes nothing. Pure: no globals, deterministic.
 */
PKTV1_PROBE_RESULT_e SpiPacketV1_RunPingProbe(ST_PKTV1_PROBE_RESULT *out);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_PROBE_H_ */
