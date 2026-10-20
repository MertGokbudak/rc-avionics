#include "bmp280.h"
#include "i2c.h"

// Calibration values (read once from the sensor at 0x88)
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

static int32_t t_fine;   // shared between temperature and pressure formulas
static int32_t p0;       // ground pressure (Pa)

static void delay(volatile uint32_t n)
{
    while (n--);
}

uint8_t bmp280_init(void)
{
    uint8_t id = 0;   // stays 0 if the sensor does not answer
    uint8_t c[24];

    // 1. Chip ID
    I2C_read(BMP280_I2C, 0xD0, BMP280_ADDR, &id, 1);

    // 2. Calibration: 24 bytes, little-endian (low byte first)
    I2C_read(BMP280_I2C, 0x88, BMP280_ADDR, c, 24);
    dig_T1 = (uint16_t)(c[1]  << 8 | c[0]);
    dig_T2 = (int16_t) (c[3]  << 8 | c[2]);
    dig_T3 = (int16_t) (c[5]  << 8 | c[4]);
    dig_P1 = (uint16_t)(c[7]  << 8 | c[6]);
    dig_P2 = (int16_t) (c[9]  << 8 | c[8]);
    dig_P3 = (int16_t) (c[11] << 8 | c[10]);
    dig_P4 = (int16_t) (c[13] << 8 | c[12]);
    dig_P5 = (int16_t) (c[15] << 8 | c[14]);
    dig_P6 = (int16_t) (c[17] << 8 | c[16]);
    dig_P7 = (int16_t) (c[19] << 8 | c[18]);
    dig_P8 = (int16_t) (c[21] << 8 | c[20]);
    dig_P9 = (int16_t) (c[23] << 8 | c[22]);

    // 3. Settings
    I2C_write(BMP280_I2C, 0xF5, BMP280_ADDR, 0x10);   // IIR filter x16, standby 0.5 ms
    I2C_write(BMP280_I2C, 0xF4, BMP280_ADDR, 0x57);   // temp x2, press x16, normal mode (~26 Hz)

    delay(1000000);                       // let the first measurements finish
    return id;
}

void bmp280_read(int32_t *temp_c100, int32_t *press_pa)
{
    uint8_t d[6];
    int32_t adc_P, adc_T;
    int32_t v1, v2;
    int64_t a, b, p;

    // Raw data: press msb/lsb/xlsb, temp msb/lsb/xlsb (20 bits each)
    I2C_read(BMP280_I2C, 0xF7, BMP280_ADDR, d, 6);
    adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);

    // Temperature (Bosch datasheet formula)
    v1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * dig_T2) >> 11;
    v2 = (((((adc_T >> 4) - (int32_t)dig_T1) * ((adc_T >> 4) - (int32_t)dig_T1)) >> 12) * dig_T3) >> 14;
    t_fine = v1 + v2;
    *temp_c100 = (t_fine * 5 + 128) >> 8;

    // Pressure (Bosch datasheet 64-bit formula)
    a = (int64_t)t_fine - 128000;
    b = a * a * dig_P6;
    b = b + ((a * dig_P5) << 17);
    b = b + ((int64_t)dig_P4 << 35);
    a = ((a * a * dig_P3) >> 8) + ((a * dig_P2) << 12);
    a = ((((int64_t)1) << 47) + a) * dig_P1 >> 33;
    if (a == 0) {            // avoid division by zero
        *press_pa = 0;
        return;
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - b) * 3125) / a;
    a = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    b = ((int64_t)dig_P8 * p) >> 19;
    p = ((p + a + b) >> 8) + ((int64_t)dig_P7 << 4);
    *press_pa = (int32_t)(p >> 8);   // result was Pa * 256
}

void bmp280_zero(void)
{
    int32_t t, p, sum = 0;

    for (int i = 0; i < 16; i++) {   // average 16 readings
        bmp280_read(&t, &p);
        sum += p;
        delay(200000);
    }
    p0 = sum / 16;
}

int32_t bmp280_altitude_dm(int32_t press_pa)
{
    // Near ground: 1 Pa ~ 8.3 cm = 0.83 dm
    return (p0 - press_pa) * 83 / 100;
}
