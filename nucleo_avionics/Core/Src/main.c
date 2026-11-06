/* =========================================================================
 * Nucleo F446 - REMOTE (ground station)  (v2)
 *
 * Every 20 ms: read joystick + button -> send command over nRF24
 *              -> wait for the ACK -> telemetry comes inside it
 *              -> draw one value on the screen
 * No interrupts: TIM5 only counts microseconds, the loop polls it.
 *
 * Pins
 *   SPI1 shared with the LCD: PB3 SCK, PB4 MISO, PB5 MOSI (AF5)
 *   nRF24  CSN PB12, CE PC7
 *   LCD    CS PB6, DC PA9, RESET PB8 (in lcd.c)
 *   Joystick X PA0 (ADC1 ch0), Y PA1 (ch1), button PC2 (0 = pressed)
 *
 * Command   5 bytes: [0..1] X  [2..3] Y  [4] button (1 = pressed)
 * Telemetry 18 bytes: [0..1] roll x10 [2..3] pitch x10
 *   [4..15] ax ay az gx gy gz  [16] mode (0 manual, 1 stab, 2 failsafe)
 *   [17] mpu_ok
 *
 * Watch in Live Expressions: nrf_ch (80), joy_x, joy_y, link_ok,
 *   ack_count (radio), telem_count (plane code)
 * ========================================================================= */

#include <stdint.h>
#include "gpio.h"
#include "spi.h"
#include "adc.h"
#include "lcd.h"

#define CMD_LEN    5
#define TELEM_LEN  18

uint8_t cmd[CMD_LEN];
uint8_t telem[TELEM_LEN];

uint16_t joy_x, joy_y;
uint8_t  button;

int16_t roll10, pitch10;
int16_t ax, ay, az, gx, gy, gz;
uint8_t plane_mode = 2;
uint8_t plane_mpu_ok = 0;

// link
uint8_t  link_ok = 0;            // 1 = telemetry came in the last 0.5 s
uint32_t last_ack = 0;
uint32_t ack_count = 0;          // radio ACKs (the nRF24 chip answers)
uint32_t last_telem = 0;
uint32_t telem_count = 0;        // telemetry packets (the plane's code answers)
uint8_t  nrf_ch = 0;             // RF_CH read back, must be 80

// DEBUG: watch these in Live Expressions
uint8_t  nrf_regs[8];            // settings read back after nrf_init
uint8_t  last_status;            // STATUS after the last send
uint32_t maxrt_count = 0;        // sent 4 times, no ACK (plane did not answer)
uint32_t timeout_count = 0;      // no TX_DS, no MAX_RT (never sent)


/* ------------------------------------------------------------------------
 * Time: TIM5 counts microseconds, no interrupt
 * ------------------------------------------------------------------------ */
void timer_init(void)
{
	RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
	TIM5->PSC = 15;
	TIM5->ARR = 0xFFFFFFFF;
	TIM5->EGR = TIM_EGR_UG;
	TIM5->CR1 = TIM_CR1_CEN;
}

void delay_us(uint32_t us)
{
	uint32_t start = TIM5->CNT;
	while (TIM5->CNT - start < us);
}


/* ------------------------------------------------------------------------
 * nRF24: same functions as the plane, CSN = PB12
 * ------------------------------------------------------------------------ */
uint8_t nrf_cmd(uint8_t c)
{
	gpio_low(GPIOB, 12);
	uint8_t status = spi_transfer(SPI1, c);
	gpio_high(GPIOB, 12);
	return status;
}

void nrf_write(uint8_t reg, uint8_t value)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, 0x20 | reg);
	spi_transfer(SPI1, value);
	gpio_high(GPIOB, 12);
}

uint8_t nrf_read(uint8_t reg)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, 0x00 | reg);
	uint8_t value = spi_transfer(SPI1, 0xFF);
	gpio_high(GPIOB, 12);
	return value;
}

void nrf_write_buf(uint8_t c, uint8_t *buf, uint8_t len)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, c);
	for (uint8_t i = 0; i < len; i++)
		spi_transfer(SPI1, buf[i]);
	gpio_high(GPIOB, 12);
}

void nrf_read_buf(uint8_t c, uint8_t *buf, uint8_t len)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, c);
	for (uint8_t i = 0; i < len; i++)
		buf[i] = spi_transfer(SPI1, 0xFF);
	gpio_high(GPIOB, 12);
}

void nrf_csn_init(void)          // call BEFORE lcd_init
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;                   // clock, so ODR works
	gpio_high(GPIOB, 12);                                  // CSN high first...
	gpio_init(GPIOB, 12, gpio_output, gpio_push_pull, 0);  // ...then output
}

