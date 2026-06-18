/*
 * spi_packet_v1_probe_test.c
 *
 * SPI Packet V1 - PING probe (A6-1) host test harness.
 *
 * HOST test, not firmware. The whole harness (main() + <stdio.h>) compiles only
 * when SPI_PACKET_V1_HOST_TEST is defined, mirroring A2/A4/A5 so that in the CCS
 * firmware build this file is an empty translation unit (no second main()).
 *
 *   gcc -std=c99 -Wall -Wextra -Werror -DSPI_PACKET_V1_HOST_TEST \
 *       spi_packet_crc16.c spi_packet_v1.c spi_packet_v1_dispatch.c \
 *       spi_packet_v1_loopback.c spi_packet_v1_probe.c \
 *       spi_packet_v1_probe_test.c -o spi_packet_v1_probe_test
 */

/* Guarantee a non-empty translation unit in the firmware build. */
typedef int spi_packet_v1_probe_test_translation_unit_t;

#ifdef SPI_PACKET_V1_HOST_TEST

#include <stdint.h>
#include <stdio.h>

#include "spi_packet_v1_cmd.h"
#include "spi_packet_v1_probe.h"

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

static void test_ping_probe_success(void)
{
    ST_PKTV1_PROBE_RESULT  r;
    PKTV1_PROBE_RESULT_e   rc = SpiPacketV1_RunPingProbe(&r);

    check(rc == PKTV1_PROBE_OK, "probe: returns PKTV1_PROBE_OK");
    check(r.result == PKTV1_PROBE_OK, "probe: result field OK");
    check(r.response_cmd == PKTV1_CMD_PING, "probe: response_cmd == 0x8000");
    check(r.response_cmd == 0x8000U, "probe: response_cmd literal 0x8000");
    check(r.pong_word == PKTV1_PONG_WORD, "probe: pong_word == 0x504F");
    check(r.pong_word == 0x504FU, "probe: pong_word literal 0x504F");
    check(r.response_payload_words == 1U, "probe: response_payload_words == 1");
    check(r.response_words >= 4U, "probe: response_words >= 4");
    check(r.request_words >= 4U, "probe: request_words >= 4");
    check(r.loopback_result == 0U, "probe: loopback_result OK (0)");
    check(r.parser_result == 0U, "probe: request parser_result OK (0)");
    check(r.dispatch_result == 0U, "probe: dispatch_result OK (0)");
}

static void test_ping_probe_null(void)
{
    PKTV1_PROBE_RESULT_e rc = SpiPacketV1_RunPingProbe(NULL);
    check(rc == PKTV1_PROBE_ERR_NULL, "probe: NULL output -> ERR_NULL");
}

static void test_ping_probe_deterministic(void)
{
    ST_PKTV1_PROBE_RESULT a;
    ST_PKTV1_PROBE_RESULT b;
    int                   same;

    (void)SpiPacketV1_RunPingProbe(&a);
    (void)SpiPacketV1_RunPingProbe(&b);

    same = (a.result == b.result) &&
           (a.loopback_result == b.loopback_result) &&
           (a.parser_result == b.parser_result) &&
           (a.dispatch_result == b.dispatch_result) &&
           (a.response_cmd == b.response_cmd) &&
           (a.response_payload_words == b.response_payload_words) &&
           (a.pong_word == b.pong_word) &&
           (a.request_words == b.request_words) &&
           (a.response_words == b.response_words);
    check(same, "probe: repeated call deterministic");
}

int main(void)
{
    printf("=== SPI Packet V1 A6-1 PING probe test ===\n");

    test_ping_probe_success();
    test_ping_probe_null();
    test_ping_probe_deterministic();

    printf("---------------------------------\n");
    printf("PASS=%d  FAIL=%d\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}

#endif /* SPI_PACKET_V1_HOST_TEST */
