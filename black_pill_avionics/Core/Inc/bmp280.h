#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#define BMP280_I2C   I2C1   // I2C bus (PB8 SCL / PB9 SDA)
#define BMP280_ADDR  0x76   // 7-bit address, SDO -> GND (your driver shifts it)

uint8_t bmp280_init(void);                                 // returns chip ID (0x58 = BMP280, 0x60 = BME280)
void    bmp280_read(int32_t *temp_c100, int32_t *press_pa); // temp in 0.01 C, pressure in Pa
void    bmp280_zero(void);                                 // store current pressure as ground level (P0)
int32_t bmp280_altitude_dm(int32_t press_pa);              // height above P0 in 0.1 m

#endif
