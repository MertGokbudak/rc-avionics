#include "stm32f4xx.h"
#include "gpio.h"
#include "spi.h"
#include "adc.h"
#include "lcd.h"
#include "timers.h"
#include "nrf24.h"
#include "cc1101.h"
#include "uart.h"

#define CMD_LEN    7
#define TELEM_LEN  20
#define PRIVATE    1 // 1: hide exact GPS position on the screen, 0: show it (real flight)


uint8_t  cmd[CMD_LEN];
uint16_t joy_x, joy_y, pot;
uint8_t  button;

uint8_t  telem[TELEM_LEN];
int16_t  roll, pitch, alt, temp, speed, course;
int32_t  lat, lon;

uint8_t cmd_ok, tel_ok;
uint8_t tel_miss;


void pc_init(void)
{
	gpio_init(GPIOA, 2, gpio_af, gpio_push_pull, 7); // USART2 TX-ST-Link
	uart_init(USART2, 16000000 / 115200);
}

void pc_send(void)
{
	uint8_t x = 0;
	uart_transmit(USART2, 0xAA);
	uart_transmit(USART2, 0x55);
	for (int i = 0; i < TELEM_LEN; i++) {
		uart_transmit(USART2, telem[i]);
		x ^= telem[i];//XOR checksum
	}
	uart_transmit(USART2, x);
}

void spi1_init(void)
{
	gpio_init(GPIOB, 3, gpio_af, gpio_push_pull, 5); // SCK  D3
	gpio_init(GPIOB, 4, gpio_af, gpio_push_pull, 5); // MISO D5
	gpio_init(GPIOB, 5, gpio_af, gpio_push_pull, 5);  // MOSI D4
	GPIOB->OSPEEDR |= (1U << (3 * 2)) | (1U << (5 * 2));
	spi_init(SPI1, spi_div4); // 4 MHz
}

void inputs_init(void)
{
	gpio_init(GPIOA, 0, gpio_analog, gpio_push_pull, 0); // joy X  A0 ch0
	gpio_init(GPIOA, 1, gpio_analog, gpio_push_pull, 0); // joy Y  A1 ch1
	gpio_init(GPIOB, 0, gpio_analog, gpio_push_pull, 0); // pot    A3 ch8
	adc_init(ADC1);
	ADC1->SMPR2 |= (4U << 0) | (4U << 3) | (4U << 24);  // 84 cycles

	gpio_init(GPIOC, 2, gpio_input, gpio_push_pull, 0);// button
	GPIOC->PUPDR |= (1U << (2 * 2));  // pull-up
}


void read_inputs(void)
{
	joy_x = adc_read(ADC1, 0);
	joy_y = adc_read(ADC1, 1);
	pot   = adc_read(ADC1, 8);

	if ((GPIOC->IDR >> 2) & 1)
		button = 0;
	else
		button = 1;  // pressed = low
}

void send_command(void)
{
	cmd[0] = joy_x & 0xFF;
	cmd[1] = joy_x >> 8;
	cmd[2] = joy_y & 0xFF;
	cmd[3] = joy_y >> 8;
	cmd[4] = pot & 0xFF;
	cmd[5] = pot >> 8;
	cmd[6] = button;
	nrf_send(cmd, CMD_LEN);
}


int16_t get16(int i)
{
	return telem[i] | (telem[i + 1] << 8);
}

int32_t get32(int i)
{
	return (uint32_t)telem[i] | ((uint32_t)telem[i + 1] << 8)
			| ((uint32_t)telem[i + 2] << 16) | ((uint32_t)telem[i + 3] << 24);
}


uint8_t receive_telemetry(void)
{

	if (cc_receive(telem, TELEM_LEN) == 0)
		return 0;

	roll   = get16(0);
	pitch  = get16(2);
	alt    = get16(4);
	temp   = get16(6);
	lat    = get32(8);
	lon    = get32(12);
	speed  = get16(16);
	course = get16(18);

	pc_send();

	return 1;
}


void screen_labels(void)
{
	lcd_fill(0, 0, 240, 320, BLACK);

	lcd_text(10,  10, "CMD",   MAGENTA, BLACK, 2);
	lcd_text(130, 10, "TEL",   MAGENTA, BLACK, 2);
	lcd_fill(0, 32, 240, 1, GRAY);
	lcd_text(10,  44, "ROLL",  BLUE, BLACK, 2);
	lcd_text(10,  70, "PITCH", BLUE, BLACK, 2);
	lcd_text(10,  96, "ALT",   BLUE, BLACK, 2);
	lcd_text(10, 122, "TEMP",  BLUE, BLACK, 2);
	lcd_fill(0, 148, 240, 1, GRAY);
	lcd_text(10, 160, "LAT",   CYAN, BLACK, 2);
	lcd_text(10, 186, "LON",   CYAN, BLACK, 2);
	lcd_text(10, 212, "SPEED", CYAN, BLACK, 2);
	lcd_text(10, 238, "CRS",   CYAN, BLACK, 2);
}

void screen_update(uint8_t step)
{
	switch (step) {
	case 0:
		if (cmd_ok)
			lcd_text(60, 10, "OK  ", GREEN, BLACK, 2);
		else
			lcd_text(60, 10, "LOST", RED, BLACK, 2);

		if (tel_ok)
			lcd_text(180, 10, "OK  ", GREEN, BLACK, 2);
		else
			lcd_text(180, 10, "LOST", RED, BLACK, 2);
		break;
	case 1: lcd_number(100,  44, roll,   10, WHITE, BLACK, 2); break;
	case 2: lcd_number(100,  70, pitch,  10, WHITE, BLACK, 2); break;
	case 3: lcd_number(100,  96, alt/10,    10, WHITE, BLACK, 2); break;
	case 4: lcd_number(100, 122, temp/10,   10, WHITE, BLACK, 2); break;
	case 5:
		if (PRIVATE) {
			lcd_number(100, 160, lat / 100000, 4, WHITE, BLACK, 2);   // ddmm only
			lcd_text(148, 160, "*****", WHITE, BLACK, 2);
		} else {
			lcd_number(100, 160, lat, 10, WHITE, BLACK, 2);
		}
		break;
	case 6:
		if (PRIVATE) {
			lcd_number(100, 186, lon / 100000, 4, WHITE, BLACK, 2);
			lcd_text(148, 186, "*****", WHITE, BLACK, 2);
		} else {
			lcd_number(100, 186, lon, 10, WHITE, BLACK, 2);
		}
		break;
	case 7: lcd_number(100, 212, speed,  10, WHITE, BLACK, 2); break;
	case 8: lcd_number(100, 238, course, 10, WHITE, BLACK, 2); break;
	}
}



int main(void)
{
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

	nrf_csn_init(); // CS pins high before any SPI traffic
	cc_csn_init();
	spi1_init();
	lcd_init();
	timer5_delay_init();
	nrf_init();
	cc_init(TELEM_LEN);
	cc_listen();
	inputs_init();
	pc_init();
	screen_labels();

	uint32_t last_loop = TIM5->CNT;
	uint8_t step = 0;

	while (1) {
		while (TIM5->CNT - last_loop < 20000);   // 50 Hz
		last_loop += 20000;

		read_inputs();

		cmd_ok = nrf_ack_ok();

		send_command();


		if (receive_telemetry())
			tel_miss = 0;
		else if (tel_miss < 25)
			tel_miss++;

		if (tel_miss < 25)
			tel_ok = 1;
		else
			tel_ok = 0;

		screen_update(step);
		step++;
		if (step > 8)
			step = 0;
	}
}
