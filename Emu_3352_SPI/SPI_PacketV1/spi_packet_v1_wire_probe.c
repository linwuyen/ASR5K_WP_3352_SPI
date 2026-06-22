/*
 * spi_packet_v1_wire_probe.c
 *
 * SPI Packet V1 - A6-3A passive wire recognizer core (pure C).
 * See spi_packet_v1_wire_probe.h.
 *
 * The CRC is accumulated incrementally with SpiPacketCrc16_UpdateWord() from
 * SPI_PACKET_CRC16_INIT over Header + Command + Length + Payload (the CRC word
 * itself is excluded), so the running value equals SpiPacketCrc16_ComputeWords()
 * over the same span used by the A2 total-buffer and streaming parsers.
 *
 * No globals, no malloc, no stdio, no hardware/runtime dependency.
 */

#include "spi_packet_v1_wire_probe.h"

#include "spi_packet_crc16.h"  /* SpiPacketCrc16_UpdateWord, SPI_PACKET_CRC16_INIT */
#include "spi_packet_v1.h"     /* SPI_PACKET_V1_HEADER_MAGIC (single source)       */

/* Word indices within a candidate frame. */
#define PKTV1_WIRE_PROBE_IDX_HEADER   0U
#define PKTV1_WIRE_PROBE_IDX_CMD      1U
#define PKTV1_WIRE_PROBE_IDX_LENGTH   2U
#define PKTV1_WIRE_PROBE_IDX_PAYLOAD  3U

void SpiPacketV1_WireProbe_Reset(ST_PKTV1_WIRE_PROBE *p)
{
    if (p == 0)
    {
        return;   /* NULL-safe */
    }
    p->state                = PKTV1_WIRE_PROBE_IDLE;
    p->result               = PKTV1_WIRE_PROBE_OK;
    p->words_seen           = 0U;
    p->expected_total_words = 0U;
    p->cmd_id               = 0U;
    p->payload_words        = 0U;
    p->crc_expected         = 0U;
    p->crc_actual           = 0U;
    p->frame_ok_count       = 0U;
    p->frame_error_count    = 0U;
}

/* Begin a fresh candidate frame on a header word. Counters are NOT touched. */
static void wireProbeBeginFrame(ST_PKTV1_WIRE_PROBE *p, uint16_t headerWord)
{
    p->state                = PKTV1_WIRE_PROBE_COLLECTING;
    p->result               = PKTV1_WIRE_PROBE_OK;
    p->words_seen           = 1U;   /* header consumed */
    p->expected_total_words = 0U;   /* set once length word arrives */
    p->cmd_id               = 0U;
    p->payload_words        = 0U;
    p->crc_expected         = 0U;
    p->crc_actual = SpiPacketCrc16_UpdateWord(SPI_PACKET_CRC16_INIT, headerWord);
}

PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWord(
    ST_PKTV1_WIRE_PROBE *p, uint16_t word)
{
    uint16_t idx;

    if (p == 0)
    {
        return PKTV1_WIRE_PROBE_ERR_NULL;
    }

    /* Between frames: ignore noise, resync on a header word. */
    if (p->state != PKTV1_WIRE_PROBE_COLLECTING)
    {
        if (word == SPI_PACKET_V1_HEADER_MAGIC)
        {
            wireProbeBeginFrame(p, word);
        }
        /* else: non-header word ignored, state and counters unchanged. */
        return PKTV1_WIRE_PROBE_OK;
    }

    /* COLLECTING: idx is the 0-based position of this word (header was idx 0). */
    idx = p->words_seen;

    if (idx == PKTV1_WIRE_PROBE_IDX_CMD)
    {
        p->cmd_id     = word;
        p->crc_actual = SpiPacketCrc16_UpdateWord(p->crc_actual, word);
        p->words_seen = 2U;
        return PKTV1_WIRE_PROBE_OK;
    }

    if (idx == PKTV1_WIRE_PROBE_IDX_LENGTH)
    {
        p->payload_words = word;   /* record the declared length for diagnostics */
        if (word > PKTV1_WIRE_PROBE_MAX_PAYLOAD_WORDS)
        {
            p->state  = PKTV1_WIRE_PROBE_FRAME_ERROR;
            p->result = PKTV1_WIRE_PROBE_ERR_BAD_LENGTH;
            p->frame_error_count++;
            return PKTV1_WIRE_PROBE_ERR_BAD_LENGTH;
        }
        p->expected_total_words =
            (uint16_t)(PKTV1_WIRE_PROBE_OVERHEAD_WORDS + word);
        p->crc_actual = SpiPacketCrc16_UpdateWord(p->crc_actual, word);
        p->words_seen = 3U;
        return PKTV1_WIRE_PROBE_OK;
    }

    /* idx >= 3: payload words then the final CRC word. */
    if (idx < (uint16_t)(p->expected_total_words - 1U))
    {
        /* Payload word: cover by CRC, do not store. */
        p->crc_actual = SpiPacketCrc16_UpdateWord(p->crc_actual, word);
        p->words_seen = (uint16_t)(p->words_seen + 1U);
        return PKTV1_WIRE_PROBE_OK;
    }

    /* idx == expected_total_words - 1: CRC word (frame complete). */
    p->crc_expected = word;
    p->words_seen   = (uint16_t)(p->words_seen + 1U);
    if (p->crc_actual == p->crc_expected)
    {
        p->state  = PKTV1_WIRE_PROBE_FRAME_OK;
        p->result = PKTV1_WIRE_PROBE_OK;
        p->frame_ok_count++;
        return PKTV1_WIRE_PROBE_OK;
    }

    p->state  = PKTV1_WIRE_PROBE_FRAME_ERROR;
    p->result = PKTV1_WIRE_PROBE_ERR_BAD_CRC;
    p->frame_error_count++;
    return PKTV1_WIRE_PROBE_ERR_BAD_CRC;
}

PKTV1_WIRE_PROBE_RESULT_e SpiPacketV1_WireProbe_FeedWords(
    ST_PKTV1_WIRE_PROBE *p, const uint16_t *words, uint16_t word_count)
{
    uint16_t i;
    PKTV1_WIRE_PROBE_RESULT_e rc = PKTV1_WIRE_PROBE_OK;

    if (p == 0)
    {
        return PKTV1_WIRE_PROBE_ERR_NULL;
    }
    if ((words == 0) && (word_count > 0U))
    {
        return PKTV1_WIRE_PROBE_ERR_NULL;
    }
    /* word_count == 0 is a valid no-op (OK), even when words == NULL. */

    for (i = 0U; i < word_count; ++i)
    {
        rc = SpiPacketV1_WireProbe_FeedWord(p, words[i]);
        if (rc != PKTV1_WIRE_PROBE_OK)
        {
            return rc;   /* surface a definitive error immediately */
        }
    }
    return rc;
}
