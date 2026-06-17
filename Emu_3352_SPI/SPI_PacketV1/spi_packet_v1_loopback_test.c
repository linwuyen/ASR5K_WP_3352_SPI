/*
 * spi_packet_v1_loopback_test.c
 *
 * SPI Packet V1 - pure-C loopback path (A5) host test harness.
 *
 * HOST test, not firmware. The whole harness (main() + <stdio.h>) compiles only
 * when SPI_PACKET_V1_HOST_TEST is defined, mirroring the A2/A4 tests so that if
 * this file ever sits in the CCS build it contributes no second main() and pulls
 * in no CIO/printf. Build and run on a host PC, e.g.:
 *
 *   gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
 *       spi_packet_crc16.c spi_packet_v1.c spi_packet_v1_dispatch.c \
 *       spi_packet_v1_loopback.c spi_packet_v1_loopback_test.c \
 *       -o spi_packet_v1_loopback_test && ./spi_packet_v1_loopback_test
 *
 * Exit code 0 = all cases pass; non-zero = at least one failure.
 * Board-verifiable: NO. This exercises the pure-C request/response path only.
 */

/* Guarantee a non-empty translation unit in the firmware build. */
typedef int spi_packet_v1_loopback_test_translation_unit_t;

#ifdef SPI_PACKET_V1_HOST_TEST

#include <stdint.h>
#include <stdio.h>

#include "spi_packet_v1.h"
#include "spi_packet_v1_cmd.h"
#include "spi_packet_v1_dispatch.h"
#include "spi_packet_v1_loopback.h"

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

/* Build a valid request frame with the A2 encoder; returns frame word count. */
static uint16_t build_req(uint16_t cmd, const uint16_t *pl, uint16_t n,
                          uint16_t *buf, uint16_t cap)
{
    uint16_t               len = 0U;
    SPI_PACKET_V1_RESULT_e r   = SpiPacketV1_Encode(cmd, pl, n, buf, cap, &len);
    return (r == SPI_PACKET_V1_OK) ? len : 0U;
}

/* Build a request, run the full loopback into rsp_words. */
static PKTV1_LOOPBACK_RESULT_e do_loop(uint16_t cmd, const uint16_t *pl, uint16_t n,
                                       uint16_t *rsp_words, uint16_t rsp_cap,
                                       uint16_t *rsp_count,
                                       ST_PKTV1_LOOPBACK_DIAG *diag)
{
    uint16_t reqbuf[80];
    uint16_t reqlen = build_req(cmd, pl, n, reqbuf, (uint16_t)sizeof(reqbuf) / 2U);
    return SpiPacketV1_Loopback(reqbuf, reqlen, rsp_words, rsp_cap, rsp_count, diag);
}

/*
 * Assert that (cmd, payload) produces a valid PKTV1_RSP_ERROR response frame
 * carrying { original_cmd, errcode } and the expected dispatch status.
 */
static void expect_error_envelope(uint16_t cmd, const uint16_t *pl, uint16_t n,
                                   uint16_t exp_orig, uint16_t exp_errcode,
                                   uint16_t exp_dispatch, const char *name)
{
    uint16_t                rbuf[80];
    uint16_t                rcount = 0xFFFFU;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    ST_SPI_PACKET_V1        prsp;
    PKTV1_LOOPBACK_RESULT_e lr;
    SPI_PACKET_V1_RESULT_e  pr;
    int                     ok;

    lr = do_loop(cmd, pl, n, rbuf, (uint16_t)sizeof(rbuf) / 2U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_OK, name);
    check(diag.dispatch_result == exp_dispatch, "  err: diag.dispatch_result");

    pr = SpiPacketV1_ParseWords(rbuf, rcount, &prsp);
    check(pr == SPI_PACKET_V1_OK, "  err: response frame parses OK (CRC valid)");
    check(prsp.cmdId == PKTV1_RSP_ERROR, "  err: response cmd = RSP_ERROR");
    check(prsp.payloadWords == 2U, "  err: response payload length 2");
    ok = (prsp.payloadWords == 2U) && (prsp.payload != NULL) &&
         (prsp.payload[0] == exp_orig) && (prsp.payload[1] == exp_errcode);
    check(ok, "  err: response payload {original_cmd, errcode}");
}

