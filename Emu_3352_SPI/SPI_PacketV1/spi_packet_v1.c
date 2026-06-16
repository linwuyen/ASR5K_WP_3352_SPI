/*
 * spi_packet_v1.c
 *
 * SPI Packet V1 - encoder + parser reference implementation (pure C).
 * See spi_packet_v1.h and docs/SPI_PACKET_V1_SPEC.md.
 *
 * No side effects, no global state, no target dependencies. The caller owns
 * every buffer; the parser aliases (never copies) the input payload.
 */

#include "spi_packet_v1.h"

SPI_PACKET_V1_RESULT_e SpiPacketV1_Encode(uint16_t        cmdId,
                                          const uint16_t *payload,
                                          uint16_t        payloadWords,
                                          uint16_t       *outWords,
                                          uint16_t        outCapacity,
                                          uint16_t       *outLen)
{
    uint16_t frameWords;
    uint16_t i;
    uint16_t crc;

    if (outWords == 0)
    {
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }
    if (outLen == 0)
    {
        /* outLen is a required output (A1): the encoder always reports the
         * frame size, so a NULL length sink is a caller error. */
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }
    if ((payload == 0) && (payloadWords > 0U))
    {
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }
    if (payloadWords > SPI_PACKET_V1_MAX_PAYLOAD_WORDS)
    {
        return SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE;
    }

    frameWords = SPI_PACKET_V1_FRAME_WORDS(payloadWords);
    if (outCapacity < frameWords)
    {
        return SPI_PACKET_V1_ERR_BUFFER_TOO_SMALL;
    }

    /* Fixed header. */
    outWords[SPI_PACKET_V1_OFFSET_HEADER] = SPI_PACKET_V1_HEADER_MAGIC;
    outWords[SPI_PACKET_V1_OFFSET_CMD]    = cmdId;
    outWords[SPI_PACKET_V1_OFFSET_LENGTH] = payloadWords;

    /* Payload. */
    for (i = 0U; i < payloadWords; ++i)
    {
        outWords[SPI_PACKET_V1_OFFSET_PAYLOAD + i] = payload[i];
    }

    /* CRC over header + cmd + length + payload (everything but the CRC word). */
    crc = SpiPacketCrc16_ComputeWords(outWords,
                                      (uint16_t)(SPI_PACKET_V1_OFFSET_PAYLOAD
                                                 + payloadWords));
    outWords[SPI_PACKET_V1_OFFSET_PAYLOAD + payloadWords] = crc;

    *outLen = frameWords;
    return SPI_PACKET_V1_OK;
}

SPI_PACKET_V1_RESULT_e SpiPacketV1_ParseWords(const uint16_t   *words,
                                              uint16_t          wordCount,
                                              ST_SPI_PACKET_V1 *outPkt)
{
    uint16_t declaredLen;
    uint16_t coveredWords;
    uint16_t calcCrc;
    uint16_t frameCrc;

    if ((words == 0) || (outPkt == 0))
    {
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }

    /* Zero the output so a rejected frame never leaves stale data behind. */
    outPkt->cmdId        = 0U;
    outPkt->payloadWords = 0U;
    outPkt->payload      = 0;
    outPkt->crc          = 0U;

    /* Too short to even hold header + cmd + length + crc. */
    if (wordCount < SPI_PACKET_V1_OVERHEAD_WORDS)
    {
        return SPI_PACKET_V1_ERR_TRUNCATED;
    }

    /* Header / magic. */
    if (words[SPI_PACKET_V1_OFFSET_HEADER] != SPI_PACKET_V1_HEADER_MAGIC)
    {
        return SPI_PACKET_V1_ERR_BAD_HEADER;
    }

    /* Declared payload length sanity (before trusting it for sizing). */
    declaredLen = words[SPI_PACKET_V1_OFFSET_LENGTH];
    if (declaredLen > SPI_PACKET_V1_MAX_PAYLOAD_WORDS)
    {
        return SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE;
    }

    /* Frame must be exactly the size its length field declares. */
    if (wordCount != SPI_PACKET_V1_FRAME_WORDS(declaredLen))
    {
        return SPI_PACKET_V1_ERR_LENGTH_MISMATCH;
    }

    /* CRC over header + cmd + length + payload; compare with the last word. */
    coveredWords = (uint16_t)(SPI_PACKET_V1_OFFSET_PAYLOAD + declaredLen);
    calcCrc      = SpiPacketCrc16_ComputeWords(words, coveredWords);
    frameCrc     = words[coveredWords];
    if (calcCrc != frameCrc)
    {
        return SPI_PACKET_V1_ERR_CRC_MISMATCH;
    }

    /* Accept. payload aliases into the caller's buffer. */
    outPkt->cmdId        = words[SPI_PACKET_V1_OFFSET_CMD];
    outPkt->payloadWords = declaredLen;
    outPkt->payload      = (declaredLen > 0U)
                               ? &words[SPI_PACKET_V1_OFFSET_PAYLOAD]
                               : 0;
    outPkt->crc          = frameCrc;
    return SPI_PACKET_V1_OK;
}

