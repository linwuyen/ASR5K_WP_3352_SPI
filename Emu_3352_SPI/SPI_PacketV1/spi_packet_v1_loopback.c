/*
 * spi_packet_v1_loopback.c
 *
 * SPI Packet V1 - pure-C loopback path (A5). See header for the contract.
 *
 * This module is glue only: it wires the existing A2 parser/encoder and the A4
 * dispatcher together. It defines no new wire behavior, no globals, and touches
 * no transport. The response frame is built with the existing A2 encoder
 * (SpiPacketV1_Encode), which already uses the shared CRC16-CCITT-FALSE.
 */

#include "spi_packet_v1_loopback.h"

#include "spi_packet_v1.h"          /* SpiPacketV1_ParseWords / _Encode, types */
#include "spi_packet_v1_dispatch.h" /* SpiPacketV1_Dispatch, response model     */

#ifndef NULL
#define NULL ((void *)0)
#endif

/* Set diag->result (if diag present) and return the result. */
static PKTV1_LOOPBACK_RESULT_e finish(ST_PKTV1_LOOPBACK_DIAG *diag,
                                      PKTV1_LOOPBACK_RESULT_e r)
{
    if (diag != NULL)
    {
        diag->result = r;
    }
    return r;
}

PKTV1_LOOPBACK_RESULT_e SpiPacketV1_Loopback(const uint16_t         *req_words,
                                             uint16_t                req_word_count,
                                             uint16_t               *rsp_words,
                                             uint16_t                rsp_word_capacity,
                                             uint16_t               *rsp_word_count,
                                             ST_PKTV1_LOOPBACK_DIAG *diag)
{
    ST_SPI_PACKET_V1        parsed;
    ST_PKTV1_DISPATCH_RSP   rsp;
    SPI_PACKET_V1_RESULT_e  pr;
    PKTV1_DISPATCH_RESULT_e dr;
    SPI_PACKET_V1_RESULT_e  er;
    uint16_t                out_len = 0U;

    /* Clear diagnostics up front so untouched stages read as 0. */
    if (diag != NULL)
    {
        diag->result          = PKTV1_LOOPBACK_OK;
        diag->parser_result   = 0U;
        diag->dispatch_result = 0U;
        diag->request_cmd     = 0U;
        diag->response_cmd    = 0U;
        diag->response_words  = 0U;
    }

    if ((req_words == NULL) || (rsp_words == NULL) || (rsp_word_count == NULL))
    {
        if (rsp_word_count != NULL)
        {
            *rsp_word_count = 0U;
        }
        return finish(diag, PKTV1_LOOPBACK_ERR_NULL);
    }

    *rsp_word_count = 0U;   /* default: no response frame produced */

    /* --- Stage 1: A2 parser (owns all framing / CRC validation) --- */
    pr = SpiPacketV1_ParseWords(req_words, req_word_count, &parsed);
    if (diag != NULL)
    {
        diag->parser_result = (uint16_t)pr;
    }
    if (pr != SPI_PACKET_V1_OK)
    {
        /* Malformed framing belongs to A2; never dispatch, never fabricate a
         * response for a bad header / bad CRC / length mismatch. */
        return finish(diag, PKTV1_LOOPBACK_ERR_PARSE);
    }
    if (diag != NULL)
    {
        diag->request_cmd = parsed.cmdId;
    }

    /* --- Stage 2: A4 dispatcher (command semantics) --- */
    dr = SpiPacketV1_Dispatch(&parsed, &rsp);
    if (diag != NULL)
    {
        diag->dispatch_result = (uint16_t)dr;
    }
    if (dr == PKTV1_DISPATCH_ERR_NULL)
    {
        /* Cannot happen after a successful parse (req + payload are valid);
         * kept as a defensive safety net - no response model to encode. */
        return finish(diag, PKTV1_LOOPBACK_ERR_DISPATCH);
    }

    /*
     * Every other dispatch outcome (OK, plus the semantic errors FORBIDDEN /
     * UNSUPPORTED / BAD_LENGTH / RSP_OVERFLOW) leaves a valid response model in
     * `rsp`: a normal reply on OK, or a PKTV1_RSP_ERROR envelope on error. Both
     * are encoded into a valid response frame below.
     */

    /* --- Stage 3: reuse the A2 encoder (shared CRC) --- */
    er = SpiPacketV1_Encode(rsp.command_id,
                            rsp.payload,
                            rsp.payload_length_words,
                            rsp_words,
                            rsp_word_capacity,
                            &out_len);
    if (er == SPI_PACKET_V1_ERR_BUFFER_TOO_SMALL)
    {
        *rsp_word_count = 0U;   /* encoder validated capacity before writing */
        return finish(diag, PKTV1_LOOPBACK_ERR_RSP_OVERFLOW);
    }
    if (er != SPI_PACKET_V1_OK)
    {
        *rsp_word_count = 0U;
        return finish(diag, PKTV1_LOOPBACK_ERR_ENCODE);
    }

    *rsp_word_count = out_len;
    if (diag != NULL)
    {
        diag->response_cmd   = rsp.command_id;
        diag->response_words = out_len;
    }
    return finish(diag, PKTV1_LOOPBACK_OK);
}
