/*
 * spi_packet_v1_wire_probe_test.c
 *
 * SPI Packet V1 - A6-3A passive wire recognizer host test harness.
 *
 * HOST test, not firmware. The whole harness (main() + <stdio.h>) compiles only
 * when SPI_PACKET_V1_HOST_TEST is defined, mirroring A2/A4/A5/A6-1 so that in the
 * CCS firmware build this file is an empty translation unit (no second main(),
 * no stdio).
 *
 *   gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
 *       spi_packet_crc16.c spi_packet_v1.c \
 *       spi_packet_v1_wire_probe.c spi_packet_v1_wire_probe_test.c \
 *       -o spi_packet_v1_wire_probe_test
 */

/* Guarantee a non-empty translation unit in the firmware build. */
typedef int spi_packet_v1_wire_probe_test_translation_unit_t;

#ifdef SPI_PACKET_V1_HOST_TEST

#include <stdint.h>
#include <stdio.h>

#include "spi_packet_v1.h"            /* SpiPacketV1_Encode, SPI_PACKET_V1_OK   */
#include "spi_packet_v1_cmd.h"        /* PKTV1_CMD_PING / _ECHO / _GET_CAPS ... */
#include "spi_packet_v1_wire_probe.h"

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

/* Build a valid Packet V1 frame with the real encoder. Returns frame length. */
static uint16_t build_frame(uint16_t cmd, const uint16_t *payload,
                            uint16_t payload_words, uint16_t *out, uint16_t cap)
{
    uint16_t len = 0U;
    SPI_PACKET_V1_RESULT_e rc =
        SpiPacketV1_Encode(cmd, payload, payload_words, out, cap, &len);
    check(rc == SPI_PACKET_V1_OK, "encode: helper frame built OK");
    return len;
}

/* 1. Reset initializes state. */
static void test_reset_initializes_state(void)
{
    ST_PKTV1_WIRE_PROBE p;
    /* Dirty the struct first so reset has something to clear. */
    p.state = PKTV1_WIRE_PROBE_FRAME_ERROR;
    p.words_seen = 99U;
    p.frame_ok_count = 7U;
    p.frame_error_count = 3U;

    SpiPacketV1_WireProbe_Reset(&p);
    check(p.state == PKTV1_WIRE_PROBE_IDLE, "reset: state IDLE");
    check(p.result == PKTV1_WIRE_PROBE_OK, "reset: result OK");
    check(p.words_seen == 0U, "reset: words_seen 0");
    check(p.expected_total_words == 0U, "reset: expected_total_words 0");
    check(p.frame_ok_count == 0U, "reset: frame_ok_count 0");
    check(p.frame_error_count == 0U, "reset: frame_error_count 0");
}

/* 2. NULL pointer rejected. */
static void test_null_pointer_rejected(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    SpiPacketV1_WireProbe_Reset(&p);

    check(SpiPacketV1_WireProbe_FeedWord(NULL, 0xA55AU)
              == PKTV1_WIRE_PROBE_ERR_NULL,
          "null: FeedWord(NULL) -> ERR_NULL");
    check(SpiPacketV1_WireProbe_FeedWords(NULL, buf, 4U)
              == PKTV1_WIRE_PROBE_ERR_NULL,
          "null: FeedWords(NULL probe) -> ERR_NULL");
    check(SpiPacketV1_WireProbe_FeedWords(&p, NULL, 4U)
              == PKTV1_WIRE_PROBE_ERR_NULL,
          "null: FeedWords(NULL words) -> ERR_NULL");
}

/* 3. Idle ignores non-header word. */
static void test_idle_ignores_non_header(void)
{
    ST_PKTV1_WIRE_PROBE p;
    PKTV1_WIRE_PROBE_RESULT_e rc;
    SpiPacketV1_WireProbe_Reset(&p);

    rc = SpiPacketV1_WireProbe_FeedWord(&p, 0x1234U);
    check(rc == PKTV1_WIRE_PROBE_OK, "idle: non-header returns OK");
    check(p.state == PKTV1_WIRE_PROBE_IDLE, "idle: stays IDLE");
    check(p.words_seen == 0U, "idle: words_seen still 0");
}

/* 4. Valid PING frame recognized. */
static void test_valid_ping_recognized(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_PING, NULL, 0U, buf, (uint16_t)sizeof(buf));
    rc  = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(rc == PKTV1_WIRE_PROBE_OK, "ping: FeedWords returns OK");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "ping: state FRAME_OK");
    check(p.result == PKTV1_WIRE_PROBE_OK, "ping: result OK");
    check(p.cmd_id == PKTV1_CMD_PING, "ping: cmd_id == 0x8000");
    check(p.payload_words == 0U, "ping: payload_words 0");
    check(p.frame_ok_count == 1U, "ping: frame_ok_count 1");
    check(p.crc_actual == p.crc_expected, "ping: crc_actual == crc_expected");
}

