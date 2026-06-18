/*
 * spi_packet_v1_dispatch.c
 *
 * SPI Packet V1 - pure-C command dispatcher mock (A4). See header for the
 * contract. This file contains no globals and touches no transport: it maps a
 * validated request view to a deterministic response object.
 */

#include "spi_packet_v1_dispatch.h"

/* NULL without pulling in <stddef.h> dependencies beyond what headers give. */
#ifndef NULL
#define NULL ((void *)0)
#endif

/* Reset a response object to a clean, all-zero OK state. */
static void rsp_clear(ST_PKTV1_DISPATCH_RSP *rsp)
{
    uint16_t i;
    rsp->command_id           = 0U;
    rsp->payload_length_words = 0U;
    for (i = 0U; i < (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS; ++i)
    {
        rsp->payload[i] = 0U;
    }
    rsp->status = PKTV1_DISPATCH_OK;
}

/* Build a PKTV1_RSP_ERROR response { original_cmd, err_code } and return status. */
static PKTV1_DISPATCH_RESULT_e make_error(ST_PKTV1_DISPATCH_RSP  *rsp,
                                          uint16_t                original_cmd,
                                          uint16_t                err_code,
                                          PKTV1_DISPATCH_RESULT_e status)
{
    rsp->command_id           = PKTV1_RSP_ERROR;
    rsp->payload[0]           = original_cmd;
    rsp->payload[1]           = err_code;
    rsp->payload_length_words = 2U;
    rsp->status               = status;
    return status;
}

/*
 * Forbidden classification (A3 / A3-0): the legacy register and block-data
 * ranges, the header magic, and anything below the 0x8000 Packet V1 namespace.
 */
static int is_forbidden_cmd(uint16_t cmd)
{
    if (cmd == PKTV1_HEADER_MAGIC)                                  /* 0xA55A */
    {
        return 1;
    }
    if ((cmd >= PKTV1_LEGACY_REG_LO) && (cmd <= PKTV1_LEGACY_REG_HI)) /* 0x0400..0x0FFF */
    {
        return 1;
    }
    if ((cmd >= PKTV1_LEGACY_BLK_LO) && (cmd <= PKTV1_LEGACY_BLK_HI)) /* 0x3000..0x3FFF */
    {
        return 1;
    }
    if (cmd < PKTV1_CMD_NAMESPACE_BASE)                            /* below 0x8000 */
    {
        return 1;
    }
    return 0;
}

PKTV1_DISPATCH_RESULT_e SpiPacketV1_Dispatch(const ST_SPI_PACKET_V1 *req,
                                             ST_PKTV1_DISPATCH_RSP  *rsp)
{
    uint16_t cmd;
    uint16_t n;
    uint16_t i;

    if (rsp == NULL)
    {
        return PKTV1_DISPATCH_ERR_NULL;
    }
    rsp_clear(rsp);

    if (req == NULL)
    {
        rsp->status = PKTV1_DISPATCH_ERR_NULL;
        return PKTV1_DISPATCH_ERR_NULL;
    }

    n = req->payloadWords;

    /* A valid parsed packet with payload words must carry a payload pointer. */
    if ((n > 0U) && (req->payload == NULL))
    {
        rsp->status = PKTV1_DISPATCH_ERR_NULL;
        return PKTV1_DISPATCH_ERR_NULL;
    }

    cmd = req->cmdId;

    if (is_forbidden_cmd(cmd))
    {
        return make_error(rsp, cmd, PKTV1_ERRCODE_FORBIDDEN,
                          PKTV1_DISPATCH_ERR_FORBIDDEN_CMD);
    }

    switch (cmd)
    {
    case PKTV1_CMD_PING:
        if (n != 0U)
        {
            return make_error(rsp, cmd, PKTV1_ERRCODE_BAD_LENGTH,
                              PKTV1_DISPATCH_ERR_BAD_LENGTH);
        }
        rsp->command_id           = PKTV1_CMD_PING;
        rsp->payload[0]           = PKTV1_PONG_WORD;
        rsp->payload_length_words = 1U;
        rsp->status               = PKTV1_DISPATCH_OK;
        return PKTV1_DISPATCH_OK;

    case PKTV1_CMD_GET_VERSION:
        if (n != 0U)
        {
            return make_error(rsp, cmd, PKTV1_ERRCODE_BAD_LENGTH,
                              PKTV1_DISPATCH_ERR_BAD_LENGTH);
        }
        rsp->command_id           = PKTV1_CMD_GET_VERSION;
        rsp->payload[0]           = PKTV1_VERSION_MAJOR;
        rsp->payload[1]           = PKTV1_VERSION_MINOR;
        rsp->payload[2]           = PKTV1_VERSION_PATCH;
        rsp->payload[3]           = PKTV1_SPEC_REV;
        rsp->payload_length_words = 4U;
        rsp->status               = PKTV1_DISPATCH_OK;
        return PKTV1_DISPATCH_OK;

    case PKTV1_CMD_GET_CAPS:
        if (n != 0U)
        {
            return make_error(rsp, cmd, PKTV1_ERRCODE_BAD_LENGTH,
                              PKTV1_DISPATCH_ERR_BAD_LENGTH);
        }
        rsp->command_id           = PKTV1_CMD_GET_CAPS;
        rsp->payload[0]           = (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS;
        rsp->payload[1]           = PKTV1_CAP_FEATURES;
        rsp->payload[2]           = PKTV1_CAP_CRC_ALGO_CCITT_FALSE;
        rsp->payload_length_words = 3U;
        rsp->status               = PKTV1_DISPATCH_OK;
        return PKTV1_DISPATCH_OK;

    case PKTV1_CMD_ECHO:
        if (n > (uint16_t)PKTV1_DISPATCH_MAX_PAYLOAD_WORDS)
        {
            return make_error(rsp, cmd, PKTV1_ERRCODE_RSP_OVERFLOW,
                              PKTV1_DISPATCH_ERR_RSP_OVERFLOW);
        }
        rsp->command_id = PKTV1_CMD_ECHO;
        for (i = 0U; i < n; ++i)
        {
            rsp->payload[i] = req->payload[i];   /* copy, never alias */
        }
        rsp->payload_length_words = n;
        rsp->status               = PKTV1_DISPATCH_OK;
        return PKTV1_DISPATCH_OK;

    case PKTV1_RSP_ERROR:   /* response-only opcode; not valid as a request */
    default:
        return make_error(rsp, cmd, PKTV1_ERRCODE_UNSUPPORTED,
                          PKTV1_DISPATCH_ERR_UNSUPPORTED_CMD);
    }
}
