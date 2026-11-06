/* =========================================================================
 * Black Pill F411 - PLANE  (v4, simple)
 *
 * nRF24 receives joystick X / Y -> servo 1 / servo 2
 * Every 200 ms: MPU6050 + BMP280 + GPS -> telemetry on the CC1101
 *
 * Pins
 *   SPI1 shared: PA5 SCK, PA6 MISO, PA7 MOSI (AF5)
 *   nRF24  CSN PA4, CE PB0      CC1101  CSN PB12
 *   I2C1   PB8 SCL, PB9 SDA: MPU 0x68, BMP280 0x76
 *   GPS    PA3 USART2 RX, 9600   Servos PB4 / PB5 TIM3   LED PC13
 *
 * Command   (nRF24) 4 bytes: X, Y (low byte first)
 * Telemetry (CC1101) 38 bytes:
 *   [0..15] ax ay az gx gy gz temp(x100) alt(dm), low byte first
 *   [16] GPS fix ('0' = none)  [17..26] latitude text  [27..37] longitude text
 * ========================================================================= */

#include <stdint.h>
#include "gpio.h"
#include "spi.h"
#include "i2c.h"
#include "uart.h"
#include "timers.h"
#include "bmp280.h"

#define TELEM_LEN  38

uint8_t telem[TELEM_LEN];
int16_t ax, ay, az, gx, gy, gz;
int32_t bmp_temp, bmp_press, bmp_alt;
uint8_t nrf_ch, cc_version;      // Live Expressions: 80 and 0x14 (clone 0x04)


/* ---- time: TIM5 counts microseconds ---- */
void timer_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
	TIM5->PSC = 15;              // 16 MHz / 16 = 1 MHz
	TIM5->ARR = 0xFFFFFFFF;
	TIM5->EGR = TIM_EGR_UG;
	TIM5->CR1 = TIM_CR1_CEN;
}

void delay_us(uint32_t us)
{
	uint32_t start = TIM5->CNT;
	while (TIM5->CNT - start < us);
}


/* ---- nRF24 (CSN PA4, CE PB0) ---- */
uint8_t nrf_cmd(uint8_t c)
{
	gpio_low(GPIOA, 4);
	uint8_t status = spi_transfer(SPI1, c);
	gpio_high(GPIOA, 4);
	return status;
}

void nrf_write(uint8_t reg, uint8_t value)
{
	gpio_low(GPIOA, 4);
	spi_transfer(SPI1, 0x20 | reg);
	spi_transfer(SPI1, value);
	gpio_high(GPIOA, 4);
}

void nrf_init(void)
{
	gpio_init(GPIOA, 5, gpio_af, gpio_push_pull, 5);      // SCK
	gpio_init(GPIOA, 6, gpio_af, gpio_push_pull, 5);      // MISO
	gpio_init(GPIOA, 7, gpio_af, gpio_push_pull, 5);      // MOSI
	gpio_high(GPIOA, 4);
	gpio_init(GPIOA, 4, gpio_output, gpio_push_pull, 0);  // CSN
	gpio_init(GPIOB, 0, gpio_output, gpio_push_pull, 0);  // CE
	spi_init(SPI1, spi_div4);

	delay_us(100000);
	nrf_write(0x00, 0x0F);       // CONFIG: CRC, power up, RX
	delay_us(2000);
	nrf_write(0x05, 80);         // RF_CH: 2480 MHz
	nrf_write(0x06, 0x00);       // RF_SETUP: 1 Mbps, low power
	nrf_write(0x11, 4);          // RX_PW_P0: 4 byte packets
	nrf_write(0x07, 0x70);       // clear flags
	nrf_cmd(0xE2);               // FLUSH_RX
	gpio_high(GPIOB, 0);         // CE high: listen

	gpio_low(GPIOA, 4);          // check: RF_CH must read 80
	spi_transfer(SPI1, 0x05);
	nrf_ch = spi_transfer(SPI1, 0xFF);
	gpio_high(GPIOA, 4);
}

