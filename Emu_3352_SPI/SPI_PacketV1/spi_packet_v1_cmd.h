/*
 * spi_packet_v1_cmd.h
 *
 * SPI Packet V1 - command catalog constants (A3 / A4, pure C).
 *
 * This header declares ONLY constants: the Packet V1 command_id catalog, the
 * forbidden legacy ranges (from the A3-0 cmd_id.h audit), the deterministic
 * response field values, and the wire error codes. It contains no code, no
 * state, and no dependency beyond <stdint.h>.
 *
 * Hard rules (see docs/SPI_PACKET_V1_COMMAND_CATALOG_A3.md,
 * docs/SPI_PACKET_V1_A4_DISPATCHER_MOCK.md):
 *   - pure C, no driverlib / device.h / board.h
 *   - does NOT include or depend on the legacy cmd_id.h register map
 *   - command_id namespace is 0x8000+, disjoint from every legacy *_spi_addr
 */

#ifndef SPI_PACKET_V1_CMD_H_
#define SPI_PACKET_V1_CMD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Packet V1 command catalog (A3). command_id namespace base = 0x8000. --- */
#define PKTV1_CMD_NAMESPACE_BASE   0x8000U

#define PKTV1_CMD_PING             0x8000U
#define PKTV1_CMD_GET_VERSION      0x8001U
#define PKTV1_CMD_GET_CAPS         0x8002U
#define PKTV1_CMD_ECHO             0x8003U
#define PKTV1_RSP_ERROR            0x80FFU   /* response-only; not a request   */

/*
 * Forbidden legacy ranges (from the A3-0 legacy command map audit). A Packet V1
 * command_id must never fall in these; they belong to the legacy register
 * protocol in cmd_id.h, which this layer does not include or touch.
 */
#define PKTV1_LEGACY_REG_LO        0x0400U   /* RO/cal/RW/setting/control/wave  */
#define PKTV1_LEGACY_REG_HI        0x0FFFU
#define PKTV1_LEGACY_BLK_LO        0x3000U   /* block-data streaming window     */
#define PKTV1_LEGACY_BLK_HI        0x3FFFU
#define PKTV1_HEADER_MAGIC         0xA55AU   /* == SPI_PACKET_V1_HEADER_MAGIC   */

/* --- Deterministic response field values --- */

/* PING reply payload word ("PO"). */
#define PKTV1_PONG_WORD            0x504FU

/* GET_VERSION reply payload (major, minor, patch, spec rev). */
#define PKTV1_VERSION_MAJOR        0x0001U
#define PKTV1_VERSION_MINOR        0x0000U
#define PKTV1_VERSION_PATCH        0x0000U
#define PKTV1_SPEC_REV             0x0003U   /* A3 catalog revision             */

/* GET_CAPS feature flags. Initial build advertises meta commands ONLY -
 * no wave/runtime/DMA integration is claimed. */
#define PKTV1_CAP_FEAT_PING        0x0001U
#define PKTV1_CAP_FEAT_GET_VERSION 0x0002U
#define PKTV1_CAP_FEAT_GET_CAPS    0x0004U
#define PKTV1_CAP_FEAT_ECHO        0x0008U
#define PKTV1_CAP_FEAT_ERROR_RSP   0x0010U
#define PKTV1_CAP_FEATURES                                          \
    ((uint16_t)(PKTV1_CAP_FEAT_PING | PKTV1_CAP_FEAT_GET_VERSION |  \
                PKTV1_CAP_FEAT_GET_CAPS | PKTV1_CAP_FEAT_ECHO |     \
                PKTV1_CAP_FEAT_ERROR_RSP))

/* CRC algorithm id advertised by GET_CAPS (1 = CRC16/CCITT-FALSE). */
#define PKTV1_CAP_CRC_ALGO_CCITT_FALSE  0x0001U

/* --- Wire error codes carried in a PKTV1_RSP_ERROR payload (word1). --- */
#define PKTV1_ERRCODE_NONE         0x0000U
#define PKTV1_ERRCODE_FORBIDDEN    0x0001U
#define PKTV1_ERRCODE_UNSUPPORTED  0x0002U
#define PKTV1_ERRCODE_BAD_LENGTH   0x0003U
#define PKTV1_ERRCODE_RSP_OVERFLOW 0x0004U

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_V1_CMD_H_ */
