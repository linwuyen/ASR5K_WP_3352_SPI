/*
 * spi_packet_v1_test.c
 *
 * SPI Packet V1 - standalone pure-C test harness.
 *
 * This is a HOST test, not firmware. The entire harness (including main() and
 * <stdio.h>) is compiled only when SPI_PACKET_V1_HOST_TEST is defined, so that
 * when the file happens to sit in the CCS firmware build it contributes no
 * second main() and pulls in no CIO/printf. Build and run on a host PC, e.g.:
 *
 *   gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
 *       spi_packet_crc16.c spi_packet_v1.c spi_packet_v1_test.c \
 *       -o spi_packet_v1_test && ./spi_packet_v1_test
 *
 * Exit code 0 = all cases pass; non-zero = at least one failure.
 * Covers the Pure-C Test Matrix in docs/SPI_PACKET_V1_SPEC.md section 11.
 */

/* Guarantee a non-empty translation unit in the firmware build. */
typedef int spi_packet_v1_test_translation_unit_t;

#ifdef SPI_PACKET_V1_HOST_TEST

#include <stdint.h>
#include <stdio.h>

#include "spi_packet_crc16.h"
#include "spi_packet_v1.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(int cond, const char *name)
{
    if (cond)
    {
        ++g_pass;
        printf("[PASS] %s\n", name);
    }
    else
    {
        ++g_fail;
        printf("[FAIL] %s\n", name);
    }
}

/* ------------------------------------------------------------------ */
/* Test 1: CRC self-check "123456789" -> 0x29B1                        */
/* ------------------------------------------------------------------ */
static void test_crc_check_vector(void)
{
    /* uint16_t (not uint8_t): C28x has no 8-bit type; low 8 bits are used. */
    static const uint16_t vec[9] = { '1', '2', '3', '4', '5',
                                     '6', '7', '8', '9' };
    uint16_t crc = SpiPacketCrc16_ComputeBytes(vec, 9U);

    check(crc == SPI_PACKET_CRC16_CHECK,
          "CRC16 \"123456789\" == 0x29B1");
    check(SPI_PACKET_CRC16_CHECK == 0x29B1U,
          "CRC16 check constant == 0x29B1");
}

/* ------------------------------------------------------------------ */
/* Test 2: encode empty payload                                        */
/* ------------------------------------------------------------------ */
static void test_encode_empty(void)
{
    uint16_t frame[8];
    uint16_t len = 0xFFFFU;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t expectCrc;

    r = SpiPacketV1_Encode(0x1234U, NULL, 0U, frame, 8U, &len);

    check(r == SPI_PACKET_V1_OK,               "encode empty: returns OK");
    check(len == 4U,                           "encode empty: len == 4");
    check(frame[0] == SPI_PACKET_V1_HEADER_MAGIC, "encode empty: header 0xA55A");
    check(frame[1] == 0x1234U,                 "encode empty: cmd id stored");
    check(frame[2] == 0x0000U,                 "encode empty: length 0");

    /* CRC over header+cmd+length (3 words). */
    expectCrc = SpiPacketCrc16_ComputeWords(frame, 3U);
    check(frame[3] == expectCrc,               "encode empty: CRC matches");
}

/* ------------------------------------------------------------------ */
/* Test 3: parse empty payload                                         */
/* ------------------------------------------------------------------ */
static void test_parse_empty(void)
{
    uint16_t frame[8];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x00AAU, NULL, 0U, frame, 8U, &len);

    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_OK,           "parse empty: returns OK");
    check(pkt.cmdId == 0x00AAU,            "parse empty: cmd id recovered");
    check(pkt.payloadWords == 0U,          "parse empty: payloadWords == 0");
    check(pkt.payload == NULL,             "parse empty: payload == NULL");
}

