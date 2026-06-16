/*
 * spi_packet_v1_dispatch_test.c
 *
 * SPI Packet V1 - pure-C dispatcher mock (A4) host test harness.
 *
 * HOST test, not firmware. The entire harness (main() + <stdio.h>) compiles
 * only when SPI_PACKET_V1_HOST_TEST is defined, mirroring the A2 test so that
 * if this file ever sits in the CCS build it contributes no second main() and
 * pulls in no CIO/printf. Build and run on a host PC, e.g.:
 *
 *   gcc -std=c99 -Wall -Wextra -DSPI_PACKET_V1_HOST_TEST \
 *       spi_packet_v1_dispatch.c spi_packet_v1_dispatch_test.c \
 *       -o spi_packet_v1_dispatch_test && ./spi_packet_v1_dispatch_test
 *
 * Exit code 0 = all cases pass; non-zero = at least one failure.
 * Covers the A4 unit-test matrix in docs/SPI_PACKET_V1_A4_DISPATCHER_MOCK.md.
 * Framing/CRC/malformed-frame cases belong to the A2 parser test, not here.
 */

/* Guarantee a non-empty translation unit in the firmware build. */
typedef int spi_packet_v1_dispatch_test_translation_unit_t;

#ifdef SPI_PACKET_V1_HOST_TEST

#include <stdint.h>
#include <stdio.h>

#include "spi_packet_v1.h"
#include "spi_packet_v1_cmd.h"
#include "spi_packet_v1_dispatch.h"

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

/* Build a parsed-packet view (the dispatcher's input). */
static ST_SPI_PACKET_V1 make_req(uint16_t cmd, const uint16_t *pl, uint16_t n)
{
    ST_SPI_PACKET_V1 r;
    r.cmdId        = cmd;
    r.payloadWords = n;
    r.payload      = pl;
    r.crc          = 0U;   /* dispatcher ignores CRC (A2 layer owns it) */
    return r;
}

/* Assert a forbidden command_id yields an ERROR response with FORBIDDEN code. */
static void expect_forbidden(uint16_t cmd, const char *name)
{
    ST_SPI_PACKET_V1      req = make_req(cmd, NULL, 0U);
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc = SpiPacketV1_Dispatch(&req, &rsp);

    check(rc == PKTV1_DISPATCH_ERR_FORBIDDEN_CMD, name);
    check(rsp.command_id == PKTV1_RSP_ERROR, "  forbidden -> RSP_ERROR command_id");
    check(rsp.payload_length_words == 2U, "  forbidden -> 2-word error payload");
    check(rsp.payload[0] == cmd, "  forbidden -> original cmd preserved");
    check(rsp.payload[1] == PKTV1_ERRCODE_FORBIDDEN, "  forbidden -> errcode FORBIDDEN");
}

/* Assert a known command with an unexpected payload yields BAD_LENGTH. */
static void expect_bad_length(uint16_t cmd, const char *name)
{
    uint16_t              one = 0x0001U;
    ST_SPI_PACKET_V1      req = make_req(cmd, &one, 1U);
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc = SpiPacketV1_Dispatch(&req, &rsp);

    check(rc == PKTV1_DISPATCH_ERR_BAD_LENGTH, name);
    check(rsp.command_id == PKTV1_RSP_ERROR, "  bad-length -> RSP_ERROR command_id");
    check(rsp.payload[0] == cmd, "  bad-length -> original cmd preserved");
    check(rsp.payload[1] == PKTV1_ERRCODE_BAD_LENGTH, "  bad-length -> errcode BAD_LENGTH");
}

