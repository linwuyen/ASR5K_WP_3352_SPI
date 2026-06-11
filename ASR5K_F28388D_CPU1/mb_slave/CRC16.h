
#ifndef CRC16_H_
#define CRC16_H_

#include <stdint.h>

typedef struct {
	uint16_t	wCRCWord;
	unsigned int  	usCRCindex;
} ST_CRC;


typedef ST_CRC * HAL_CRC;

#define DEFAULT_CRC (ST_CRC){0xFFFF, 0}

extern uint16_t ucMBCRC16(uint16_t data, HAL_CRC ref);



#endif /* CRC16_H_ */