/* ------------------------------------------------------------------ */
/* Test 4 + 9: encode/parse 3-word payload round-trip                  */
/* ------------------------------------------------------------------ */
static void test_roundtrip_3word(void)
{
    static const uint16_t payload[3] = { 0xDEADU, 0xBEEFU, 0x0042U };
    uint16_t frame[16];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e re, rp;
    int payloadOk;

    re = SpiPacketV1_Encode(0x0100U, payload, 3U, frame, 16U, &len);
    check(re == SPI_PACKET_V1_OK,          "encode 3w: returns OK");
    check(len == 7U,                       "encode 3w: len == 7 (4 + 3)");

    rp = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(rp == SPI_PACKET_V1_OK,          "parse 3w: returns OK");
    check(pkt.cmdId == 0x0100U,            "parse 3w: cmd id recovered");
    check(pkt.payloadWords == 3U,          "parse 3w: payloadWords == 3");

    payloadOk = (pkt.payload != NULL)
                && (pkt.payload[0] == 0xDEADU)
                && (pkt.payload[1] == 0xBEEFU)
                && (pkt.payload[2] == 0x0042U);
    check(payloadOk,                       "round-trip: payload preserved");
}

/* ------------------------------------------------------------------ */
/* Test 5: bad header reject                                           */
/* ------------------------------------------------------------------ */
static void test_bad_header(void)
{
    uint16_t frame[8];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x0001U, NULL, 0U, frame, 8U, &len);
    frame[0] = 0x1234U;   /* corrupt the magic */

    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER, "bad header: rejected");
}

/* ------------------------------------------------------------------ */
/* Test 6: length mismatch reject                                      */
/* ------------------------------------------------------------------ */
static void test_length_mismatch(void)
{
    static const uint16_t payload[2] = { 0x1111U, 0x2222U };
    uint16_t frame[16];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x0002U, payload, 2U, frame, 16U, &len);
    /* Declared length says 2, but hand the parser one word too few. */
    r = SpiPacketV1_ParseWords(frame, (uint16_t)(len - 1U), &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "length mismatch: short frame rejected");

    /* Length field tampered to declare more payload than supplied. */
    frame[2] = 5U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "length mismatch: tampered length rejected");
}

/* ------------------------------------------------------------------ */
/* Test 7: CRC mismatch reject                                         */
/* ------------------------------------------------------------------ */
static void test_crc_mismatch(void)
{
    static const uint16_t payload[3] = { 0x0AAAU, 0x0BBBU, 0x0CCCU };
    uint16_t frame[16];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x0003U, payload, 3U, frame, 16U, &len);
    frame[4] ^= 0x0001U;   /* flip one payload bit, leave CRC stale */

    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "crc mismatch: rejected");
}

/* ------------------------------------------------------------------ */
/* Test 8: payload too large reject (encode + parse)                   */
/* ------------------------------------------------------------------ */
static void test_payload_too_large(void)
{
    uint16_t header[8];
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t dummyPayload[1] = { 0U };
    uint16_t len = 0U;

    /* Encode side: declared payload exceeds the cap. */
    r = SpiPacketV1_Encode(0x0004U, dummyPayload,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U),
                           header, 8U, &len);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "payload too large: encode rejected");

    /* Parse side: a frame whose length field exceeds the cap. */
    header[0] = SPI_PACKET_V1_HEADER_MAGIC;
    header[1] = 0x0004U;
    header[2] = (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U);
    header[3] = 0x0000U;
    r = SpiPacketV1_ParseWords(header, 8U, &pkt);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "payload too large: parse rejected");
}