/* ====================================================================== */
/* A2 - Streaming / incremental-feed parser                               */
/*                                                                        */
/* Word-at-a-time state machine. CRC is accumulated incrementally over    */
/* Header + Command ID + Data Length + Payload (CRC word excluded), so the */
/* running value at WAIT_CRC equals SpiPacketCrc16_ComputeWords() over the */
/* same span used by the total-buffer parser. State and the payload buffer */
/* live entirely in the caller-owned parser struct (no malloc, no globals).*/
/* ====================================================================== */

void SpiPacketV1_StreamReset(SpiPacketV1_StreamParser *parser)
{
    if (parser == 0)
    {
        return;   /* NULL-safe (A2 defensive requirement). */
    }
    parser->state         = SPI_PACKET_V1_STREAM_WAIT_HEADER;
    parser->command_id    = 0U;
    parser->payload_words = 0U;
    parser->payload_index = 0U;
    parser->crc           = SPI_PACKET_CRC16_INIT;
}

void SpiPacketV1_StreamInit(SpiPacketV1_StreamParser *parser)
{
    /* Init == Reset: bring the parser to WAIT_HEADER. The payload buffer is
     * left untouched; it is only read back via out_packet after completion. */
    SpiPacketV1_StreamReset(parser);
}

SPI_PACKET_V1_RESULT_e SpiPacketV1_StreamFeedWord(
    SpiPacketV1_StreamParser *parser,
    uint16_t                  word,
    ST_SPI_PACKET_V1         *out_packet)
{
    /* out_packet is required on every call (A2): a NULL sink is a caller error
     * and leaves the parser state untouched (the word is not consumed). */
    if ((parser == 0) || (out_packet == 0))
    {
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }

    switch (parser->state)
    {
    case SPI_PACKET_V1_STREAM_WAIT_HEADER:
        if (word != SPI_PACKET_V1_HEADER_MAGIC)
        {
            /* Stay in WAIT_HEADER so the next word can be a fresh header
             * (stream resync without an explicit reset). */
            return SPI_PACKET_V1_ERR_BAD_HEADER;
        }
        parser->crc   = SpiPacketCrc16_UpdateWord(SPI_PACKET_CRC16_INIT, word);
        parser->state = SPI_PACKET_V1_STREAM_WAIT_COMMAND;
        return SPI_PACKET_V1_IN_PROGRESS;

    case SPI_PACKET_V1_STREAM_WAIT_COMMAND:
        parser->command_id = word;
        parser->crc        = SpiPacketCrc16_UpdateWord(parser->crc, word);
        parser->state      = SPI_PACKET_V1_STREAM_WAIT_LENGTH;
        return SPI_PACKET_V1_IN_PROGRESS;

    case SPI_PACKET_V1_STREAM_WAIT_LENGTH:
        if (word > SPI_PACKET_V1_MAX_PAYLOAD_WORDS)
        {
            SpiPacketV1_StreamReset(parser);
            return SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE;
        }
        parser->payload_words = word;
        parser->payload_index = 0U;
        parser->crc           = SpiPacketCrc16_UpdateWord(parser->crc, word);
        parser->state = (word == 0U) ? SPI_PACKET_V1_STREAM_WAIT_CRC
                                     : SPI_PACKET_V1_STREAM_WAIT_PAYLOAD;
        return SPI_PACKET_V1_IN_PROGRESS;

    case SPI_PACKET_V1_STREAM_WAIT_PAYLOAD:
        parser->payload[parser->payload_index] = word;
        parser->crc = SpiPacketCrc16_UpdateWord(parser->crc, word);
        parser->payload_index++;
        if (parser->payload_index >= parser->payload_words)
        {
            parser->state = SPI_PACKET_V1_STREAM_WAIT_CRC;
        }
        return SPI_PACKET_V1_IN_PROGRESS;

    case SPI_PACKET_V1_STREAM_WAIT_CRC:
    default:
        if (word != parser->crc)
        {
            SpiPacketV1_StreamReset(parser);
            return SPI_PACKET_V1_ERR_CRC_MISMATCH;
        }
        /* Frame complete: commit only now, then auto-reset for the next one.
         * payload aliases the parser buffer (NULL when empty), valid until the
         * next packet's payload overwrites it. */
        out_packet->cmdId        = parser->command_id;
        out_packet->payloadWords = parser->payload_words;
        out_packet->payload      = (parser->payload_words > 0U)
                                       ? parser->payload
                                       : 0;
        out_packet->crc          = word;
        SpiPacketV1_StreamReset(parser);
        return SPI_PACKET_V1_OK;
    }
}

SPI_PACKET_V1_RESULT_e SpiPacketV1_StreamFinalize(
    SpiPacketV1_StreamParser *parser)
{
    if (parser == 0)
    {
        return SPI_PACKET_V1_ERR_NULL_ARG;
    }
    if (parser->state == SPI_PACKET_V1_STREAM_WAIT_HEADER)
    {
        return SPI_PACKET_V1_OK;   /* idle: no frame in flight */
    }
    /* A partial frame was mid-stream; report and reset for resync. */
    SpiPacketV1_StreamReset(parser);
    return SPI_PACKET_V1_ERR_TRUNCATED;
}