void nrf_init(void)
{
	// PB3/PB4/PB5 and SPI1 are already set by lcd_init()
	GPIOB->OSPEEDR |= (1U << (3 * 2)) | (1U << (5 * 2));   // SCK, MOSI medium speed
	gpio_init(GPIOC, 7, gpio_output, gpio_push_pull, 0);   // CE (starts low)

	delay_us(100000);
	nrf_write(0x00, 0x0E);       // CONFIG: CRC 2 bytes, power up, TX
	delay_us(2000);
	nrf_write(0x01, 0x01);       // EN_AA: auto-ACK on pipe 0
	nrf_write(0x02, 0x01);       // EN_RXADDR: pipe 0 (receives the ACK)
	nrf_write(0x04, 0x13);       // SETUP_RETR: 500 us wait, 3 retries
	nrf_write(0x05, 80);         // RF_CH: same as plane
	nrf_write(0x06, 0x00);       // RF_SETUP: same as plane
	nrf_write(0x1C, 0x01);       // DYNPD
	nrf_write(0x1D, 0x06);       // FEATURE: EN_DPL + EN_ACK_PAY
	nrf_write(0x07, 0x70);
	nrf_cmd(0xE1);
	nrf_cmd(0xE2);

	nrf_ch = nrf_read(0x05);     // check: must be 80

	// DEBUG: read every setting back
	nrf_regs[0] = nrf_read(0x00);   // CONFIG      expect 0x0E (14)
	nrf_regs[1] = nrf_read(0x01);   // EN_AA       expect 0x01
	nrf_regs[2] = nrf_read(0x02);   // EN_RXADDR   expect 0x01
	nrf_regs[3] = nrf_read(0x04);   // SETUP_RETR  expect 0x13 (19)
	nrf_regs[4] = nrf_read(0x05);   // RF_CH       expect 80
	nrf_regs[5] = nrf_read(0x06);   // RF_SETUP    expect 0x00
	nrf_regs[6] = nrf_read(0x1C);   // DYNPD       expect 0x01
	nrf_regs[7] = nrf_read(0x1D);   // FEATURE     expect 0x06
}

uint8_t nrf_send(void)                    // returns 1 if telemetry came
{
	nrf_write(0x07, 0x70);                // clear old flags
	nrf_write_buf(0xA0, cmd, CMD_LEN);    // W_TX_PAYLOAD
	gpio_high(GPIOC, 7);                  // CE pulse > 10 us = send
	delay_us(15);
	gpio_low(GPIOC, 7);

	// wait for TX_DS (ACK came) or MAX_RT (no ACK), at most 5 ms
	uint8_t status;
	uint32_t start = TIM5->CNT;
	do {
		status = nrf_cmd(0xFF);
	} while (!(status & 0x30) && TIM5->CNT - start < 5000);

	// DEBUG
	last_status = status;
	if (status & 0x10)
		maxrt_count++;                    // MAX_RT: sent 4 times, no ACK
	else if (!(status & 0x20))
		timeout_count++;                  // no TX_DS, no MAX_RT: never sent

	uint8_t got = 0;

	if (status & 0x20) {                  // TX_DS: plane got the command
		last_ack = TIM5->CNT;
		ack_count++;

		if (status & 0x40) {              // RX_DR: telemetry inside the ACK
			uint8_t len;
			nrf_read_buf(0x60, &len, 1);  // R_RX_PL_WID
			if (len == TELEM_LEN) {
				nrf_read_buf(0x61, telem, TELEM_LEN);
				got = 1;
			}
		}
	}
	else {
		nrf_cmd(0xE1);                    // not sent: drop it
	}

	nrf_cmd(0xE2);                        // FLUSH_RX
	nrf_write(0x07, 0x70);
	return got;
}


/* ------------------------------------------------------------------------
 * Joystick + button
 * ------------------------------------------------------------------------ */
void joystick_init(void)
{
	gpio_init(GPIOA, 0, gpio_analog, gpio_push_pull, 0);
	gpio_init(GPIOA, 1, gpio_analog, gpio_push_pull, 0);
	adc_init(ADC1);
	ADC1->SMPR2 |= (4 << 0) | (4 << 3);   // ch0, ch1: 84 cycles

	gpio_init(GPIOC, 2, gpio_input, gpio_push_pull, 0);
	GPIOC->PUPDR |= (1U << (2 * 2));      // PC2 pull-up
}

uint16_t adc_average(uint8_t ch)          // 8 readings -> less noise
{
	uint32_t sum = 0;
	for (int i = 0; i < 8; i++)
		sum += adc_read(ADC1, ch);
	return sum / 8;
}


/* ------------------------------------------------------------------------
 * Telemetry
 * ------------------------------------------------------------------------ */
int16_t get16(uint8_t *p)                 // low byte first
{
	return (int16_t)(p[0] | p[1] << 8);
}

