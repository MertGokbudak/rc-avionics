#ifndef INC_I2C_H_
#define INC_I2C_H_

#include "stm32f4xx.h"

void I2C_init(I2C_TypeDef *i2c);


uint8_t I2C_read(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
        uint8_t *data_array,
		uint8_t len);

uint8_t I2C_write(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
		uint8_t value);

// Free a stuck bus (slave holding SDA low): 9 clocks + STOP + peripheral reset
void I2C_recover(I2C_TypeDef *i2c, GPIO_TypeDef *port, uint32_t scl, uint32_t sda);


#endif /* INC_I2C_H_ */
