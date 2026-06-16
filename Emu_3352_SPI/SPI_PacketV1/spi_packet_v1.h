/*
 * spi_packet_v1.h
 *
 * SPI Packet V1 - self-describing, CRC-protected frame (pure C).
 *
 * Frame layout (array of 16-bit words):
 *   w0        Header / Magic  = 0xA55A
 *   w1        Command ID      (uint16_t)
 *   w2        Data Length     = N, payload length in 16-bit words
 *   w3..3+N-1 Payload         (N words)
 *   w3+N      CRC16           CRC16/CCITT-FALSE over w0..2+N
 *
 *   total_words = 4 + N   (4 overhead words: header, cmd, length, crc)
 *
 * Constraints (see docs/SPI_PACKET_V1_SPEC.md):
 *   - pure C, depends only on <stdint.h> (+ spi_packet_crc16.h)
 *   - no driverlib / device.h / spi_slave.h / wave_download.h
 *   - no legacy runtime calls, no global/watch state (caller owns all buffers)
 */

#ifndef SPI_PACKET_V1_H_
#define SPI_PACKET_V1_H_

#include <stdint.h>
#include "spi_packet_crc16.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed frame constants. */
#define SPI_PACKET_V1_HEADER_MAGIC      0xA55AU
#define SPI_PACKET_V1_OVERHEAD_WORDS    4U      /* header + cmd + length + crc */

/*
 * Minimum valid frame = overhead only (zero payload). Any wordCount below this
 * is rejected as SPI_PACKET_V1_ERR_TRUNCATED by the minimum-length gate
 * (parser validation step 2), regardless of which field appears "missing".
 */
#define SPI_PACKET_V1_MIN_WORDS         SPI_PACKET_V1_OVERHEAD_WORDS

/*
 * Maximum payload, in 16-bit words. This is the Packet V1 parser/spec limit and
 * is deliberately independent of the legacy SIZE_OF_SPI_BLOCK_RAM (4095): the
 * pure-C packet format is not bound to any legacy buffer size. A future SPIB
 * runtime adapter that is constrained by DMA buffer or legacy block size must
 * segment or reject at the adapter layer, never by shrinking this limit. See
 * docs/SPI_PACKET_V1_SPEC.md section 5 / 12.
 */
#define SPI_PACKET_V1_MAX_PAYLOAD_WORDS 4096U

/* Word offsets of the fixed header fields. */
#define SPI_PACKET_V1_OFFSET_HEADER     0U
#define SPI_PACKET_V1_OFFSET_CMD        1U
#define SPI_PACKET_V1_OFFSET_LENGTH     2U
#define SPI_PACKET_V1_OFFSET_PAYLOAD    3U

/* Total frame word count for a payload of n words. */
#define SPI_PACKET_V1_FRAME_WORDS(n) \
    ((uint16_t)((n) + SPI_PACKET_V1_OVERHEAD_WORDS))

typedef enum
{
    SPI_PACKET_V1_OK = 0,
    SPI_PACKET_V1_ERR_NULL_ARG,
    SPI_PACKET_V1_ERR_BAD_HEADER,
    SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
    SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
    SPI_PACKET_V1_ERR_CRC_MISMATCH,
    SPI_PACKET_V1_ERR_BUFFER_TOO_SMALL,
    SPI_PACKET_V1_ERR_TRUNCATED,
    /* A2 streaming parser: frame not yet complete (more words expected). */
    SPI_PACKET_V1_IN_PROGRESS
} SPI_PACKET_V1_RESULT_e;

/*
 * Parsed view of a frame. payload aliases into the source buffer supplied to
 * SpiPacketV1_ParseWords (no copy); it is NULL when payloadWords == 0.
 * Contents are only valid when the parse returns SPI_PACKET_V1_OK.
 */
typedef struct
{
    uint16_t        cmdId;
    uint16_t        payloadWords;
    const uint16_t *payload;
    uint16_t        crc;
} ST_SPI_PACKET_V1;

/*
 * Encode a frame into outWords.
 *   cmdId        : command id placed in w1
 *   payload      : N payload words (may be NULL only when payloadWords == 0)
 *   payloadWords : N
 *   outWords     : destination buffer (required, non-NULL)
 *   outCapacity  : capacity of outWords, in words
 *   outLen       : required (non-NULL); receives total frame word count.
 *                  A NULL outLen returns SPI_PACKET_V1_ERR_NULL_ARG (A1).
 * Returns SPI_PACKET_V1_OK on success; see header for error codes.
 */