/* ------------------------------------------------------------------ */
/* Test 10/11/14: max payload (4096 words) encode, parse, round-trip   */
/* ------------------------------------------------------------------ */
static void test_max_payload(void)
{
    /* Static to keep these ~8 KB buffers off the stack. */
    static uint16_t payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS];
    static uint16_t frame[SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                          + SPI_PACKET_V1_OVERHEAD_WORDS];
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e re, rp;
    uint16_t len = 0U;
    uint16_t i;

    for (i = 0U; i < SPI_PACKET_V1_MAX_PAYLOAD_WORDS; ++i)
    {
        payload[i] = (uint16_t)(i & 0xFFFFU);
    }
    payload[0] = 0xF00DU;                                       /* first word */
    payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS - 1U] = 0xCAFEU;    /* last word  */

    re = SpiPacketV1_Encode(0x0123U, payload,
                            SPI_PACKET_V1_MAX_PAYLOAD_WORDS,
                            frame,
                            (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                                       + SPI_PACKET_V1_OVERHEAD_WORDS),
                            &len);
    check(re == SPI_PACKET_V1_OK,          "encode 4096: returns OK");
    check(len == 4100U,                    "encode 4096: len == 4100 (4096+4)");

    rp = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(rp == SPI_PACKET_V1_OK,          "parse 4096: returns OK");
    check(pkt.cmdId == 0x0123U,            "round-trip 4096: cmd id recovered");
    check(pkt.payloadWords == SPI_PACKET_V1_MAX_PAYLOAD_WORDS,
          "round-trip 4096: payloadWords == 4096");
    check((pkt.payload != NULL) && (pkt.payload[0] == 0xF00DU),
          "round-trip 4096: first payload word preserved");
    check((pkt.payload != NULL)
              && (pkt.payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS - 1U] == 0xCAFEU),
          "round-trip 4096: last payload word preserved");
}

/* ------------------------------------------------------------------ */
/* Test 12/13: over-max (4097) rejected on encode and on parse         */
/* ------------------------------------------------------------------ */
static void test_over_max_payload(void)
{
    uint16_t header[8];
    uint16_t dummyPayload[1] = { 0U };
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t len = 0U;

    /* Encode side: 4097 payload words must be refused. */
    r = SpiPacketV1_Encode(0x0124U, dummyPayload,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U),
                           header, 8U, &len);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "encode 4097: rejected (payload too large)");

    /* Parse side: a frame declaring Data Length = 4097 must be refused.
     * The length sanity check fires before any large read is attempted, so a
     * small buffer is sufficient to exercise it. */
    header[0] = SPI_PACKET_V1_HEADER_MAGIC;
    header[1] = 0x0124U;
    header[2] = (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U);  /* 4097 */
    header[3] = 0x0000U;
    r = SpiPacketV1_ParseWords(header, 8U, &pkt);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "parse 4097: rejected (Data Length too large)");
}

/* ------------------------------------------------------------------ */
/* Test (defensive): truncated + null-arg                              */
/* ------------------------------------------------------------------ */
static void test_truncated_and_null(void)
{
    uint16_t frame[3] = { SPI_PACKET_V1_HEADER_MAGIC, 0x0001U, 0x0000U };
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    r = SpiPacketV1_ParseWords(frame, 3U, &pkt);  /* < 4 words */
    check(r == SPI_PACKET_V1_ERR_TRUNCATED, "truncated frame: rejected");

    r = SpiPacketV1_ParseWords(NULL, 4U, &pkt);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG,  "null words: rejected");
}

/* ================================================================== */
/* A1 - Malformed Packet Matrix                                       */
/* (see docs/SPI_PACKET_V1_SPEC.md "Malformed Packet Handling")       */
/* ================================================================== */

