#include "API_SPI.h"
#include "spi.h"

/**
 * @brief Transmits and receives a single word via SPI.
 *
 * This function sends a word to the SPI slave device and waits for a word
 * to be received in return. It operates in a blocking (polling) manner.
 *
 * @param base The base address of the SPI peripheral.
 * @param data The data word (e.g., 8-bit or 16-bit) to be transmitted.
 * @return The data word received from the slave device.
 */
uint16_t SPI_transmitReceiveWord(uint32_t base, uint16_t data)
{
    //
    // Wait until the transmit buffer is not full, then write the data.
    // This function blocks until the character can be sent.
    //
    SPI_writeDataBlockingNonFIFO(base, data);

    //
    // Wait until data has been received, then read it from the buffer.
    // This function blocks until a character is received.
    //
    return SPI_readDataBlockingNonFIFO(base);
}