SPI_PACKET_V1_RESULT_e SpiPacketV1_Encode(uint16_t        cmdId,
                                          const uint16_t *payload,
                                          uint16_t        payloadWords,
                                          uint16_t       *outWords,
                                          uint16_t        outCapacity,
                                          uint16_t       *outLen);

/*
 * Parse and validate a frame from words[0..wordCount-1] into *outPkt.
 * Checks (in order): args, min length, header, declared length, exact length,
 * CRC. Returns SPI_PACKET_V1_OK on success; otherwise a specific error code and
 * *outPkt is zeroed.
 */
SPI_PACKET_V1_RESULT_e SpiPacketV1_ParseWords(const uint16_t   *words,
                                              uint16_t          wordCount,
                                              ST_SPI_PACKET_V1 *outPkt);

/* ====================================================================== */
/* A2 - Streaming / incremental-feed parser                               */
/*                                                                        */
/* Same wire format and CRC coverage as the total-buffer parser, but the  */
/* caller feeds one 16-bit word at a time. State lives entirely in a       */
/* caller-owned SpiPacketV1_StreamParser (no malloc, no globals). The CRC  */
/* is accumulated incrementally over Header + Command ID + Data Length +   */
/* Payload (the CRC word itself is excluded), identical to ParseWords.     */
/* ====================================================================== */

typedef enum
{
    SPI_PACKET_V1_STREAM_WAIT_HEADER = 0,
    SPI_PACKET_V1_STREAM_WAIT_COMMAND,
    SPI_PACKET_V1_STREAM_WAIT_LENGTH,
    SPI_PACKET_V1_STREAM_WAIT_PAYLOAD,
    SPI_PACKET_V1_STREAM_WAIT_CRC
} SpiPacketV1_StreamState;

/*
 * Streaming parser state. Caller-owned; pass the same instance to every
 * SpiPacketV1_StreamFeedWord call. The payload buffer is embedded for
 * correctness-first simplicity (A2): the struct is ~8 KB, so prefer static
 * or heap-free long-lived storage rather than a deep stack frame.
 */
typedef struct
{
    SpiPacketV1_StreamState state;
    uint16_t command_id;
    uint16_t payload_words;   /* declared N (valid once past WAIT_LENGTH)   */
    uint16_t payload_index;   /* payload words received so far              */
    uint16_t crc;             /* running CRC over header+cmd+length+payload */
    uint16_t payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS];
} SpiPacketV1_StreamParser;

/*
 * Initialise / reset a streaming parser to WAIT_HEADER. Both are NULL-safe
 * (a NULL parser is ignored). Reset is also performed automatically after a
 * completed packet and after any error result.
 */
void SpiPacketV1_StreamInit(SpiPacketV1_StreamParser *parser);
void SpiPacketV1_StreamReset(SpiPacketV1_StreamParser *parser);

/*
 * Feed one 16-bit word. Returns:
 *   SPI_PACKET_V1_IN_PROGRESS         - word accepted, frame not yet complete
 *   SPI_PACKET_V1_OK                  - frame complete; *out_packet committed,
 *                                       parser auto-reset to WAIT_HEADER
 *   SPI_PACKET_V1_ERR_NULL_ARG        - parser or out_packet is NULL
 *   SPI_PACKET_V1_ERR_BAD_HEADER      - first word != 0xA55A; stays WAIT_HEADER
 *                                       (ready to resync on the next word)
 *   SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE - declared length > MAX; parser reset
 *   SPI_PACKET_V1_ERR_CRC_MISMATCH    - CRC word mismatch; parser reset
 *
 * out_packet is required on every call and is written only on OK. On OK,
 * out_packet->payload aliases the parser's internal buffer (NULL if empty);
 * it stays valid until the next packet's payload overwrites it.
 */
SPI_PACKET_V1_RESULT_e SpiPacketV1_StreamFeedWord(
    SpiPacketV1_StreamParser *parser,
    uint16_t                  word,
    ST_SPI_PACKET_V1         *out_packet);

/*
 * Finalise a stream (e.g. end of transfer). Returns:
 *   SPI_PACKET_V1_OK            - parser was idle (WAIT_HEADER); nothing pending
 *   SPI_PACKET_V1_ERR_TRUNCATED - a partial frame was in flight; parser reset
 *   SPI_PACKET_V1_ERR_NULL_ARG  - parser is NULL
 */
SPI_PACKET_V1_RESULT_e SpiPacketV1_StreamFinalize(
    SpiPacketV1_StreamParser *parser);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_H_ */