/* A1-1: parser NULL args + below-minimum word counts. */
static void test_a1_parser_null_minlen(void)
{
    uint16_t buf[SPI_PACKET_V1_MIN_WORDS];
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t i;

    for (i = 0U; i < SPI_PACKET_V1_MIN_WORDS; ++i)
    {
        buf[i] = 0U;
    }

    /* Validation step 1: NULL argument check. */
    r = SpiPacketV1_ParseWords(NULL, SPI_PACKET_V1_MIN_WORDS, &pkt);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG, "A1 parse: words=NULL -> ERR_NULL_ARG");
    r = SpiPacketV1_ParseWords(buf, SPI_PACKET_V1_MIN_WORDS, NULL);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG, "A1 parse: outPkt=NULL -> ERR_NULL_ARG");

    /* Validation step 2: minimum word_count. Anything < MIN_WORDS is the
     * too-short class (ERR_TRUNCATED), regardless of which field is missing. */
    r = SpiPacketV1_ParseWords(buf, 0U, &pkt);
    check(r == SPI_PACKET_V1_ERR_TRUNCATED, "A1 parse: wordCount=0 -> ERR_TRUNCATED");
    r = SpiPacketV1_ParseWords(buf, 1U, &pkt);
    check(r == SPI_PACKET_V1_ERR_TRUNCATED, "A1 parse: wordCount=1 -> ERR_TRUNCATED");
    r = SpiPacketV1_ParseWords(buf, 2U, &pkt);
    check(r == SPI_PACKET_V1_ERR_TRUNCATED, "A1 parse: wordCount=2 -> ERR_TRUNCATED");
    r = SpiPacketV1_ParseWords(buf, 3U, &pkt);
    check(r == SPI_PACKET_V1_ERR_TRUNCATED,
          "A1 parse: wordCount=3 (no CRC) -> ERR_TRUNCATED");
    r = SpiPacketV1_ParseWords(buf, (uint16_t)(SPI_PACKET_V1_MIN_WORDS - 1U), &pkt);
    check(r == SPI_PACKET_V1_ERR_TRUNCATED, "A1 parse: wordCount=MIN-1 -> ERR_TRUNCATED");
}

/* A1-2: header error (word0 != 0xA55A), with otherwise valid length. */
static void test_a1_header_error(void)
{
    uint16_t frame[SPI_PACKET_V1_MIN_WORDS];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x00C0U, NULL, 0U, frame,
                             SPI_PACKET_V1_MIN_WORDS, &len);

    frame[SPI_PACKET_V1_OFFSET_HEADER] = 0x0000U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER, "A1 header: 0x0000 -> ERR_BAD_HEADER");

    frame[SPI_PACKET_V1_OFFSET_HEADER] = 0xFFFFU;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER, "A1 header: 0xFFFF -> ERR_BAD_HEADER");

    frame[SPI_PACKET_V1_OFFSET_HEADER] = 0xA55BU;   /* 1 bit off from 0xA55A */
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER,
          "A1 header: 0xA55B (bit flip) -> ERR_BAD_HEADER");

    frame[SPI_PACKET_V1_OFFSET_HEADER] = 0x1234U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER, "A1 header: 0x1234 -> ERR_BAD_HEADER");
}