/* ------------------------------------------------------------------ */
/* 1-8: meta command success (PING / GET_VERSION / GET_CAPS)          */
/* ------------------------------------------------------------------ */
static void test_meta_success(void)
{
    uint16_t                rbuf[80];
    uint16_t                rcount = 0U;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    ST_SPI_PACKET_V1        p;
    PKTV1_LOOPBACK_RESULT_e lr;
    int                     ok;

    /* 1-4: PING */
    lr = do_loop(PKTV1_CMD_PING, NULL, 0U, rbuf, 80U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_OK, "PING: loopback OK");
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_OK,
          "PING: response parses OK");
    check(p.cmdId == PKTV1_CMD_PING, "PING: response cmd 0x8000");
    check(p.payloadWords == 1U, "PING: response payload length 1");
    check((p.payload != NULL) && (p.payload[0] == PKTV1_PONG_WORD),
          "PING: response payload[0] == 0x504F");

    /* 5-6: GET_VERSION */
    lr = do_loop(PKTV1_CMD_GET_VERSION, NULL, 0U, rbuf, 80U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_OK, "GET_VERSION: loopback OK");
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_OK,
          "GET_VERSION: response parses OK");
    ok = (p.cmdId == PKTV1_CMD_GET_VERSION) && (p.payloadWords == 4U) &&
         (p.payload != NULL) &&
         (p.payload[0] == PKTV1_VERSION_MAJOR) &&
         (p.payload[1] == PKTV1_VERSION_MINOR) &&
         (p.payload[2] == PKTV1_VERSION_PATCH) &&
         (p.payload[3] == PKTV1_SPEC_REV);
    check(ok, "GET_VERSION: payload {1,0,0,3}");

    /* 7-8: GET_CAPS */
    lr = do_loop(PKTV1_CMD_GET_CAPS, NULL, 0U, rbuf, 80U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_OK, "GET_CAPS: loopback OK");
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_OK,
          "GET_CAPS: response parses OK");
    check((p.cmdId == PKTV1_CMD_GET_CAPS) && (p.payloadWords == 3U) &&
          (p.payload != NULL) &&
          (p.payload[0] == (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS),
          "GET_CAPS: payload[0] == 64 (max payload words)");
}

