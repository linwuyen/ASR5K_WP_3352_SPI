#ifndef API_SPI_H_
#define API_SPI_H_

#include "device.h"

//
// Function Prototypes
//

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
uint16_t SPI_transmitReceiveWord(uint32_t base, uint16_t data);


#endif /* API_SPI_H_ */