/* A1-3: declared length vs actual word_count mismatch (small frames). */
static void test_a1_length_mismatch(void)
{
    static const uint16_t p2[2] = { 0x1111U, 0x2222U };
    uint16_t frame[16];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    /* Declared length 0 but an extra payload word makes word_count too big. */
    (void)SpiPacketV1_Encode(0x0010U, NULL, 0U, frame, 16U, &len);  /* len=4 */
    r = SpiPacketV1_ParseWords(frame, (uint16_t)(len + 1U), &pkt);  /* 5 != 4 */
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 0 + extra word -> ERR_LENGTH");

    /* Declared length 1, payload word missing (only 4 words present). */
    frame[0] = SPI_PACKET_V1_HEADER_MAGIC;
    frame[1] = 0x0010U;
    frame[2] = 1U;
    frame[3] = 0x0000U;
    r = SpiPacketV1_ParseWords(frame, 4U, &pkt);   /* declared 1 -> needs 5, got 4 */
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 1, payload missing -> ERR_LENGTH");

    /* Declared length 1, CRC word missing (same shape: 4 words, needs 5). */
    frame[0] = SPI_PACKET_V1_HEADER_MAGIC;
    frame[1] = 0x0010U;
    frame[2] = 1U;
    frame[3] = 0xBEEFU;   /* a payload word, but no CRC word follows */
    r = SpiPacketV1_ParseWords(frame, 4U, &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 1, CRC missing -> ERR_LENGTH");

    /* Declared length 2 but only 1 payload word present (5 words, needs 6). */
    (void)SpiPacketV1_Encode(0x0010U, p2, 2U, frame, 16U, &len);   /* len=6 */
    r = SpiPacketV1_ParseWords(frame, 5U, &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 2, 1 payload word -> ERR_LENGTH");

    /* Declared length 4097 -> too large (step 4, before exact-length). */
    frame[0] = SPI_PACKET_V1_HEADER_MAGIC;
    frame[1] = 0x0010U;
    frame[2] = (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U);
    frame[3] = 0x0000U;
    r = SpiPacketV1_ParseWords(frame, 8U, &pkt);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "A1 len: declared 4097 -> ERR_PAYLOAD_TOO_LARGE");
}

/* A1-3b: length mismatch at the 4096 boundary (truncated / extra word). */
static void test_a1_length_mismatch_4096(void)
{
    static uint16_t payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS];
    static uint16_t frame[SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                          + SPI_PACKET_V1_OVERHEAD_WORDS + 1U];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t i;

    for (i = 0U; i < SPI_PACKET_V1_MAX_PAYLOAD_WORDS; ++i)
    {
        payload[i] = (uint16_t)(i & 0xFFFFU);
    }
    (void)SpiPacketV1_Encode(0x0011U, payload, SPI_PACKET_V1_MAX_PAYLOAD_WORDS,
                             frame,
                             (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                                        + SPI_PACKET_V1_OVERHEAD_WORDS),
                             &len);   /* len == 4100, declared 4096 */

    r = SpiPacketV1_ParseWords(frame, (uint16_t)(len - 1U), &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 4096, truncated -1 -> ERR_LENGTH");

    frame[len] = 0x0000U;   /* extra trailing word (frame has room for +1) */
    r = SpiPacketV1_ParseWords(frame, (uint16_t)(len + 1U), &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 len: declared 4096, extra +1 -> ERR_LENGTH");
}

/* A1-4: CRC mismatch and validation-order interactions. */
static void test_a1_crc_mismatch(void)
{
    static const uint16_t p3[3] = { 0x0AAAU, 0x0BBBU, 0x0CCCU };
    uint16_t good[16];
    uint16_t frame[16];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t i;

    (void)SpiPacketV1_Encode(0x0030U, p3, 3U, good, 16U, &len);   /* len == 7 */

    /* Header bit flip: header check (step 3) precedes CRC (step 6). */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[SPI_PACKET_V1_OFFSET_HEADER] ^= 0x0001U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_BAD_HEADER,
          "A1 crc: header bit flip -> ERR_BAD_HEADER (order)");

    /* Command id bit flip: header & length valid -> CRC fails. */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[SPI_PACKET_V1_OFFSET_CMD] ^= 0x0001U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "A1 crc: command id flip -> ERR_CRC");

    /* Data length bit flip (3 -> 2): exact-length check (step 5) fires first. */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[SPI_PACKET_V1_OFFSET_LENGTH] ^= 0x0001U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_LENGTH_MISMATCH,
          "A1 crc: data length flip -> ERR_LENGTH (order)");

    /* Payload[0] bit flip -> CRC fails. */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[SPI_PACKET_V1_OFFSET_PAYLOAD] ^= 0x0001U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "A1 crc: payload[0] flip -> ERR_CRC");

    /* Last payload word bit flip -> CRC fails. */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[SPI_PACKET_V1_OFFSET_PAYLOAD + 2U] ^= 0x8000U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "A1 crc: last payload flip -> ERR_CRC");

    /* CRC word bit flip -> CRC fails. */
    for (i = 0U; i < len; ++i) { frame[i] = good[i]; }
    frame[len - 1U] ^= 0x0001U;
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "A1 crc: CRC word flip -> ERR_CRC");
}

