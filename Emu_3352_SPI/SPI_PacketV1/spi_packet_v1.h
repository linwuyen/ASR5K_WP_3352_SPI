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
    SPI_PACKET_V1_ERR_TRUNCATED
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
 *   outWords     : destination buffer
 *   outCapacity  : capacity of outWords, in words
 *   outLen       : (optional, may be NULL) receives total frame word count
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

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_H_ */
