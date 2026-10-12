#include "i2c.h"

void I2C_init(I2C_TypeDef *i2c)
{
	if (i2c == I2C1) RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
	if (i2c == I2C2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
	if (i2c == I2C3) RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;

	i2c->CR2 = 16;      //Bits 5:0 FREQ[5:0]: Peripheral clock frequency
	i2c->CCR = 80;      //Bits 11:0 CCR[11:0]: Clock control register in Fm/Sm mode (controller mode)
	i2c->TRISE = 17;    //Bits 5:0 TRISE[5:0]: Maximum rise time in Fm/Sm mode (controller mode)

	i2c->CR1 |= (I2C_CR1_PE);
}

uint8_t I2C_read(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
        uint8_t *data_array,
		uint8_t len){

	if (len == 0) return 0;

	i2c->CR1 |= (I2C_CR1_START);
	while (!( i2c->SR1 & (I2C_SR1_SB)));
	i2c->DR = (adress << 1) | 0 ;
	while (!(i2c->SR1 & (I2C_SR1_ADDR))){

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}
	/*Note: In target mode, it is recommended to perform the complete clearing sequence (READ
	SR1 then READ SR2) after ADDR is set.(IMPORTENT TO AVOID clock stretching*/
	(void)i2c->SR1;
	(void)i2c->SR2;

	i2c->DR = reg;
	while (!(i2c->SR1 & (I2C_SR1_BTF))){ //Bit 2 BTF: Byte transfer finished - includes ACK

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}
	i2c->CR1 |= (I2C_CR1_START);
	while (!( i2c->SR1 & (I2C_SR1_SB)));
	i2c->DR = (adress << 1) | 1 ;
	while (!(i2c->SR1 & (I2C_SR1_ADDR))){

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}


	if (len > 1) i2c->CR1 |=  (I2C_CR1_ACK);
	else         i2c->CR1 &= ~(I2C_CR1_ACK);
	(void)i2c->SR1;
	(void)i2c->SR2;

	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {
			i2c->CR1 &= ~(I2C_CR1_ACK);
			i2c->CR1 |=  (I2C_CR1_STOP);
		}
		while (!(i2c->SR1 & (I2C_SR1_RXNE)));
		data_array[i] = i2c->DR;
	}
	while (i2c->CR1 & (I2C_CR1_STOP));
	return 1;
}


uint8_t I2C_write(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
		uint8_t value) {

	i2c->CR1 |= (I2C_CR1_START);
	while (!( i2c->SR1 & (I2C_SR1_SB)));
	i2c->DR = (adress << 1) | 0 ;
	while (!(i2c->SR1 & (I2C_SR1_ADDR))){

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}
	(void)i2c->SR1;
	(void)i2c->SR2;
	i2c->DR = reg;
	while (!(i2c->SR1 & (I2C_SR1_TXE))){

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}
	i2c->DR = value;
	while (!(i2c->SR1 & (I2C_SR1_BTF))){

		if (i2c->SR1 & (I2C_SR1_AF)) {
			i2c->SR1 &= ~(I2C_SR1_AF);
			i2c->CR1 |= (I2C_CR1_STOP);
			return 0;
		}
	}
	i2c->CR1 |= (I2C_CR1_STOP);
	while (i2c->CR1 & (I2C_CR1_STOP));

	return 1;
}