void telemetry_decode(void)
{
	roll10  = get16(&telem[0]);
	pitch10 = get16(&telem[2]);
	ax = get16(&telem[4]);
	ay = get16(&telem[6]);
	az = get16(&telem[8]);
	gx = get16(&telem[10]);
	gy = get16(&telem[12]);
	gz = get16(&telem[14]);
	plane_mode   = telem[16];
	plane_mpu_ok = telem[17];
}


/* ------------------------------------------------------------------------
 * Screen: labels once, then one value per loop
 * ------------------------------------------------------------------------ */
void screen_labels(void)
{
	lcd_fill(0, 0, 240, 320, BLACK);
	lcd_text(10,  10, "MODE",  CYAN,  BLACK, 2);
	lcd_text(10,  40, "ROLL",  WHITE, BLACK, 2);
	lcd_text(10,  65, "PITCH", WHITE, BLACK, 2);
	lcd_text(10, 100, "AX",    WHITE, BLACK, 2);
	lcd_text(10, 125, "AY",    WHITE, BLACK, 2);
	lcd_text(10, 150, "AZ",    WHITE, BLACK, 2);
	lcd_text(10, 185, "GX",    WHITE, BLACK, 2);
	lcd_text(10, 210, "GY",    WHITE, BLACK, 2);
	lcd_text(10, 235, "GZ",    WHITE, BLACK, 2);
	lcd_text(10, 270, "LINK",  CYAN,  BLACK, 2);
	lcd_text(10, 295, "MPU",   CYAN,  BLACK, 2);
}

void screen_step(uint8_t step)
{
	switch (step) {
	case 0:
		if (plane_mode == 0)      lcd_text(100, 10, "MANUAL", WHITE, BLACK, 2);
		else if (plane_mode == 1) lcd_text(100, 10, "STAB  ", WHITE, BLACK, 2);
		else                      lcd_text(100, 10, "FAIL  ", RED,   BLACK, 2);
		break;
	case 1:  lcd_number(100,  40, roll10 / 10,  4, WHITE, BLACK, 2); break;
	case 2:  lcd_number(100,  65, pitch10 / 10, 4, WHITE, BLACK, 2); break;
	case 3:  lcd_number(100, 100, ax, 6, WHITE, BLACK, 2); break;
	case 4:  lcd_number(100, 125, ay, 6, WHITE, BLACK, 2); break;
	case 5:  lcd_number(100, 150, az, 6, WHITE, BLACK, 2); break;
	case 6:  lcd_number(100, 185, gx, 6, WHITE, BLACK, 2); break;
	case 7:  lcd_number(100, 210, gy, 6, WHITE, BLACK, 2); break;
	case 8:  lcd_number(100, 235, gz, 6, WHITE, BLACK, 2); break;
	case 9:
		if (link_ok) lcd_text(100, 270, "OK  ", WHITE, BLACK, 2);
		else         lcd_text(100, 270, "LOST", RED,   BLACK, 2);
		break;
	case 10:
		if (plane_mpu_ok) lcd_text(100, 295, "OK  ", WHITE, BLACK, 2);
		else              lcd_text(100, 295, "ERR ", RED,   BLACK, 2);
		break;
	}
}


/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
	nrf_csn_init();          // FIRST: nRF24 off the bus before the LCD talks
	lcd_init();              // also starts SPI1 (PB3/PB4/PB5)
	timer_init();
	joystick_init();
	nrf_init();
	screen_labels();

	uint32_t last_loop = TIM5->CNT;
	uint8_t step = 0;

	while (1) {

		/* wait until 20 ms have passed since the last turn */
		while (TIM5->CNT - last_loop < 20000);
		last_loop += 20000;

		/* ---- A. joystick + button -> command ---- */
		joy_x  = adc_average(0);
		joy_y  = adc_average(1);
		button = ((GPIOC->IDR >> 2) & 1) ? 0 : 1;   // pin low = pressed

		cmd[0] = joy_x & 0xFF;
		cmd[1] = joy_x >> 8;
		cmd[2] = joy_y & 0xFF;
		cmd[3] = joy_y >> 8;
		cmd[4] = button;

		/* ---- B. send, get telemetry back ---- */
		if (nrf_send()) {
			telemetry_decode();
			last_telem = TIM5->CNT;
			telem_count++;
		}

		// LINK OK only if the plane's code is alive and sending telemetry.
		// (An ACK alone is not enough: the nRF24 chip ACKs even if the
		//  plane's processor is stuck.)
		link_ok = (telem_count > 0 && TIM5->CNT - last_telem < 500000) ? 1 : 0;

		/* ---- C. screen: one value per turn, all 11 in 0.22 s ---- */
		screen_step(step);
		step++;
		if (step > 10)
			step = 0;
	}
}
