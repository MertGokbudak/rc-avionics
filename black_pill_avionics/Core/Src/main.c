#include <stdint.h>
#include "gpio.h"
#include "timers.h"
#include "uart.h"

void hc12_init(void){

	gpio_init(GPIOA, 10, gpio_af, gpio_push_pull, 7);
	gpio_init(GPIOA, 9, gpio_af, gpio_push_pull, 7);

	uart_init(USART1, (16000000/9600));
}

void servo_init(void){
	gpio_init(GPIOB, 4, gpio_af, gpio_push_pull, 2); //TIM3 CH 1
	gpio_init(GPIOB, 5, gpio_af, gpio_push_pull, 2); //TIM3 CH 2

	pwm_init(TIM3, 1, 15, 19999);
	pwm_init(TIM3, 2, 15, 19999);

	pwm_set(TIM3, 1, 1500);
	pwm_set(TIM3, 2, 1500);
}

int main(void){

	hc12_init();
	servo_init();

	while(1){

		if (uart_receive(USART1) != 0xAA)// wait for start byte
			continue;

		uint8_t received_signal[4];
		for (int i = 0; i < 4; i++)
			received_signal[i] = uart_receive(USART1);

		uint16_t x1 = (received_signal[0] << 8) | received_signal[1];
		uint16_t y1 = (received_signal[2] << 8) | received_signal[3];

		pwm_set(TIM3, 1, 1000 + (uint32_t)x1 * 1000 / 4095);
		pwm_set(TIM3, 2, 1000 + (uint32_t)y1 * 1000 / 4095);


	}
}

