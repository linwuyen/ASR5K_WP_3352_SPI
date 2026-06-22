/*
 * spi_packet_v1_wire_probe.h
 *
 * SPI Packet V1 - A6-3A passive wire recognizer core (pure C).
 *
 * Consumes 16-bit words exactly as if they had arrived from the SPI wire and
 * recognizes Packet V1 frames (header 0xA55A, command id, length, payload,
 * CRC16/CCITT-FALSE). It is PASSIVE and DETECT-ONLY:
 *   - it never produces a response (no PONG, no MISO write),
 *   - it never touches any SPIB/SPIA runtime, DMA, or legacy path,
 *   - it holds no global state; the caller owns the ST_PKTV1_WIRE_PROBE.
 *
 * This module is NOT yet connected to the SPIB runtime (that is a later,
 * separately gated task A6-3B / A7-0). It exists so the recognizer logic can be
 * proven in isolation on the host before any RX/DMA integration.
 *
 * Pure C constraints (mirrors the rest of SPI_PacketV1/):
 *   - depends only on <stdint.h> and the pure-C Packet V1 CRC/format sources;
 *   - no driverlib / device.h / board.h / spi_slave.h / SPI_master.h;
 *   - no cmd_id.h, no hardware registers, no global diagnostics, no malloc,
 *     no stdio in this production source;
 *   - C99 compatible and C28x safe: frame sizing uses explicit word counts,
 *     never sizeof(buffer)/2 (on C28x sizeof(uint16_t) == 1).
 */

#ifndef SPI_PACKET_V1_WIRE_PROBE_H_
#define SPI_PACKET_V1_WIRE_PROBE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fixed overhead = header + command + length + crc (same as the Packet V1
 * format's SPI_PACKET_V1_OVERHEAD_WORDS; restated here so this header has no
 * compile dependency on spi_packet_v1.h).
 */
#define PKTV1_WIRE_PROBE_OVERHEAD_WORDS   4U

/*
 * Recognizer payload cap. Deliberately SMALL and host-test sized (16 words),
 * independent of the Packet V1 spec maximum (4096). A declared length above
 * this cap is rejected as PKTV1_WIRE_PROBE_ERR_BAD_LENGTH. Keeping the cap small
 * keeps the recognizer allocation-free and bounds the candidate frame size.
 */
#define PKTV1_WIRE_PROBE_MAX_PAYLOAD_WORDS 16U

/* Largest candidate frame the recognizer will ever assemble (4 + 16 = 20). */
#define PKTV1_WIRE_PROBE_MAX_FRAME_WORDS \
    (PKTV1_WIRE_PROBE_OVERHEAD_WORDS + PKTV1_WIRE_PROBE_MAX_PAYLOAD_WORDS)

typedef enum
{
    PKTV1_WIRE_PROBE_IDLE = 0,
    PKTV1_WIRE_PROBE_COLLECTING,
    PKTV1_WIRE_PROBE_FRAME_OK,
    PKTV1_WIRE_PROBE_FRAME_ERROR
} PKTV1_WIRE_PROBE_STATE_e;

typedef enum
{
    PKTV1_WIRE_PROBE_OK = 0,
    PKTV1_WIRE_PROBE_ERR_NULL,
    PKTV1_WIRE_PROBE_ERR_OVERFLOW,   /* reserved defensive guard (see .c)      */
    PKTV1_WIRE_PROBE_ERR_BAD_HEADER, /* reserved: header is handled by resync  */
    PKTV1_WIRE_PROBE_ERR_BAD_LENGTH,
    PKTV1_WIRE_PROBE_ERR_BAD_CRC
} PKTV1_WIRE_PROBE_RESULT_e;

/*
 * Recognizer state. Caller-owned, no globals. Pass the same instance to every
 * FeedWord / FeedWords call.
 *
 * Field notes:
 *   state                 - current FSM state (see enum).
 *   result                - latest definitive result (OK, or an ERR_* reason).
 *                           Stays OK while a frame is still being collected.
 *   words_seen            - words consumed for the current candidate frame
 *                           (1 right after the header word; 0 between frames).
 *   expected_total_words  - 4 + payload_words, set once the length word is seen
 *                           (0 before that).
 *   cmd_id                - command id (valid once the command word is seen).
 *   payload_words         - declared payload length in 16-bit words.
 *   crc_expected          - CRC word taken from the wire (the final frame word).
 *   crc_actual            - CRC16/CCITT-FALSE computed over header+cmd+length+
 *                           payload. During COLLECTING this holds the running
 *                           value; at completion it is the full covered CRC.
 *   frame_ok_count        - count of CRC-valid frames recognized.
 *   frame_error_count     - count of malformed frames rejected.
 */
typedef struct
{
    PKTV1_WIRE_PROBE_STATE_e  state;
    PKTV1_WIRE_PROBE_RESULT_e result;
    uint16_t words_seen;
    uint16_t expected_total_words;
    uint16_t cmd_id;
    uint16_t payload_words;
    uint16_t crc_expected;
    uint16_t crc_actual;
    uint16_t frame_ok_count;
    uint16_t frame_error_count;
} ST_PKTV1_WIRE_PROBE;

/*
 * Full reset: clears state, result, the in-progress frame fields, AND the
 * frame_ok_count / frame_error_count counters. NULL-safe (a NULL p is ignored).
 */
void SpiPacketV1_WireProbe_Reset(ST_PKTV1_WIRE_PROBE *p);

/*
 * Feed one wire word.
 *
 * Return value reflects THIS word's outcome:
 *   PKTV1_WIRE_PROBE_OK        - word accepted with no error. This covers both
 *                                "frame still collecting" and "frame just
 *                                completed OK". Inspect p->state to tell them
 *                                apart (COLLECTING vs FRAME_OK).
 *   PKTV1_WIRE_PROBE_ERR_NULL  - p is NULL.
 *   PKTV1_WIRE_PROBE_ERR_BAD_LENGTH - declared length exceeds the cap.
 *   PKTV1_WIRE_PROBE_ERR_BAD_CRC    - CRC word did not match.
 *
 * Between frames (IDLE / FRAME_OK / FRAME_ERROR) a non-header word is ignored
 * and returns OK; a 0xA55A word begins a new candidate frame.
 */
PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWord(
    ST_PKTV1_WIRE_PROBE *p, uint16_t word);

/*
 * Feed word_count words from words[]. Stops and returns early on the first word
 * that produces a definitive error (ERR_*); otherwise returns the result of the
 * final word fed. NULL p or NULL words (with word_count > 0) returns ERR_NULL.
 */
PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWords(
    ST_PKTV1_WIRE_PROBE *p, const uint16_t *words, uint16_t word_count);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_WIRE_PROBE_H_ */