/* ------------------------------------------------------------------ */
/* 9-12: ECHO success (len 0, 1, multi, max)                          */
/* ------------------------------------------------------------------ */
static void test_echo_success(void)
{
    uint16_t                rbuf[80];
    uint16_t                rcount = 0U;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    ST_SPI_PACKET_V1        p;
    uint16_t                one   = 0xBEEFU;
    uint16_t                three[3] = { 0x0001U, 0x0002U, 0x0003U };
    uint16_t                big[PKTV1_DISPATCH_MAX_PAYLOAD_WORDS];
    unsigned                i;
    int                     ok;

    for (i = 0U; i < (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        big[i] = (uint16_t)((i * 3U) + 0x10U);
    }

    /* 9: ECHO len 0 */
    (void)do_loop(PKTV1_CMD_ECHO, NULL, 0U, rbuf, 80U, &rcount, &diag);
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_OK,
          "ECHO len0: response parses OK");
    check((p.cmdId == PKTV1_CMD_ECHO) && (p.payloadWords == 0U),
          "ECHO len0: empty payload");

    /* 10: ECHO len 1 */
    (void)do_loop(PKTV1_CMD_ECHO, &one, 1U, rbuf, 80U, &rcount, &diag);
    (void)SpiPacketV1_ParseWords(rbuf, rcount, &p);
    check((p.cmdId == PKTV1_CMD_ECHO) && (p.payloadWords == 1U) &&
          (p.payload != NULL) && (p.payload[0] == 0xBEEFU),
          "ECHO len1: payload preserved");

    /* 11: ECHO multi-word */
    (void)do_loop(PKTV1_CMD_ECHO, three, 3U, rbuf, 80U, &rcount, &diag);
    (void)SpiPacketV1_ParseWords(rbuf, rcount, &p);
    ok = (p.cmdId == PKTV1_CMD_ECHO) && (p.payloadWords == 3U) &&
         (p.payload != NULL) && (p.payload[0] == 0x0001U) &&
         (p.payload[1] == 0x0002U) && (p.payload[2] == 0x0003U);
    check(ok, "ECHO 3w: payload preserved");

    /* 12: ECHO max dispatcher length (64) */
    (void)do_loop(PKTV1_CMD_ECHO, big, (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS,
                  rbuf, 80U, &rcount, &diag);
    (void)SpiPacketV1_ParseWords(rbuf, rcount, &p);
    ok = (p.cmdId == PKTV1_CMD_ECHO) &&
         (p.payloadWords == (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS) &&
         (p.payload != NULL);
    for (i = 0U; ok && (i < (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS); ++i)
    {
        if (p.payload[i] != big[i]) { ok = 0; }
    }
    check(ok, "ECHO max(64): every word preserved");
}

/* ------------------------------------------------------------------ */
/* 13-23: dispatcher semantic errors -> valid error response frames   */
/* ------------------------------------------------------------------ */
static void test_error_responses(void)
{
    uint16_t over[PKTV1_DISPATCH_MAX_PAYLOAD_WORDS + 1U];
    uint16_t one = 0x0001U;
    unsigned i;

    for (i = 0U; i <= (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        over[i] = (uint16_t)(i + 1U);
    }

    /* 13: ECHO 65 -> RSP_OVERFLOW envelope */
    expect_error_envelope(PKTV1_CMD_ECHO, over,
                          (uint16_t)(PKTV1_DISPATCH_MAX_PAYLOAD_WORDS + 1U),
                          PKTV1_CMD_ECHO, PKTV1_ERRCODE_RSP_OVERFLOW,
                          (uint16_t)PKTV1_DISPATCH_ERR_RSP_OVERFLOW,
                          "ECHO over-cap -> RSP_OVERFLOW response");

    /* 14-16: zero-payload commands with a payload -> BAD_LENGTH */
    expect_error_envelope(PKTV1_CMD_PING, &one, 1U,
                          PKTV1_CMD_PING, PKTV1_ERRCODE_BAD_LENGTH,
                          (uint16_t)PKTV1_DISPATCH_ERR_BAD_LENGTH,
                          "PING+payload -> BAD_LENGTH response");
    expect_error_envelope(PKTV1_CMD_GET_VERSION, &one, 1U,
                          PKTV1_CMD_GET_VERSION, PKTV1_ERRCODE_BAD_LENGTH,
                          (uint16_t)PKTV1_DISPATCH_ERR_BAD_LENGTH,
                          "GET_VERSION+payload -> BAD_LENGTH response");
    expect_error_envelope(PKTV1_CMD_GET_CAPS, &one, 1U,
                          PKTV1_CMD_GET_CAPS, PKTV1_ERRCODE_BAD_LENGTH,
                          (uint16_t)PKTV1_DISPATCH_ERR_BAD_LENGTH,
                          "GET_CAPS+payload -> BAD_LENGTH response");

    /* 17-21: forbidden command IDs */
    expect_error_envelope(0x0958U, NULL, 0U, 0x0958U, PKTV1_ERRCODE_FORBIDDEN,
                          (uint16_t)PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,
                          "cmd 0x0958 -> FORBIDDEN response");
    expect_error_envelope(0x0960U, NULL, 0U, 0x0960U, PKTV1_ERRCODE_FORBIDDEN,
                          (uint16_t)PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,
                          "cmd 0x0960 -> FORBIDDEN response");
    expect_error_envelope(0x3000U, NULL, 0U, 0x3000U, PKTV1_ERRCODE_FORBIDDEN,
                          (uint16_t)PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,
                          "cmd 0x3000 -> FORBIDDEN response");
    expect_error_envelope(0x3FFFU, NULL, 0U, 0x3FFFU, PKTV1_ERRCODE_FORBIDDEN,
                          (uint16_t)PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,
                          "cmd 0x3FFF -> FORBIDDEN response");
    expect_error_envelope(0xA55AU, NULL, 0U, 0xA55AU, PKTV1_ERRCODE_FORBIDDEN,
                          (uint16_t)PKTV1_DISPATCH_ERR_FORBIDDEN_CMD,
                          "cmd 0xA55A -> FORBIDDEN response");

    /* 22-23: unknown commands in the 0x8000+ namespace */
    expect_error_envelope(0x8004U, NULL, 0U, 0x8004U, PKTV1_ERRCODE_UNSUPPORTED,
                          (uint16_t)PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD,
                          "cmd 0x8004 -> UNSUPPORTED response");
    expect_error_envelope(0xFFFFU, NULL, 0U, 0xFFFFU, PKTV1_ERRCODE_UNSUPPORTED,
                          (uint16_t)PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD,
                          "cmd 0xFFFF -> UNSUPPORTED response");
}

/* ------------------------------------------------------------------ */
/* 24-26, 35: malformed framing -> ERR_PARSE, never dispatched        */
/* ------------------------------------------------------------------ */
static void test_malformed(void)
{
    uint16_t                base[8];
    uint16_t                req[8];
    uint16_t                reqlen;
    uint16_t                rbuf[16];
    uint16_t                rcount;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    PKTV1_LOOPBACK_RESULT_e lr;
    unsigned                i;

    /* Valid 4-word PING request frame as the corruption base. */
    reqlen = build_req(PKTV1_CMD_PING, NULL, 0U, base, (uint16_t)8);

    /* 24: bad header */
    for (i = 0U; i < reqlen; ++i) { req[i] = base[i]; }
    req[0] = 0x1234U;
    rcount = 0xFFFFU;
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_PARSE, "bad header -> ERR_PARSE");
    check(rcount == 0U, "bad header -> no response words");
    check(diag.parser_result == (uint16_t)SPI_PACKET_V1_ERR_BAD_HEADER,
          "bad header -> diag parser_result BAD_HEADER");

    /* 25: bad CRC (flip last word) */
    for (i = 0U; i < reqlen; ++i) { req[i] = base[i]; }
    req[reqlen - 1U] ^= 0xFFFFU;
    rcount = 0xFFFFU;
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_PARSE, "bad CRC -> ERR_PARSE");
    check(rcount == 0U, "bad CRC -> no response words");
    check(diag.parser_result == (uint16_t)SPI_PACKET_V1_ERR_CRC_MISMATCH,
          "bad CRC -> diag parser_result CRC_MISMATCH");

    /* 26: length mismatch (tamper declared length) */
    for (i = 0U; i < reqlen; ++i) { req[i] = base[i]; }
    req[SPI_PACKET_V1_OFFSET_LENGTH] = 1U;   /* claims 1 payload word; count unchanged */
    rcount = 0xFFFFU;
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_PARSE, "length mismatch -> ERR_PARSE");
    check(rcount == 0U, "length mismatch -> no response words");

    /* 35: parser failure must not dispatch (diag shows parse err, no response) */
    for (i = 0U; i < reqlen; ++i) { req[i] = base[i]; }
    req[0] = 0x0000U;   /* bad header again */
    rcount = 0xFFFFU;
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_PARSE, "no-dispatch: ERR_PARSE on bad frame");
    check(diag.parser_result != (uint16_t)SPI_PACKET_V1_OK,
          "no-dispatch: parser_result is an error");
    check(diag.dispatch_result == 0U, "no-dispatch: dispatch_result untouched (0)");
    check(diag.response_cmd == 0U, "no-dispatch: no response command in diag");
}