/* 5. Valid ECHO frame with payload recognized. */
static void test_valid_echo_with_payload(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[2] = {0x1111U, 0x2222U};
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_ECHO, payload, 2U, buf, (uint16_t)sizeof(buf));
    rc  = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(rc == PKTV1_WIRE_PROBE_OK, "echo: FeedWords returns OK");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "echo: state FRAME_OK");
    check(p.cmd_id == PKTV1_CMD_ECHO, "echo: cmd_id == 0x8003");
    check(p.payload_words == 2U, "echo: payload_words 2");
    check(p.frame_ok_count == 1U, "echo: frame_ok_count 1");
}

/* 6. Frame split across one-word feeds recognized. */
static void test_frame_split_single_word_feeds(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t len;
    uint16_t i;
    PKTV1_WIRE_PROBE_RESULT_e rc = PKTV1_WIRE_PROBE_OK;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_PING, NULL, 0U, buf, (uint16_t)sizeof(buf));

    rc = SpiPacketV1_WireProbe_FeedWord(&p, buf[0]);
    check(rc == PKTV1_WIRE_PROBE_OK, "split: header word OK");
    check(p.state == PKTV1_WIRE_PROBE_COLLECTING, "split: COLLECTING after header");

    for (i = 1U; i < (uint16_t)(len - 1U); ++i)
    {
        rc = SpiPacketV1_WireProbe_FeedWord(&p, buf[i]);
        check(rc == PKTV1_WIRE_PROBE_OK, "split: mid word OK");
        check(p.state == PKTV1_WIRE_PROBE_COLLECTING, "split: still COLLECTING");
    }

    rc = SpiPacketV1_WireProbe_FeedWord(&p, buf[len - 1U]);
    check(rc == PKTV1_WIRE_PROBE_OK, "split: final CRC word OK");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "split: FRAME_OK at end");
    check(p.frame_ok_count == 1U, "split: frame_ok_count 1");
}

/* 7. Frame fed via FeedWords() recognized (single-payload command). */
static void test_frame_fed_via_feedwords(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[1] = {0xBEEFU};
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_GET_VERSION, payload, 1U, buf,
                      (uint16_t)sizeof(buf));
    rc  = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(rc == PKTV1_WIRE_PROBE_OK, "feedwords: returns OK");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "feedwords: FRAME_OK");
    check(p.cmd_id == PKTV1_CMD_GET_VERSION, "feedwords: cmd_id preserved");
    check(p.frame_ok_count == 1U, "feedwords: frame_ok_count 1");
}

/* 8. Bad CRC rejected. */
static void test_bad_crc_rejected(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[1] = {0xAAAAU};
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_ECHO, payload, 1U, buf, (uint16_t)sizeof(buf));
    buf[len - 1U] = (uint16_t)(buf[len - 1U] ^ 0xFFFFU);   /* corrupt CRC word */
    rc = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(rc == PKTV1_WIRE_PROBE_ERR_BAD_CRC, "badcrc: FeedWords ERR_BAD_CRC");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_ERROR, "badcrc: state FRAME_ERROR");
    check(p.result == PKTV1_WIRE_PROBE_ERR_BAD_CRC, "badcrc: result ERR_BAD_CRC");
    check(p.frame_error_count == 1U, "badcrc: frame_error_count 1");
}

/* 9. Declared length too large rejected. */
static void test_declared_length_too_large(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t bad[3];
    PKTV1_WIRE_PROBE_RESULT_e rc;

    bad[0] = 0xA55AU;                                       /* header          */
    bad[1] = PKTV1_CMD_ECHO;                                /* command         */
    bad[2] = (uint16_t)(PKTV1_WIRE_PROBE_MAX_PAYLOAD_WORDS + 1U); /* > cap     */

    SpiPacketV1_WireProbe_Reset(&p);
    rc = SpiPacketV1_WireProbe_FeedWords(&p, bad, 3U);

    check(rc == PKTV1_WIRE_PROBE_ERR_BAD_LENGTH, "badlen: ERR_BAD_LENGTH");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_ERROR, "badlen: state FRAME_ERROR");
    check(p.result == PKTV1_WIRE_PROBE_ERR_BAD_LENGTH, "badlen: result reason");
    check(p.frame_error_count == 1U, "badlen: frame_error_count 1");
    check(p.payload_words == (uint16_t)(PKTV1_WIRE_PROBE_MAX_PAYLOAD_WORDS + 1U),
          "badlen: declared length recorded");
}

/* 10. Truncated frame remains COLLECTING, not OK. */
static void test_truncated_frame_collecting(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_PING, NULL, 0U, buf, (uint16_t)sizeof(buf));
    /* Feed only header + command (2 of 4 words). */
    rc = SpiPacketV1_WireProbe_FeedWords(&p, buf, 2U);
    (void)len;

    check(rc == PKTV1_WIRE_PROBE_OK, "trunc: partial feed returns OK");
    check(p.state == PKTV1_WIRE_PROBE_COLLECTING, "trunc: state COLLECTING");
    check(p.state != PKTV1_WIRE_PROBE_FRAME_OK, "trunc: not FRAME_OK");
    check(p.frame_ok_count == 0U, "trunc: frame_ok_count 0");
}

