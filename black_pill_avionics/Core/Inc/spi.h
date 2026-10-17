#ifndef INC_SPI_H_
#define INC_SPI_H_

#include "stm32f4xx.h"

typedef enum{
	spi_div2,     // 000
	spi_div4,     // 001
	spi_div8,     // 010
	spi_div16,    // 011
	spi_div32,    // 100
	spi_div64,    // 101
	spi_div128,   // 110
	spi_div256,   // 111
} spi_brr_t;


void spi_init(SPI_TypeDef *spi,
		spi_brr_t brr);

uint8_t spi_transfer(SPI_TypeDef *spi,
		uint8_t spi_data);


#endif /* INC_SPI_H_ */