/* ------------------------------------------------------------------ */
/* 27-29: NULL argument handling                                      */
/* ------------------------------------------------------------------ */
static void test_null(void)
{
    uint16_t                req[8];
    uint16_t                reqlen;
    uint16_t                rbuf[16];
    uint16_t                rcount = 0xFFFFU;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    PKTV1_LOOPBACK_RESULT_e lr;

    reqlen = build_req(PKTV1_CMD_PING, NULL, 0U, req, (uint16_t)8);

    /* 27: NULL request pointer */
    lr = SpiPacketV1_Loopback(NULL, reqlen, rbuf, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_NULL, "NULL request -> ERR_NULL");
    check(rcount == 0U, "NULL request -> rsp_word_count 0");

    /* 28: NULL response buffer */
    lr = SpiPacketV1_Loopback(req, reqlen, NULL, 16U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_NULL, "NULL response buffer -> ERR_NULL");

    /* 29: NULL response length pointer */
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 16U, NULL, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_NULL, "NULL response length -> ERR_NULL");
}

/* ------------------------------------------------------------------ */
/* 30: response capacity too small -> ERR_RSP_OVERFLOW, no overwrite  */
/* ------------------------------------------------------------------ */
static void test_capacity(void)
{
    uint16_t                req[8];
    uint16_t                reqlen;
    uint16_t                rbuf[8];
    uint16_t                rcount = 0xFFFFU;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    PKTV1_LOOPBACK_RESULT_e lr;
    unsigned                i;
    int                     intact;

    reqlen = build_req(PKTV1_CMD_PING, NULL, 0U, req, (uint16_t)8);
    for (i = 0U; i < 8U; ++i) { rbuf[i] = 0xCCCCU; }   /* sentinels */

    /* PING response is 5 words; offer only 3 -> overflow before any write. */
    lr = SpiPacketV1_Loopback(req, reqlen, rbuf, 3U, &rcount, &diag);
    check(lr == PKTV1_LOOPBACK_ERR_RSP_OVERFLOW, "small capacity -> ERR_RSP_OVERFLOW");
    check(rcount == 0U, "small capacity -> rsp_word_count 0");

    intact = 1;
    for (i = 0U; i < 8U; ++i) { if (rbuf[i] != 0xCCCCU) { intact = 0; } }
    check(intact, "small capacity -> response buffer not overwritten");
}

