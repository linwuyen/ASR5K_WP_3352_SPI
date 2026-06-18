/*
 * spi_packet_v1_probe.c
 *
 * SPI Packet V1 - board-observable PING probe (A6-1). See header for contract.
 *
 * Glue only: reuses the A2 encoder/parser, the A4 dispatcher (via A5 loopback),
 * and validates the PONG reply. No transport, no globals, no runtime coupling.
 *
 * C28x note: never use sizeof(buf)/2 to derive word capacity - on C28x
 * CHAR_BIT==16 so sizeof() already counts 16-bit words. Capacities are passed
 * as explicit word counts.
 */

#include "spi_packet_v1_probe.h"

#include "spi_packet_v1.h"          /* SpiPacketV1_Encode / _ParseWords, types  */
#include "spi_packet_v1_cmd.h"      /* PKTV1_CMD_PING, PKTV1_PONG_WORD          */
#include "spi_packet_v1_loopback.h" /* SpiPacketV1_Loopback, diag               */

#ifndef NULL
#define NULL ((void *)0)
#endif

/* PING request = overhead only (no payload) = 4 words. Response = 5 words. */
#define PKTV1_PROBE_REQ_CAP_WORDS  SPI_PACKET_V1_OVERHEAD_WORDS  /* 4 */
#define PKTV1_PROBE_RSP_CAP_WORDS  16U

static void probe_clear(ST_PKTV1_PROBE_RESULT *out)
{
    out->result                 = PKTV1_PROBE_OK;
    out->loopback_result        = 0U;
    out->parser_result          = 0U;
    out->dispatch_result        = 0U;
    out->response_cmd           = 0U;
    out->response_payload_words = 0U;
    out->pong_word              = 0U;
    out->request_words          = 0U;
    out->response_words         = 0U;
}

PKTV1_PROBE_RESULT_e SpiPacketV1_RunPingProbe(ST_PKTV1_PROBE_RESULT *out)
{
    uint16_t                req[PKTV1_PROBE_REQ_CAP_WORDS];
    uint16_t                rsp[PKTV1_PROBE_RSP_CAP_WORDS];
    uint16_t                req_len = 0U;
    uint16_t                rsp_len = 0U;
    SPI_PACKET_V1_RESULT_e  er;
    PKTV1_LOOPBACK_RESULT_e lr;
    SPI_PACKET_V1_RESULT_e  pr;
    ST_PKTV1_LOOPBACK_DIAG  diag;
    ST_SPI_PACKET_V1        parsed_rsp;

    if (out == NULL)
    {
        return PKTV1_PROBE_ERR_NULL;
    }
    probe_clear(out);

    /* 1. Build the PING request with the A2 encoder. */
    er = SpiPacketV1_Encode(PKTV1_CMD_PING, NULL, 0U,
                            req, (uint16_t)PKTV1_PROBE_REQ_CAP_WORDS, &req_len);
    if (er != SPI_PACKET_V1_OK)
    {
        out->result = PKTV1_PROBE_ERR_ENCODE;
        return PKTV1_PROBE_ERR_ENCODE;
    }
    out->request_words = req_len;

    /* 2. Run the full A5 loopback (parse -> dispatch -> encode). */
    lr = SpiPacketV1_Loopback(req, req_len,
                              rsp, (uint16_t)PKTV1_PROBE_RSP_CAP_WORDS,
                              &rsp_len, &diag);
    out->loopback_result = (uint16_t)lr;
    out->parser_result   = diag.parser_result;   /* request parse stage */
    out->dispatch_result = diag.dispatch_result;
    out->response_words  = rsp_len;
    if (lr != PKTV1_LOOPBACK_OK)
    {
        out->result = PKTV1_PROBE_ERR_LOOPBACK;
        return PKTV1_PROBE_ERR_LOOPBACK;
    }

    /* 3. Parse the response frame with the A2 parser (CRC re-validated). */
    pr = SpiPacketV1_ParseWords(rsp, rsp_len, &parsed_rsp);
    if (pr != SPI_PACKET_V1_OK)
    {
        out->result = PKTV1_PROBE_ERR_PARSE_RSP;
        return PKTV1_PROBE_ERR_PARSE_RSP;
    }

    out->response_cmd           = parsed_rsp.cmdId;
    out->response_payload_words = parsed_rsp.payloadWords;
    if ((parsed_rsp.payloadWords >= 1U) && (parsed_rsp.payload != NULL))
    {
        out->pong_word = parsed_rsp.payload[0];
    }

    /* 4. Validate the PONG. */
    if (parsed_rsp.cmdId != PKTV1_CMD_PING)
    {
        out->result = PKTV1_PROBE_ERR_BAD_RSP_CMD;
        return PKTV1_PROBE_ERR_BAD_RSP_CMD;
    }
    if (parsed_rsp.payloadWords != 1U)
    {
        out->result = PKTV1_PROBE_ERR_BAD_RSP_LEN;
        return PKTV1_PROBE_ERR_BAD_RSP_LEN;
    }
    if (out->pong_word != PKTV1_PONG_WORD)
    {
        out->result = PKTV1_PROBE_ERR_BAD_PONG;
        return PKTV1_PROBE_ERR_BAD_PONG;
    }

    out->result = PKTV1_PROBE_OK;
    return PKTV1_PROBE_OK;
}
