/*
 * spi_packet_crc16.c
 *
 * SPI Packet V1 - CRC16/CCITT-FALSE reference implementation (pure C).
 * See spi_packet_crc16.h and docs/SPI_PACKET_V1_SPEC.md.
 *
 * Bit-serial core (no lookup table) to keep the footprint minimal and the
 * code trivially auditable. Polynomial 0x1021, init 0xFFFF, no reflection.
 */

#include "spi_packet_crc16.h"

uint16_t SpiPacketCrc16_UpdateByte(uint16_t crc, uint16_t byteVal)
{
    uint16_t i;

    crc ^= (uint16_t)((byteVal & 0x00FFU) << 8);
    for (i = 0U; i < 8U; ++i)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc = (uint16_t)((crc << 1) ^ SPI_PACKET_CRC16_POLY);
        }
        else
        {
            crc = (uint16_t)(crc << 1);
        }
    }
    return (uint16_t)(crc & 0xFFFFU);
}

uint16_t SpiPacketCrc16_UpdateWord(uint16_t crc, uint16_t word)
{
    /* Big-endian: most-significant byte first, then least-significant byte. */
    crc = SpiPacketCrc16_UpdateByte(crc, (uint16_t)((word >> 8) & 0x00FFU));
    crc = SpiPacketCrc16_UpdateByte(crc, (uint16_t)(word & 0x00FFU));
    return crc;
}

uint16_t SpiPacketCrc16_ComputeWords(const uint16_t *words, uint16_t wordCount)
{
    uint16_t crc = SPI_PACKET_CRC16_INIT;
    uint16_t i;

    if (words == 0)
    {
        return crc;
    }
    for (i = 0U; i < wordCount; ++i)
    {
        crc = SpiPacketCrc16_UpdateWord(crc, words[i]);
    }
    return crc;
}

uint16_t SpiPacketCrc16_ComputeBytes(const uint16_t *bytes, uint16_t byteCount)
{
    uint16_t crc = SPI_PACKET_CRC16_INIT;
    uint16_t i;

    if (bytes == 0)
    {
        return crc;
    }
    for (i = 0U; i < byteCount; ++i)
    {
        /* Only the low 8 bits of each element are consumed (see header). */
        crc = SpiPacketCrc16_UpdateByte(crc, bytes[i]);
    }
    return crc;
}