/* ------------------------------------------------------------------ */
/* 31-32: determinism                                                 */
/* ------------------------------------------------------------------ */
static void test_determinism(void)
{
    uint16_t a[80];
    uint16_t b[80];
    uint16_t na = 0U;
    uint16_t nb = 0U;
    uint16_t pl[2] = { 0x1234U, 0x5678U };
    unsigned i;
    int      same;

    /* 31: PING twice */
    (void)do_loop(PKTV1_CMD_PING, NULL, 0U, a, 80U, &na, NULL);
    (void)do_loop(PKTV1_CMD_PING, NULL, 0U, b, 80U, &nb, NULL);
    same = (na == nb);
    for (i = 0U; same && (i < (unsigned)na); ++i) { if (a[i] != b[i]) { same = 0; } }
    check(same, "determinism: PING loopback repeatable");

    /* 32: ECHO twice */
    (void)do_loop(PKTV1_CMD_ECHO, pl, 2U, a, 80U, &na, NULL);
    (void)do_loop(PKTV1_CMD_ECHO, pl, 2U, b, 80U, &nb, NULL);
    same = (na == nb);
    for (i = 0U; same && (i < (unsigned)na); ++i) { if (a[i] != b[i]) { same = 0; } }
    check(same, "determinism: ECHO loopback repeatable");
}

/* ------------------------------------------------------------------ */
/* 33: response CRC validates (and a tampered response is rejected)   */
/* ------------------------------------------------------------------ */
static void test_response_crc(void)
{
    uint16_t               rbuf[80];
    uint16_t               rcount = 0U;
    ST_SPI_PACKET_V1       p;

    (void)do_loop(PKTV1_CMD_PING, NULL, 0U, rbuf, 80U, &rcount, NULL);
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_OK,
          "response CRC: A2 parser accepts untampered response");

    rbuf[rcount - 1U] ^= 0x0001U;   /* corrupt the response CRC word */
    check(SpiPacketV1_ParseWords(rbuf, rcount, &p) == SPI_PACKET_V1_ERR_CRC_MISMATCH,
          "response CRC: tampered response rejected by A2 parser");
}

/* ------------------------------------------------------------------ */
/* 34: response payload must not alias the request payload            */
/* ------------------------------------------------------------------ */
static void test_no_alias(void)
{
    uint16_t                reqbuf[80];
    uint16_t                rbuf[80];
    uint16_t                reqlen;
    uint16_t                rcount = 0U;
    uint16_t                in_pl[3] = { 0x1111U, 0x2222U, 0x3333U };
    ST_SPI_PACKET_V1        p;
    ST_PKTV1_LOOPBACK_DIAG  diag;

    reqlen = build_req(PKTV1_CMD_ECHO, in_pl, 3U, reqbuf, (uint16_t)80);
    (void)SpiPacketV1_Loopback(reqbuf, reqlen, rbuf, 80U, &rcount, &diag);

    check((const void *)rbuf != (const void *)reqbuf,
          "no-alias: response buffer distinct from request buffer");

    (void)SpiPacketV1_ParseWords(rbuf, rcount, &p);
    /* Mutate the request buffer's payload words after the call. */
    reqbuf[SPI_PACKET_V1_OFFSET_PAYLOAD + 0U] = 0xDEADU;
    check((p.payload != NULL) && (p.payload[0] == 0x1111U),
          "no-alias: response unaffected by request mutation");
}

int main(void)
{
    printf("=== SPI Packet V1 A5 loopback test ===\n");

    test_meta_success();    /* 1-8   */
    test_echo_success();    /* 9-12  */
    test_error_responses(); /* 13-23 */
    test_malformed();       /* 24-26, 35 */
    test_null();            /* 27-29 */
    test_capacity();        /* 30    */
    test_determinism();     /* 31-32 */
    test_response_crc();    /* 33    */
    test_no_alias();        /* 34    */

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
