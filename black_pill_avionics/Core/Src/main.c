#include <math.h>
#include "stm32f4xx.h"
#include "gpio.h"
#include "spi.h"
#include "i2c.h"
#include "uart.h"
#include "timers.h"
#include "bmp280.h"
#include "nrf24.h"
#include "cc1101.h"
#include "gps.h"

#define CMD_LEN    7
#define TELEM_LEN  20

// MPU6050 0x68- BMP280 0x76

uint8_t  cmd[CMD_LEN];
uint16_t joy_x, joy_y, pot;
uint8_t  button;
uint32_t last_cmd;

int16_t  ax, ay, az;
int16_t  roll, pitch;
int32_t  bmp_temp;
int32_t  bmp_press;
int32_t  bmp_alt;

uint8_t  telem[TELEM_LEN];

uint8_t bmp_id;     // BMP280 chip ID: 0x58 (88) = OK, 0 = sensor did not answer


// nRF24 + CC1101
void spi1_init(void)
{
	gpio_init(GPIOA, 5, gpio_af, gpio_push_pull, 5);// SCK
	gpio_init(GPIOA, 6, gpio_af, gpio_push_pull, 5);// MISO
	gpio_init(GPIOA, 7, gpio_af, gpio_push_pull, 5);// MOSI
	spi_init(SPI1, spi_div4); // 4 MHz
}

void servo_init(void)
{
	gpio_init(GPIOB, 4, gpio_af, gpio_push_pull, 2); // TIM3 CH1
	gpio_init(GPIOB, 5, gpio_af, gpio_push_pull, 2);// TIM3 CH2
	pwm_init(TIM3, 1, 15, 19999);
	pwm_init(TIM3, 2, 15, 19999);
	pwm_set(TIM3, 1, 1500);
	pwm_set(TIM3, 2, 1500);
}

uint8_t receive_command(void)
{
	if (nrf_receive(cmd, CMD_LEN) == 0)
		return 0; //if no command arrive skips below

	joy_x  = cmd[0] | (cmd[1] << 8);
	joy_y  = cmd[2] | (cmd[3] << 8);
	pot    = cmd[4] | (cmd[5] << 8);
	button = cmd[6];

	pwm_set(TIM3, 1, 1000 + joy_x * 1000 / 4095);
	pwm_set(TIM3, 2, 1000 + joy_y * 1000 / 4095);

	last_cmd = TIM5->CNT;

	GPIOC->ODR ^= (1U << 13);    // LED
	return 1;
}

void check_failsafe(void)
{
	if (TIM5->CNT - last_cmd > 500000) {
		pwm_set(TIM3, 1, 1500);
		pwm_set(TIM3, 2, 1500);
	}
}


void i2c1_init(void)
{
	gpio_init(GPIOB, 8, gpio_af, gpio_open_drain, 4);// SCL
	gpio_init(GPIOB, 9, gpio_af, gpio_open_drain, 4);// SDA
	I2C_init(I2C1);
}

void read_mpu(void)
{
	uint8_t b[6];
	if (I2C_read(I2C1, 0x3B, 0x68, b, 6) == 0) {
		I2C_recover(I2C1, GPIOB, 8, 9);   // bus stuck: free it (SCL PB8, SDA PB9)
		return;                           // keep the last angles
	}
	ax = (int16_t)(b[0] << 8 | b[1]);
	ay = (int16_t)(b[2] << 8 | b[3]);
	az = (int16_t)(b[4] << 8 | b[5]);

	// 572.96 = 1800 / pi  (rad -> 0.1 deg)
	roll  = atan2f(ay, az) * 572.96f;
	pitch = atan2f(-ax, sqrtf((float)ay * ay + (float)az * az)) * 572.96f;
}

void read_bmp(void)
{
	if (bmp280_read(&bmp_temp, &bmp_press))     // only when the read worked
		bmp_alt = bmp280_altitude_dm(bmp_press);
}


void send_telemetry(void)
{
	int16_t temp = bmp_temp / 10;

	telem[0]  = roll;           telem[1]  = roll >> 8;
	telem[2]  = pitch;          telem[3]  = pitch >> 8;
	telem[4]  = bmp_alt;        telem[5]  = bmp_alt >> 8;
	telem[6]  = temp;           telem[7]  = temp >> 8;
	telem[8]  = gps_lat;        telem[9]  = gps_lat >> 8;
	telem[10] = gps_lat >> 16;  telem[11] = gps_lat >> 24;
	telem[12] = gps_lon;        telem[13] = gps_lon >> 8;
	telem[14] = gps_lon >> 16;  telem[15] = gps_lon >> 24;
	telem[16] = gps_speed;      telem[17] = gps_speed >> 8;
	telem[18] = gps_course;     telem[19] = gps_course >> 8;   // course (direction)

	cc_send(telem, TELEM_LEN);
}


// Radio watchdog: if a radio chip lost its settings (e.g. short power dip
// reset the chip but not the MCU), set it up again.
void radio_check(void)
{
	if (nrf_read(0x00) != 0x0F) {      // nRF CONFIG: our RX setting (CRC 2 B, power up, RX)
		nrf_init_rx(CMD_LEN);
	}
	if (cc_read(0x06) != TELEM_LEN) {  // CC1101 PKTLEN: our packet length
		cc_init(TELEM_LEN);
	}
}


int main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

	nrf_csn_init(); // CS pins high before any SPI traffic
	cc_csn_init();
	spi1_init();
	timer5_delay_init();
	nrf_init_rx(CMD_LEN);
	cc_init(TELEM_LEN);
	servo_init();
	gpio_init(GPIOC, 13, gpio_output, gpio_push_pull, 0);   // LED

	i2c1_init();
	I2C_recover(I2C1, GPIOB, 8, 9);      // a sensor may still be stuck from before the reset
	I2C_write(I2C1, 0x6B, 0x68, 0x00);   // MPU6050 wake up
	bmp_id = bmp280_init();
	bmp280_zero();
	gps_init();

	uint32_t last_telem = TIM5->CNT;

	while (1) {
		while (receive_command());   // read every waiting command, the last one wins
		check_failsafe();
		gps_update();

		if (TIM5->CNT - last_telem >= 100000) {// 10 Hz
			last_telem += 100000;
			read_mpu();
			read_bmp();
			send_telemetry();
			radio_check();
		}
	}
}