/* A1-5: encoder defensive behaviour (small frames). */
static void test_a1_encoder_defensive(void)
{
    static const uint16_t p1[1] = { 0x1234U };
    uint16_t frame[16];
    uint16_t len = 0U;
    SPI_PACKET_V1_RESULT_e r;

    r = SpiPacketV1_Encode(0x0040U, p1, 1U, NULL, 16U, &len);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG, "A1 enc: outWords=NULL -> ERR_NULL_ARG");

    r = SpiPacketV1_Encode(0x0040U, p1, 1U, frame, 16U, NULL);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG, "A1 enc: outLen=NULL -> ERR_NULL_ARG");

    len = 0U;
    r = SpiPacketV1_Encode(0x0040U, NULL, 0U, frame, 16U, &len);
    check((r == SPI_PACKET_V1_OK) && (len == 4U),
          "A1 enc: payload=NULL, words=0 -> OK (len 4)");

    r = SpiPacketV1_Encode(0x0040U, NULL, 1U, frame, 16U, &len);
    check(r == SPI_PACKET_V1_ERR_NULL_ARG,
          "A1 enc: payload=NULL, words>0 -> ERR_NULL_ARG");

    /* 1 payload word needs 5; give 4 (short by 1). */
    r = SpiPacketV1_Encode(0x0040U, p1, 1U, frame, 4U, &len);
    check(r == SPI_PACKET_V1_ERR_BUFFER_TOO_SMALL,
          "A1 enc: capacity short by 1 -> ERR_BUFFER_TOO_SMALL");

    len = 0U;
    r = SpiPacketV1_Encode(0x0040U, p1, 1U, frame, 5U, &len);
    check((r == SPI_PACKET_V1_OK) && (len == 5U),
          "A1 enc: capacity exactly required -> OK (len 5)");
}

/* A1-5b: encoder payload-size bounds (4096 OK, 4097 rejected). */
static void test_a1_encoder_payload_bounds(void)
{
    static uint16_t payload[SPI_PACKET_V1_MAX_PAYLOAD_WORDS];
    static uint16_t frame[SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                          + SPI_PACKET_V1_OVERHEAD_WORDS];
    uint16_t len = 0U;
    SPI_PACKET_V1_RESULT_e r;
    uint16_t i;

    for (i = 0U; i < SPI_PACKET_V1_MAX_PAYLOAD_WORDS; ++i)
    {
        payload[i] = (uint16_t)(i & 0xFFFFU);
    }

    r = SpiPacketV1_Encode(0x0041U, payload, SPI_PACKET_V1_MAX_PAYLOAD_WORDS,
                           frame,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                                      + SPI_PACKET_V1_OVERHEAD_WORDS),
                           &len);
    check((r == SPI_PACKET_V1_OK) && (len == 4100U),
          "A1 enc: payloadWords=4096 -> OK (len 4100)");

    r = SpiPacketV1_Encode(0x0041U,
                           payload,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U),
                           frame,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS
                                      + SPI_PACKET_V1_OVERHEAD_WORDS),
                           &len);
    check(r == SPI_PACKET_V1_ERR_PAYLOAD_TOO_LARGE,
          "A1 enc: payloadWords=4097 -> ERR_PAYLOAD_TOO_LARGE");
}

