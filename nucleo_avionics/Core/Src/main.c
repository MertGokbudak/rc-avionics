#include "stm32f4xx.h"
#include "gpio.h"
#include "spi.h"
#include "adc.h"
#include "lcd.h"
#include "timers.h"
#include "nrf24.h"
#include "cc1101.h"

#define CMD_LEN    7
#define TELEM_LEN  20


uint8_t  cmd[CMD_LEN];
uint16_t joy_x, joy_y, pot;
uint8_t  button;

uint8_t  telem[TELEM_LEN];
int16_t  roll, pitch, alt, temp, speed, course;
int32_t  lat, lon;


uint32_t telem_count;
uint32_t last_telem;
uint8_t  link_ok;


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
	return (uint32_t)telem[i]
						   | ((uint32_t)telem[i + 1] << 8)
						   | ((uint32_t)telem[i + 2] << 16)
						   | ((uint32_t)telem[i + 3] << 24);
}


void receive_telemetry(void)
{

	if (cc_receive(telem, TELEM_LEN) == 0)
		return;

	roll   = get16(0);
	pitch  = get16(2);
	alt    = get16(4);
	temp   = get16(6);
	lat    = get32(8);
	lon    = get32(12);
	speed  = get16(16);
	course = get16(18);

	telem_count++;
	last_telem = TIM5->CNT;
}


void screen_labels(void)
{
	lcd_fill(0, 0, 240, 320, BLACK);

	lcd_text(10,  10, "LINK",  CYAN, BLACK, 2);
	lcd_text(10,  36, "ROLL",  CYAN, BLACK, 2);
	lcd_text(10,  62, "PITCH", CYAN, BLACK, 2);
	lcd_text(10,  88, "ALT",   CYAN, BLACK, 2);
	lcd_text(10, 114, "TEMP",  CYAN, BLACK, 2);
	lcd_text(10, 140, "LAT",   CYAN, BLACK, 2);
	lcd_text(10, 166, "LON",   CYAN, BLACK, 2);
	lcd_text(10, 192, "SPEED", CYAN, BLACK, 2);
	lcd_text(10, 218, "CRS",   CYAN, BLACK, 2);
}

void screen_update(uint8_t step)
{
	switch (step) {
	case 0:
		if (link_ok)
			lcd_text(90, 10, "OK  ", WHITE, BLACK, 2);
		else
			lcd_text(90, 10, "LOST", RED, BLACK, 2);
		break;
	case 1: lcd_number(90,  36, roll,   10, WHITE, BLACK, 2); break;
	case 2: lcd_number(90,  62, pitch,  10, WHITE, BLACK, 2); break;
	case 3: lcd_number(90,  88, alt,    10, WHITE, BLACK, 2); break;
	case 4: lcd_number(90, 114, temp,   10, WHITE, BLACK, 2); break;
	case 5: lcd_number(90, 140, lat,    10, WHITE, BLACK, 2); break;
	case 6: lcd_number(90, 166, lon,    10, WHITE, BLACK, 2); break;
	case 7: lcd_number(90, 192, speed,  10, WHITE, BLACK, 2); break;
	case 8: lcd_number(90, 218, course, 10, WHITE, BLACK, 2); break;
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
	screen_labels();

	uint32_t last_loop = TIM5->CNT;
	uint8_t step = 0;

	while (1) {
		while (TIM5->CNT - last_loop < 20000);   // 50 Hz
		last_loop += 20000;

		read_inputs();
		send_command();
		receive_telemetry();

		if (telem_count > 0 && TIM5->CNT - last_telem < 500000)
			link_ok = 1;
		else
			link_ok = 0;

		screen_update(step);
		step++;
		if (step > 8)
			step = 0;
	}
}