uint8_t nrf_receive(uint8_t *data)        // 1 = 4 new bytes in data[]
{
	if ((nrf_cmd(0xFF) & 0x0E) == 0x0E)   // RX FIFO empty
		return 0;
	gpio_low(GPIOA, 4);
	spi_transfer(SPI1, 0x61);             // R_RX_PAYLOAD
	for (int i = 0; i < 4; i++)
		data[i] = spi_transfer(SPI1, 0xFF);
	gpio_high(GPIOA, 4);
	nrf_write(0x07, 0x40);                // clear RX_DR
	return 1;
}


/* ---- CC1101 (CSN PB12): after CSN low, wait for MISO (PA6) low ---- */
void cc_select(void)
{
	gpio_low(GPIOB, 12);
	for (int i = 0; i < 10000 && (GPIOA->IDR & (1U << 6)); i++);
}

void cc_command(uint8_t c)                // strobe
{
	cc_select();
	spi_transfer(SPI1, c);
	gpio_high(GPIOB, 12);
}

void cc_write(uint8_t reg, uint8_t value)
{
	cc_select();
	spi_transfer(SPI1, reg);
	spi_transfer(SPI1, value);
	gpio_high(GPIOB, 12);
}

uint8_t cc_status(uint8_t reg)            // status register 0x30..0x3D
{
	cc_select();
	spi_transfer(SPI1, 0xC0 | reg);
	uint8_t value = spi_transfer(SPI1, 0x00);
	gpio_high(GPIOB, 12);
	return value;
}

// 434.4 MHz, GFSK, 38.4 kbps, fixed length + CRC (same table on the Nucleo)
const uint8_t cc_config[][2] = {
	{0x03, 0x47}, {0x06, TELEM_LEN}, {0x08, 0x04}, {0x0B, 0x06},
	{0x0D, 0x10}, {0x0E, 0xB5}, {0x0F, 0x2B},          // 434.4 MHz
	{0x10, 0xCA}, {0x11, 0x83}, {0x12, 0x13}, {0x15, 0x35},
	{0x18, 0x18}, {0x19, 0x16}, {0x1B, 0x43}, {0x20, 0xFB},
	{0x23, 0xE9}, {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F},
	{0x2C, 0x81}, {0x2D, 0x35}, {0x2E, 0x09},
	{0x3E, 0x60},                                      // PATABLE: 0 dBm
};

void cc_init(void)                        // after nrf_init (SPI1 ready)
{
	cc_command(0x30);                     // SRES: reset
	delay_us(1000);
	for (int i = 0; i < (int)(sizeof(cc_config) / 2); i++)
		cc_write(cc_config[i][0], cc_config[i][1]);
	cc_version = cc_status(0x31);
}

void cc_send(void)
{
	if ((cc_status(0x35) & 0x1F) != 0x01) // not IDLE: last packet still going
		return;
	cc_command(0x3B);                     // SFTX
	cc_select();
	spi_transfer(SPI1, 0x7F);             // TX FIFO, burst
	for (int i = 0; i < TELEM_LEN; i++)
		spi_transfer(SPI1, telem[i]);
	gpio_high(GPIOB, 12);
	cc_command(0x35);                     // STX
}


/* ---- servos, MPU6050 ---- */
void servo_init(void)
{
	gpio_init(GPIOB, 4, gpio_af, gpio_push_pull, 2);      // TIM3 CH1
	gpio_init(GPIOB, 5, gpio_af, gpio_push_pull, 2);      // TIM3 CH2
	pwm_init(TIM3, 1, 15, 19999);
	pwm_init(TIM3, 2, 15, 19999);
	pwm_set(TIM3, 1, 1500);
	pwm_set(TIM3, 2, 1500);
}

void mpu_init(void)
{
	gpio_init(GPIOB, 8, gpio_af, gpio_open_drain, 4);     // SCL
	gpio_init(GPIOB, 9, gpio_af, gpio_open_drain, 4);     // SDA
	I2C_init(I2C1);
	I2C_write(I2C1, 0x6B, 0x68, 0x00);                    // wake up
}

