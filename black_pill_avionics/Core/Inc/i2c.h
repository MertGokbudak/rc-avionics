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


#endif /* INC_I2C_H_ */

/* Usage notes generated with AI assistance (Claude), reviewed by the author. */

/* =====================================================================
 * USAGE NOTES
 *
 * I2C_read (i2c, reg, adress, data_array, len)  → 1 = OK, 0 = error
 * I2C_write(i2c, reg, adress, value)            → 1 = OK, 0 = error
 *
 * ---------------- MPU6050  (address 0x68, AD0=GND) -------------------
 *
 *  Identity:    uint8_t id;
 *               I2C_read(I2C1, 0x75, 0x68, &id, 1);        // id == 0x68
 *
 *  Wake up:     I2C_write(I2C1, 0x6B, 0x68, 0x00);          // PWR_MGMT_1
 *
 *  Read data:   uint8_t mpu_raw[14];
 *               I2C_read(I2C1, 0x3B, 0x68, mpu_raw, 14);
 *
 *  Decoding (high byte first):
 *    ax = (int16_t)((mpu_raw[0]  << 8) | mpu_raw[1]);
 *    ay = (int16_t)((mpu_raw[2]  << 8) | mpu_raw[3]);
 *    az = (int16_t)((mpu_raw[4]  << 8) | mpu_raw[5]);
 *    t  = (int16_t)((mpu_raw[6]  << 8) | mpu_raw[7]);
 *    gx = (int16_t)((mpu_raw[8]  << 8) | mpu_raw[9]);
 *    gy = (int16_t)((mpu_raw[10] << 8) | mpu_raw[11]);
 *    gz = (int16_t)((mpu_raw[12] << 8) | mpu_raw[13]);
 *
 *  Unit conversion (default ranges):
 *    accel ±2 g     → 16384 = 1 g
 *    gyro  ±250 °/s → 131   = 1 °/s
 *    temperature °C = t / 340.0 + 36.53
 *
 * ---------------- BMP280  (address 0x76, SDO=GND; else 0x77) ---------
 *
 *  Identity:    uint8_t id;
 *               I2C_read(I2C1, 0xD0, 0x76, &id, 1);        // id == 0x58
 *
 *  Calibration: uint8_t bmp_calib[24];                      // once at startup
 *               I2C_read(I2C1, 0x88, 0x76, bmp_calib, 24);
 *
 *  Start:       I2C_write(I2C1, 0xF4, 0x76, 0x27);          // normal mode, x1
 *
 *  Read data:   uint8_t bmp_raw[6];
 *               I2C_read(I2C1, 0xF7, 0x76, bmp_raw, 6);
 *
 *  Decoding (20-bit, MSB first):
 *    adc_P = (bmp_raw[0] << 12) | (bmp_raw[1] << 4) | (bmp_raw[2] >> 4);
 *    adc_T = (bmp_raw[3] << 12) | (bmp_raw[4] << 4) | (bmp_raw[5] >> 4);
 *
 *  NOTE: adc_P/adc_T are raw values. Real pressure and temperature
 *        need the calibration coefficients and the compensation
 *        formula from the datasheet (to be written in bmp280.c).
 * ===================================================================== */