/* ------------------------------------------------------------------ */
/* 1-3: meta command success                                          */
/* ------------------------------------------------------------------ */
static void test_meta_success(void)
{
    ST_SPI_PACKET_V1      req;
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc;

    /* 1. PING */
    req = make_req(PKTV1_CMD_PING, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "PING: returns OK");
    check(rsp.command_id == PKTV1_CMD_PING, "PING: response command_id");
    check(rsp.payload_length_words == 1U, "PING: 1-word reply");
    check(rsp.payload[0] == PKTV1_PONG_WORD, "PING: pong word 0x504F");
    check(rsp.status == PKTV1_DISPATCH_OK, "PING: status OK");

    /* 2. GET_VERSION */
    req = make_req(PKTV1_CMD_GET_VERSION, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "GET_VERSION: returns OK");
    check(rsp.command_id == PKTV1_CMD_GET_VERSION, "GET_VERSION: response command_id");
    check(rsp.payload_length_words == 4U, "GET_VERSION: 4-word reply");
    check(rsp.payload[0] == PKTV1_VERSION_MAJOR, "GET_VERSION: major");
    check(rsp.payload[1] == PKTV1_VERSION_MINOR, "GET_VERSION: minor");
    check(rsp.payload[2] == PKTV1_VERSION_PATCH, "GET_VERSION: patch");
    check(rsp.payload[3] == PKTV1_SPEC_REV, "GET_VERSION: spec rev");

    /* 3. GET_CAPS */
    req = make_req(PKTV1_CMD_GET_CAPS, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "GET_CAPS: returns OK");
    check(rsp.command_id == PKTV1_CMD_GET_CAPS, "GET_CAPS: response command_id");
    check(rsp.payload_length_words == 3U, "GET_CAPS: 3-word reply");
    check(rsp.payload[0] == (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS,
          "GET_CAPS: advertises max payload words");
    check(rsp.payload[1] == PKTV1_CAP_FEATURES, "GET_CAPS: meta feature flags");
    check(rsp.payload[2] == PKTV1_CAP_CRC_ALGO_CCITT_FALSE, "GET_CAPS: CRC algo id");
}

/* ------------------------------------------------------------------ */
/* 4-7: ECHO success (len 0, 1, multi, max)                           */
/* 8:   ECHO overflow                                                 */
/* ------------------------------------------------------------------ */
static void test_echo(void)
{
    ST_SPI_PACKET_V1      req;
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc;
    uint16_t              big[PKTV1_DISPATCH_MAX_PAYLOAD_WORDS + 1U];
    uint16_t              three[3] = { 0xAAAAU, 0x5555U, 0x0001U };
    uint16_t              one      = 0x1234U;
    unsigned              i;
    int                   ok;

    for (i = 0U; i <= (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        big[i] = (uint16_t)((i * 7U) + 1U);
    }

    /* 4. ECHO length 0 */
    req = make_req(PKTV1_CMD_ECHO, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "ECHO len0: returns OK");
    check(rsp.command_id == PKTV1_CMD_ECHO, "ECHO len0: response command_id");
    check(rsp.payload_length_words == 0U, "ECHO len0: empty reply");

    /* 5. ECHO length 1 */
    req = make_req(PKTV1_CMD_ECHO, &one, 1U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "ECHO len1: returns OK");
    check(rsp.payload_length_words == 1U, "ECHO len1: 1-word reply");
    check(rsp.payload[0] == 0x1234U, "ECHO len1: word preserved");

    /* 6. ECHO multi-word */
    req = make_req(PKTV1_CMD_ECHO, three, 3U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "ECHO 3w: returns OK");
    check(rsp.payload_length_words == 3U, "ECHO 3w: 3-word reply");
    ok = (rsp.payload[0] == 0xAAAAU) && (rsp.payload[1] == 0x5555U) &&
         (rsp.payload[2] == 0x0001U);
    check(ok, "ECHO 3w: payload preserved");

    /* 7. ECHO max allowed length */
    req = make_req(PKTV1_CMD_ECHO, big, (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_OK, "ECHO max: returns OK");
    check(rsp.payload_length_words == (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS,
          "ECHO max: full-length reply");
    ok = 1;
    for (i = 0U; i < (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        if (rsp.payload[i] != big[i]) { ok = 0; }
    }
    check(ok, "ECHO max: every word preserved");

    /* 8. ECHO one word over capacity -> RSP_OVERFLOW */
    req = make_req(PKTV1_CMD_ECHO, big,
                   (uint16_t)(PKTV1_DISPATCH_MAX_PAYLOAD_WORDS + 1U));
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_RSP_OVERFLOW, "ECHO over-cap: RSP_OVERFLOW");
    check(rsp.command_id == PKTV1_RSP_ERROR, "ECHO over-cap: RSP_ERROR command_id");
    check(rsp.payload[0] == PKTV1_CMD_ECHO, "ECHO over-cap: original cmd preserved");
    check(rsp.payload[1] == PKTV1_ERRCODE_RSP_OVERFLOW, "ECHO over-cap: errcode RSP_OVERFLOW");
}

/* ------------------------------------------------------------------ */
/* 9-11: bad length on zero-payload commands                          */
/* ------------------------------------------------------------------ */
static void test_bad_length(void)
{
    expect_bad_length(PKTV1_CMD_PING,        "PING+payload: BAD_LENGTH");
    expect_bad_length(PKTV1_CMD_GET_VERSION, "GET_VERSION+payload: BAD_LENGTH");
    expect_bad_length(PKTV1_CMD_GET_CAPS,    "GET_CAPS+payload: BAD_LENGTH");
}

/* ------------------------------------------------------------------ */
/* 12-19: forbidden command IDs                                       */
/* ------------------------------------------------------------------ */
static void test_forbidden(void)
{
    expect_forbidden(0x0400U, "cmd 0x0400 (legacy RO): FORBIDDEN");
    expect_forbidden(0x0958U, "cmd 0x0958 (wave page select): FORBIDDEN");
    expect_forbidden(0x0960U, "cmd 0x0960 (wave validate): FORBIDDEN");
    expect_forbidden(0x3000U, "cmd 0x3000 (block base): FORBIDDEN");
    expect_forbidden(0x3FFFU, "cmd 0x3FFF (block end): FORBIDDEN");
    expect_forbidden(0xA55AU, "cmd 0xA55A (header magic): FORBIDDEN");
    expect_forbidden(0x0000U, "cmd 0x0000 (NULL/below namespace): FORBIDDEN");
    expect_forbidden(0x7FFFU, "cmd 0x7FFF (below 0x8000): FORBIDDEN");
}

/* ------------------------------------------------------------------ */
/* 20-21: unknown commands in the 0x8000+ namespace                   */
/* ------------------------------------------------------------------ */
static void test_unsupported(void)
{
    ST_SPI_PACKET_V1      req;
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc;

    req = make_req(0x8004U, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD, "cmd 0x8004: UNSUPPORTED");
    check(rsp.command_id == PKTV1_RSP_ERROR, "0x8004: RSP_ERROR command_id");
    check(rsp.payload[0] == 0x8004U, "0x8004: original cmd preserved");
    check(rsp.payload[1] == PKTV1_ERRCODE_UNSUPPORTED, "0x8004: errcode UNSUPPORTED");

    req = make_req(0xFFFFU, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD, "cmd 0xFFFF: UNSUPPORTED");
    check(rsp.payload[0] == 0xFFFFU, "0xFFFF: original cmd preserved");

    /* PKTV1_RSP_ERROR (0x80FF) is response-only -> not a valid request. */
    req = make_req(PKTV1_RSP_ERROR, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD, "cmd 0x80FF (response-only): UNSUPPORTED");
}

/* ------------------------------------------------------------------ */
/* 22-24: NULL / malformed-argument handling                          */
/* ------------------------------------------------------------------ */
static void test_null(void)
{
    ST_SPI_PACKET_V1      req;
    ST_PKTV1_DISPATCH_RSP rsp;
    PKTV1_DISPATCH_RESULT_e rc;

    /* 22. NULL request pointer */
    rc = SpiPacketV1_Dispatch(NULL, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_NULL, "NULL request: ERR_NULL");
    check(rsp.status == PKTV1_DISPATCH_ERR_NULL, "NULL request: rsp.status ERR_NULL");
    check(rsp.command_id == 0U, "NULL request: rsp zeroed (command_id)");
    check(rsp.payload_length_words == 0U, "NULL request: rsp zeroed (length)");

    /* 23. NULL response pointer */
    req = make_req(PKTV1_CMD_PING, NULL, 0U);
    rc  = SpiPacketV1_Dispatch(&req, NULL);
    check(rc == PKTV1_DISPATCH_ERR_NULL, "NULL response: ERR_NULL");

    /* 24. NULL payload with nonzero length (fixed rule: ERR_NULL) */
    req = make_req(PKTV1_CMD_ECHO, NULL, 1U);
    rc  = SpiPacketV1_Dispatch(&req, &rsp);
    check(rc == PKTV1_DISPATCH_ERR_NULL, "NULL payload + len>0: ERR_NULL");
    check(rsp.command_id == 0U, "NULL payload + len>0: rsp zeroed (no error envelope)");
}

/* ------------------------------------------------------------------ */
/* 25: determinism (no hidden global state)                           */
/* ------------------------------------------------------------------ */
static void test_determinism(void)
{
    uint16_t              pl[2] = { 0x1357U, 0x2468U };
    ST_SPI_PACKET_V1      req   = make_req(PKTV1_CMD_ECHO, pl, 2U);
    ST_PKTV1_DISPATCH_RSP a;
    ST_PKTV1_DISPATCH_RSP b;
    unsigned              i;
    int                   same;

    (void)SpiPacketV1_Dispatch(&req, &a);
    (void)SpiPacketV1_Dispatch(&req, &b);

    same = (a.command_id == b.command_id) &&
           (a.payload_length_words == b.payload_length_words) &&
           (a.status == b.status);
    for (i = 0U; i < (unsigned)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        if (a.payload[i] != b.payload[i]) { same = 0; }
    }
    check(same, "determinism: repeated dispatch identical");

    /* A forbidden command is also deterministic. */
    {
        ST_SPI_PACKET_V1      freq = make_req(0x0958U, NULL, 0U);
        ST_PKTV1_DISPATCH_RSP fa;
        ST_PKTV1_DISPATCH_RSP fb;
        PKTV1_DISPATCH_RESULT_e ra = SpiPacketV1_Dispatch(&freq, &fa);
        PKTV1_DISPATCH_RESULT_e rb = SpiPacketV1_Dispatch(&freq, &fb);
        check((ra == rb) && (fa.command_id == fb.command_id) &&
              (fa.payload[0] == fb.payload[0]) && (fa.payload[1] == fb.payload[1]),
              "determinism: repeated forbidden dispatch identical");
    }
}

/* ------------------------------------------------------------------ */
/* 26: response payload must not alias the request payload            */
/* ------------------------------------------------------------------ */
static void test_no_alias(void)
{
    uint16_t              in_pl[3] = { 0x1111U, 0x2222U, 0x3333U };
    ST_SPI_PACKET_V1      req      = make_req(PKTV1_CMD_ECHO, in_pl, 3U);
    ST_PKTV1_DISPATCH_RSP rsp;

    (void)SpiPacketV1_Dispatch(&req, &rsp);

    check((const void *)rsp.payload != (const void *)req.payload,
          "no-alias: response buffer distinct from request payload");

    /* Mutating the input after dispatch must not change the response copy. */
    in_pl[0] = 0xDEADU;
    check(rsp.payload[0] == 0x1111U, "no-alias: response unaffected by input mutation");
}

int main(void)
{
    printf("=== SPI Packet V1 A4 dispatcher mock test ===\n");

    test_meta_success();     /* 1-3   */
    test_echo();             /* 4-8   */
    test_bad_length();       /* 9-11  */
    test_forbidden();        /* 12-19 */
    test_unsupported();      /* 20-21 */
    test_null();             /* 22-24 */
    test_determinism();      /* 25    */
    test_no_alias();         /* 26    */

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