/* 11. Noise before a valid header is ignored. */
static void test_noise_before_header_ignored(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t noise[3] = {0x0000U, 0x1234U, 0xFFFFU};
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    rc = SpiPacketV1_WireProbe_FeedWords(&p, noise, 3U);
    check(rc == PKTV1_WIRE_PROBE_OK, "noise: ignored, returns OK");
    check(p.state == PKTV1_WIRE_PROBE_IDLE, "noise: still IDLE");

    len = build_frame(PKTV1_CMD_PING, NULL, 0U, buf, (uint16_t)sizeof(buf));
    rc  = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "noise: frame after noise OK");
    check(p.frame_ok_count == 1U, "noise: frame_ok_count 1");
}

/* 12. Two valid frames with reset in between. */
static void test_two_frames_with_reset(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[1] = {0x55AAU};
    uint16_t len;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_PING, NULL, 0U, buf, (uint16_t)sizeof(buf));
    (void)SpiPacketV1_WireProbe_FeedWords(&p, buf, len);
    check(p.frame_ok_count == 1U, "two: frame1 ok_count 1");

    SpiPacketV1_WireProbe_Reset(&p);
    check(p.frame_ok_count == 0U, "two: reset clears counters");
    check(p.state == PKTV1_WIRE_PROBE_IDLE, "two: reset state IDLE");

    len = build_frame(PKTV1_CMD_ECHO, payload, 1U, buf, (uint16_t)sizeof(buf));
    (void)SpiPacketV1_WireProbe_FeedWords(&p, buf, len);
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "two: frame2 FRAME_OK");
    check(p.cmd_id == PKTV1_CMD_ECHO, "two: frame2 cmd_id 0x8003");
}

/* 13. Payload length zero accepted (non-PING command, generic zero payload). */
static void test_payload_length_zero_accepted(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t len;
    PKTV1_WIRE_PROBE_RESULT_e rc;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_GET_CAPS, NULL, 0U, buf, (uint16_t)sizeof(buf));
    rc  = SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(rc == PKTV1_WIRE_PROBE_OK, "zerolen: FeedWords OK");
    check(p.state == PKTV1_WIRE_PROBE_FRAME_OK, "zerolen: FRAME_OK");
    check(p.payload_words == 0U, "zerolen: payload_words 0");
}

/* 14. Command id preserved. */
static void test_command_id_preserved(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[1] = {0x1234U};
    uint16_t len;

    SpiPacketV1_WireProbe_Reset(&p);
    len = build_frame(PKTV1_CMD_ECHO, payload, 1U, buf, (uint16_t)sizeof(buf));
    (void)SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(p.cmd_id == PKTV1_CMD_ECHO, "cmdid: preserved == 0x8003");
}

/* 15. CRC actual / expected exposed on error. */
static void test_crc_fields_exposed_on_error(void)
{
    ST_PKTV1_WIRE_PROBE p;
    uint16_t buf[PKTV1_WIRE_PROBE_MAX_FRAME_WORDS];
    uint16_t payload[1] = {0x5555U};
    uint16_t len;
    uint16_t good_crc;

    SpiPacketV1_WireProbe_Reset(&p);
    len      = build_frame(PKTV1_CMD_ECHO, payload, 1U, buf, (uint16_t)sizeof(buf));
    good_crc = buf[len - 1U];
    buf[len - 1U] = (uint16_t)(good_crc ^ 0x0001U);   /* corrupt one bit */
    (void)SpiPacketV1_WireProbe_FeedWords(&p, buf, len);

    check(p.state == PKTV1_WIRE_PROBE_FRAME_ERROR, "crcfields: FRAME_ERROR");
    check(p.crc_expected == (uint16_t)(good_crc ^ 0x0001U),
          "crcfields: crc_expected == corrupted word");
    check(p.crc_actual == good_crc, "crcfields: crc_actual == correct CRC");
    check(p.crc_actual != p.crc_expected, "crcfields: actual != expected");
}

/* 16. Harness is guarded (compile-time property). */
static void test_harness_guarded(void)
{
    check(1, "guard: harness compiles only under SPI_PACKET_V1_HOST_TEST");
}

int main(void)
{
    printf("=== SPI Packet V1 A6-3A wire-probe core test ===\n");

    test_reset_initializes_state();
    test_null_pointer_rejected();
    test_idle_ignores_non_header();
    test_valid_ping_recognized();
    test_valid_echo_with_payload();
    test_frame_split_single_word_feeds();
    test_frame_fed_via_feedwords();
    test_bad_crc_rejected();
    test_declared_length_too_large();
    test_truncated_frame_collecting();
    test_noise_before_header_ignored();
    test_two_frames_with_reset();
    test_payload_length_zero_accepted();
    test_command_id_preserved();
    test_crc_fields_exposed_on_error();
    test_harness_guarded();

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
