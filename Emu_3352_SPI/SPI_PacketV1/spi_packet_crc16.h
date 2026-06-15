/*
 * spi_packet_crc16.h
 *
 * SPI Packet V1 - CRC16/CCITT-FALSE reference (pure C).
 *
 * Algorithm parameters:
 *   Width      = 16
 *   Polynomial = 0x1021
 *   Initial    = 0xFFFF
 *   RefIn      = false
 *   RefOut     = false
 *   XorOut     = 0x0000
 *   Check      = 0x29B1   (ASCII "123456789")
 *
 * Constraints (see docs/SPI_PACKET_V1_SPEC.md):
 *   - pure C, depends only on <stdint.h>
 *   - no driverlib / device.h / spi_slave.h / wave_download.h
 *   - no legacy runtime calls, no global/watch state
 */

#ifndef SPI_PACKET_CRC16_H_
#define SPI_PACKET_CRC16_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_PACKET_CRC16_POLY   0x1021U
#define SPI_PACKET_CRC16_INIT   0xFFFFU
#define SPI_PACKET_CRC16_XOROUT 0x0000U
#define SPI_PACKET_CRC16_CHECK  0x29B1U   /* CRC of ASCII "123456789" */

/*
 * Update the running CRC with one 8-bit value.
 * Only the low 8 bits of byteVal are used; the parameter is uint16_t so the
 * routine is safe on targets where the smallest addressable unit is 16 bits.
 */
uint16_t SpiPacketCrc16_UpdateByte(uint16_t crc, uint16_t byteVal);

/*
 * Update the running CRC with one 16-bit word, fed big-endian
 * (high byte first, then low byte) per the V1 spec.
 */
uint16_t SpiPacketCrc16_UpdateWord(uint16_t crc, uint16_t word);

/*
 * Compute CRC over an array of 16-bit words, starting from INIT (0xFFFF).
 * wordCount == 0 returns INIT. Passing NULL with wordCount > 0 returns INIT
 * (defensive; callers should not do this).
 */
uint16_t SpiPacketCrc16_ComputeWords(const uint16_t *words, uint16_t wordCount);

/*
 * Compute CRC over a byte stream, starting from INIT (0xFFFF).
 * Each element supplies one 8-bit value (only the low 8 bits are used). The
 * element type is uint16_t, not uint8_t, because the C28x target has no 8-bit
 * type (CHAR_BIT == 16, so <stdint.h> does not define uint8_t). Provided so the
 * canonical "123456789" -> 0x29B1 vector can be verified.
 */
uint16_t SpiPacketCrc16_ComputeBytes(const uint16_t *bytes, uint16_t byteCount);

#ifdef __cplusplus
}
#endif

#endif /* SPI_PACKET_CRC16_H_ */
