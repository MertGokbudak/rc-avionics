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

	uart_init(USART3, (16000000/9600));
}


int main(void){

	lcd_init();
	joystick_init();
	hc12_init();


	lcd_fill(0, 0, 240, 320, BLACK);       // clear screen once

	// Labels
	lcd_text(10, 10,  "X1", WHITE, BLACK, 3);
	lcd_text(10, 50,  "Y1", WHITE, BLACK, 3);
	lcd_text(10, 90,  "X2", WHITE, BLACK, 3);
	lcd_text(10, 130, "Y2", WHITE, BLACK, 3);

	uint8_t counter = 0;

	while (1) {
		joystick_read();

		uart_transmit(USART3, 0xAA);
		uart_transmit(USART3, joystick_values[0] >> 8);
		uart_transmit(USART3, joystick_values[0] & 0xFF);

		uart_transmit(USART3, joystick_values[1] >> 8);
		uart_transmit(USART3, joystick_values[1] & 0xFF);


		counter++;
		if (counter >= 5) {
			counter = 0;
			lcd_number(80, 10,  joystick_values[0], 4, WHITE, BLACK, 3);
			lcd_number(80, 50,  joystick_values[1], 4, WHITE, BLACK, 3);
			lcd_number(80, 90,  joystick_values[2], 4, WHITE, BLACK, 3);
			lcd_number(80, 130, joystick_values[3], 4, WHITE, BLACK, 3);
		}
	}
}

