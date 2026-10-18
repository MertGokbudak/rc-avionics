#include <stdint.h>
#include "gpio.h"
#include "spi.h"
#include "uart.h"
#include "adc.h"
#include "lcd.h"

void joystick_init(void){

	gpio_init(GPIOA,0,gpio_analog,gpio_push_pull,0);
	gpio_init(GPIOA,1,gpio_analog,gpio_push_pull,0);
	gpio_init(GPIOA,4,gpio_analog,gpio_push_pull,0);
	gpio_init(GPIOB,0,gpio_analog,gpio_push_pull,0);
	adc_init(ADC1);

	// Sample time = 84 cycles for channels 0, 1, 4, 8 (SMPR2: 3 bits per channel)
	ADC1->SMPR2 |= (4 << 0) | (4 << 3) | (4 << 12) | (4 << 24);
}

uint16_t joystick_values[4];

void joystick_read(void){

	joystick_values[0] = adc_read(ADC1, 0);
	joystick_values[1] = adc_read(ADC1, 1);
	joystick_values[2] = adc_read(ADC1, 4);
	joystick_values[3] = adc_read(ADC1, 8);
}

void hc12_init(void){

	gpio_init(GPIOC, 10, gpio_af, gpio_push_pull, 7);
	gpio_init(GPIOC, 11, gpio_af, gpio_push_pull, 7);

	uart_init(USART3, (16000000/19200));
}


int main(void){

	lcd_init();
	joystick_init();
	hc12_init();

	lcd_fill(0, 0, 240, 320, BLACK);       // clear screen once

	// Labels
	lcd_text(10, 10,  "X1",  WHITE, BLACK, 2);
	lcd_text(10, 40,  "Y1",  WHITE, BLACK, 2);
	lcd_text(10, 70,  "AX",  WHITE, BLACK, 2);
	lcd_text(10, 100, "AY",  WHITE, BLACK, 2);
	lcd_text(10, 130, "AZ",  WHITE, BLACK, 2);
	lcd_text(10, 160, "T",   WHITE, BLACK, 2);
	lcd_text(10, 190, "RX",  WHITE, BLACK, 2);
	lcd_text(10, 220, "LAT", WHITE, BLACK, 2);
	lcd_text(10, 250, "LON", WHITE, BLACK, 2);

	uint8_t counter = 0;
	uint32_t rx_count = 0;

	int16_t ax = 0, ay = 0, az = 0, temp_x10 = 0;

	char lat_s[11] = "0000000000";
	char lon_s[12] = "00000000000";


	while (1) {

		joystick_read();

		uart_transmit(USART3, 0xAA);
		uart_transmit(USART3, joystick_values[0] >> 8);
		uart_transmit(USART3, joystick_values[0] & 0xFF);

		uart_transmit(USART3, joystick_values[1] >> 8);
		uart_transmit(USART3, joystick_values[1] & 0xFF);

		uint32_t t = 0;
		while (!uart_available(USART3) && t < 100000){
			t++;
		}

		if (uart_available(USART3) && uart_receive(USART3) == 0x55) {
			uint8_t d[8];
			for (int i = 0; i < 8; i++)
				d[i] = uart_receive(USART3);
			ax       = (int16_t)((d[0] << 8) | d[1]);
			ay       = (int16_t)((d[2] << 8) | d[3]);
			az       = (int16_t)((d[4] << 8) | d[5]);
			temp_x10 = (int16_t)((d[6] << 8) | d[7]);


			char g[21];
			int n = 0;
			uint32_t w = 0;
			while (n < 21 && w < 20000) {
				if (uart_available(USART3)) {
					g[n++] = uart_receive(USART3);
					w = 0;
				} else {
					w++;
				}
			}

			if (n == 21) {
				for (int k = 0; k < 10; k++) lat_s[k] = g[k];
				for (int k = 0; k < 11; k++) lon_s[k] = g[10 + k];
			}

			rx_count++;
		}

		counter++;
		if (counter >= 5) {
			counter = 0;

			lcd_number(80, 10,  joystick_values[0], 4, WHITE, BLACK, 2);
			lcd_number(80, 40,  joystick_values[1], 4, WHITE, BLACK, 2);

			lcd_number(80, 70,  ax,       6, WHITE, BLACK, 2);
			lcd_number(80, 100, ay,       6, WHITE, BLACK, 2);
			lcd_number(80, 130, az,       6, WHITE, BLACK, 2);
			lcd_number(80, 160, temp_x10, 6, WHITE, BLACK, 2);
			lcd_number(80, 190, rx_count, 6, WHITE, BLACK, 2);

			lcd_text(80, 220, lat_s, WHITE, BLACK, 2);
			lcd_text(80, 250, lon_s, WHITE, BLACK, 2);
		}
	}
}