/* A1-6: boundary command / payload values, all expected to round-trip. */
static void test_a1_boundary_values(void)
{
    static const uint16_t allZero[4] = { 0x0000U, 0x0000U, 0x0000U, 0x0000U };
    static const uint16_t allOnes[4] = { 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU };
    static const uint16_t altern[4]  = { 0xAAAAU, 0x5555U, 0xAAAAU, 0x5555U };
    static const uint16_t hdrLike[3] = { 0xA55AU, 0x0001U, 0x1234U };
    uint16_t frame[16];
    uint16_t len = 0U;
    uint16_t crcVal;
    uint16_t crcPay[2];
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x0000U, allZero, 4U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.cmdId == 0x0000U)
              && (pkt.payloadWords == 4U),
          "A1 bound: cmd 0x0000 round-trip OK");

    (void)SpiPacketV1_Encode(0xFFFFU, allZero, 4U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.cmdId == 0xFFFFU),
          "A1 bound: cmd 0xFFFF round-trip OK");

    (void)SpiPacketV1_Encode(0x0050U, allZero, 4U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.payload != NULL)
              && (pkt.payload[0] == 0x0000U) && (pkt.payload[3] == 0x0000U),
          "A1 bound: payload all 0x0000 round-trip OK");

    (void)SpiPacketV1_Encode(0x0050U, allOnes, 4U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.payload != NULL)
              && (pkt.payload[0] == 0xFFFFU) && (pkt.payload[3] == 0xFFFFU),
          "A1 bound: payload all 0xFFFF round-trip OK");

    (void)SpiPacketV1_Encode(0x0050U, altern, 4U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.payload != NULL)
              && (pkt.payload[0] == 0xAAAAU) && (pkt.payload[1] == 0x5555U),
          "A1 bound: payload alternating AAAA/5555 round-trip OK");

    /* Payload word that equals the header magic must stay payload, not header. */
    (void)SpiPacketV1_Encode(0x0050U, hdrLike, 3U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.payloadWords == 3U)
              && (pkt.payload != NULL) && (pkt.payload[0] == 0xA55AU),
          "A1 bound: payload[0]=0xA55A not mistaken as header");

    /* Payload word that equals a real CRC value is just data. */
    crcVal    = frame[len - 1U];       /* an actual CRC value from a real frame */
    crcPay[0] = crcVal;
    crcPay[1] = 0x1234U;
    (void)SpiPacketV1_Encode(0x0050U, crcPay, 2U, frame, 16U, &len);
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check((r == SPI_PACKET_V1_OK) && (pkt.payload != NULL)
              && (pkt.payload[0] == crcVal),
          "A1 bound: CRC-looking payload value parsed as data");
}

/* A1-7: on failure the parser must not surface valid-looking output. */
static void test_a1_failure_output_safety(void)
{
    static const uint16_t p2[2] = { 0x1357U, 0x2468U };
    uint16_t frame[8];
    uint16_t len = 0U;
    ST_SPI_PACKET_V1 pkt;
    SPI_PACKET_V1_RESULT_e r;

    (void)SpiPacketV1_Encode(0x7777U, p2, 2U, frame, 8U, &len);

    /* Pre-load the output with sentinels that must not survive a failed parse. */
    pkt.cmdId        = 0xDEADU;
    pkt.payloadWords = 0x7FFFU;
    pkt.payload      = p2;          /* non-NULL sentinel */
    pkt.crc          = 0xBEEFU;

    frame[len - 1U] ^= 0xFFFFU;     /* corrupt CRC -> fail at the last gate */
    r = SpiPacketV1_ParseWords(frame, len, &pkt);
    check(r == SPI_PACKET_V1_ERR_CRC_MISMATCH, "A1 safety: corrupted packet rejected");

    /* Contract: a failed parse zeroes the output and never exposes the
     * (attacker-controlled) command id / payload of the bad frame. */
    check(pkt.cmdId != 0x7777U,        "A1 safety: failure does not expose cmd id");
    check(pkt.cmdId == 0x0000U,        "A1 safety: failure zeroes cmd id");
    check(pkt.payloadWords == 0x0000U, "A1 safety: failure zeroes payloadWords");
    check(pkt.payload == NULL,         "A1 safety: failure clears payload pointer");
}

int main(void)
{
    printf("=== SPI Packet V1 pure-C test ===\n");

    test_crc_check_vector();    /* #1 */
    test_encode_empty();        /* #2 */
    test_parse_empty();         /* #3 */
    test_roundtrip_3word();     /* #4 + #9 */
    test_bad_header();          /* #5 */
    test_length_mismatch();     /* #6 */
    test_crc_mismatch();        /* #7 */
    test_payload_too_large();   /* #8 */
    test_max_payload();         /* #10 + #11 + #14 */
    test_over_max_payload();    /* #12 + #13 */
    test_truncated_and_null();  /* defensive */

    /* ---- A1: Malformed Packet Matrix ---- */
    test_a1_parser_null_minlen();
    test_a1_header_error();
    test_a1_length_mismatch();
    test_a1_length_mismatch_4096();
    test_a1_crc_mismatch();
    test_a1_encoder_defensive();
    test_a1_encoder_payload_bounds();
    test_a1_boundary_values();
    test_a1_failure_output_safety();

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
