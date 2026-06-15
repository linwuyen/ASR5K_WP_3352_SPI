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

    /* Encode side: declared payload exceeds the cap. */
    r = SpiPacketV1_Encode(0x0004U, dummyPayload,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U),
                           header, 8U, NULL);
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

    /* Encode side: 4097 payload words must be refused. */
    r = SpiPacketV1_Encode(0x0124U, dummyPayload,
                           (uint16_t)(SPI_PACKET_V1_MAX_PAYLOAD_WORDS + 1U),
                           header, 8U, NULL);
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

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
