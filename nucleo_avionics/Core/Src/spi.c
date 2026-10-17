#include "spi.h"

void spi_init(SPI_TypeDef *spi,
		spi_brr_t brr){

	if (spi == SPI1) RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
	if (spi == SPI4) RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
	if (spi == SPI2) RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
	if (spi == SPI3) RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;

	spi->CR1 |= (brr << 3);
	spi->CR1 |= SPI_CR1_MSTR;
	spi->CR1 |= SPI_CR1_SSM;//Software slave management
	spi->CR1 |= SPI_CR1_SSI;//Internal slave select
	spi->CR1 |= SPI_CR1_SPE;
}


uint8_t spi_transfer(SPI_TypeDef *spi,
		uint8_t spi_data){

	while (!(spi->SR & SPI_SR_TXE));
	spi->DR = spi_data;
	while (!(spi->SR & SPI_SR_RXNE));
	while (spi->SR & SPI_SR_BSY);

	return spi->DR;
}