void mpu_read(void)
{
	uint8_t b[14];
	I2C_read(I2C1, 0x3B, 0x68, b, 14);
	ax = (int16_t)(b[0]  << 8 | b[1]);
	ay = (int16_t)(b[2]  << 8 | b[3]);
	az = (int16_t)(b[4]  << 8 | b[5]);
	gx = (int16_t)(b[8]  << 8 | b[9]);
	gy = (int16_t)(b[10] << 8 | b[11]);
	gz = (int16_t)(b[12] << 8 | b[13]);
}


/* ---- GPS: interrupt collects one GGA line ---- */
volatile uint8_t gps_ready = 0;
char gps_line[100];
uint8_t gps_pos = 0;
char lat_s[10] = "000000000";
char lon_s[11] = "0000000000";
char gps_fix = '0';

void gps_init(void)
{
	gpio_init(GPIOA, 3, gpio_af, gpio_push_pull, 7);
	uart_init(USART2, 16000000 / 9600);
	USART2->CR1 |= USART_CR1_RXNEIE;
	NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)
{
	(void)USART2->SR;                     // SR then DR: also clears overrun
	char c = USART2->DR;
	if (gps_ready) return;
	if (c == '$') gps_pos = 0;
	if (gps_pos < 99) gps_line[gps_pos++] = c;
	if (c == '\n' && gps_line[3] == 'G' && gps_line[4] == 'G' && gps_line[5] == 'A')
		gps_ready = 1;
}

int gps_field(int n)                      // index after the n-th comma
{
	int i = 0;
	while (n > 0 && i < 99)
		if (gps_line[i++] == ',') n--;
	return i;
}

void gps_update(void)                     // $GPGGA,time,lat,N,lon,E,fix,...
{
	if (!gps_ready) return;
	int lat = gps_field(2), lon = gps_field(4), fix = gps_field(6);
	gps_fix = (gps_line[fix] == ',') ? '0' : gps_line[fix];
	if (gps_line[lat] != ',') {
		for (int k = 0; k < 10; k++) lat_s[k] = gps_line[lat + k];
		for (int k = 0; k < 11; k++) lon_s[k] = gps_line[lon + k];
	}
	gps_ready = 0;
}


/* ---- telemetry ---- */
void telemetry_send(void)
{
	int16_t v[8] = { ax, ay, az, gx, gy, gz, (int16_t)bmp_temp, (int16_t)bmp_alt };
	for (int i = 0; i < 8; i++) {
		telem[i * 2]     = v[i] & 0xFF;
		telem[i * 2 + 1] = v[i] >> 8;
	}
	telem[16] = gps_fix;
	for (int k = 0; k < 10; k++) telem[17 + k] = lat_s[k];
	for (int k = 0; k < 11; k++) telem[27 + k] = lon_s[k];
	cc_send();
}


int main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	gpio_high(GPIOB, 12);                                 // CC1101 CSN high FIRST
	gpio_init(GPIOB, 12, gpio_output, gpio_push_pull, 0);
	gpio_init(GPIOC, 13, gpio_output, gpio_push_pull, 0); // LED

	timer_init();
	nrf_init();
	cc_init();
	servo_init();
	mpu_init();
	bmp280_init();
	bmp280_zero();
	gps_init();

	uint32_t last_telem = TIM5->CNT;

	while (1) {
		uint8_t pkt[4];
		if (nrf_receive(pkt)) {
			uint16_t x = pkt[0] | pkt[1] << 8;
			uint16_t y = pkt[2] | pkt[3] << 8;
			if (x <= 4095 && y <= 4095) {             // ignore corrupt values
				pwm_set(TIM3, 1, 1000 + (uint32_t)x * 1000 / 4095);
				pwm_set(TIM3, 2, 1000 + (uint32_t)y * 1000 / 4095);
				GPIOC->ODR ^= (1 << 13);
			}
		}

		gps_update();

		if (TIM5->CNT - last_telem >= 200000) {       // every 200 ms
			last_telem += 200000;
			mpu_read();
			bmp280_read(&bmp_temp, &bmp_press);
			bmp_alt = bmp280_altitude_dm(bmp_press);
			telemetry_send();
		}
	}
}
