#include <stdint.h>
#include "gpio.h"
#include "timers.h"
#include "uart.h"
#include "i2c.h"
#include <stdlib.h>   // atof


#define MPU_ADDR 0x68


void hc12_init(void){

	gpio_init(GPIOA, 10, gpio_af, gpio_push_pull, 7);
	gpio_init(GPIOA, 9, gpio_af, gpio_push_pull, 7);

	uart_init(USART1, (16000000/19200));
}


void servo_init(void){
	gpio_init(GPIOB, 4, gpio_af, gpio_push_pull, 2); //TIM3 CH 1
	gpio_init(GPIOB, 5, gpio_af, gpio_push_pull, 2); //TIM3 CH 2

	pwm_init(TIM3, 1, 15, 19999);
	pwm_init(TIM3, 2, 15, 19999);

	pwm_set(TIM3, 1, 1500);
	pwm_set(TIM3, 2, 1500);
}


void mpu6050_init(void){

	gpio_init(GPIOB, 8, gpio_af, gpio_open_drain, 4);
	gpio_init(GPIOB, 9, gpio_af, gpio_open_drain, 4);

	I2C_init(I2C1);

	I2C_write(I2C1, 0x6B, MPU_ADDR, 0x00);   // PWR_MGMT_1 = 0: wake up
}

uint8_t mpu_raw[14];
int16_t ax, ay, az;
int16_t temp_raw;
int16_t temp_x10;


void telemetry(int16_t t)
{
	uart_transmit(USART1, (uint8_t)(t >> 8));
	uart_transmit(USART1, (uint8_t)(t & 0xFF));
}


volatile uint8_t gps_ready = 0;
int32_t gps_lat = 0;
int32_t gps_lon = 0;

void gps_init(void){

	gpio_init(GPIOA,3, gpio_af, gpio_push_pull, 7);
	uart_init(USART2, (16000000/9600));

	USART2->CR1 |= USART_CR1_RXNEIE;
	NVIC_EnableIRQ(USART2_IRQn);
}

char gps_character[100];
uint8_t gps_position = 0;

void USART2_IRQHandler(void)
{
	if (USART2->SR & USART_SR_RXNE) {

		char c = USART2->DR;

		if (gps_ready) return;

		if (c == '$'){
			gps_position = 0;
		}
		if (gps_position < 99){
			gps_character[gps_position++] = c;
		}

		if (c == '\n') {
			gps_character[gps_position] = 0;
			if (gps_character[3] == 'G' && gps_character[4] == 'G' && gps_character[5] == 'A')
				gps_ready = 1;
		}
	}
}

int gps_field(int n)
{
	int commas = 0;
	int i = 0;
	while (commas < n) {
		if (gps_character[i] == ',')
			commas++;
		i++;
	}
	return i;
}


int main(void){

	hc12_init();
	servo_init();
	mpu6050_init();
	gps_init();

	while(1){

		if (uart_available(USART1)){

			if (uart_receive(USART1) == 0xAA){

				uint8_t received_signal[4];
				for (int i = 0; i < 4; i++)
					received_signal[i] = uart_receive(USART1);

				uint16_t x1 = (received_signal[0] << 8) | received_signal[1];
				uint16_t y1 = (received_signal[2] << 8) | received_signal[3];

				pwm_set(TIM3, 1, 1000 + (uint32_t)x1 * 1000 / 4095);
				pwm_set(TIM3, 2, 1000 + (uint32_t)y1 * 1000 / 4095);

				I2C_read(I2C1, 0x3B, MPU_ADDR, mpu_raw, 14);

				ax = (int16_t)((mpu_raw[0] << 8) | mpu_raw[1]);
				ay = (int16_t)((mpu_raw[2] << 8) | mpu_raw[3]);
				az = (int16_t)((mpu_raw[4] << 8) | mpu_raw[5]);
				temp_raw = (int16_t)((mpu_raw[6] << 8) | mpu_raw[7]);
				temp_x10 = temp_raw * 10 / 340 + 365;


				uart_transmit(USART1, 0x55);
				telemetry(ax);
				telemetry(ay);
				telemetry(az);
				telemetry(temp_x10);

				if (gps_ready) {
					for (int g = gps_field(2); g < (gps_field(3)-1) ; g++){
						uart_transmit(USART1, gps_character[g]);}

					for (int p = gps_field(4); p < (gps_field(5)-1) ; p++){
						uart_transmit(USART1, gps_character[p]);}

					gps_ready = 0;
				}

			}
		}
	}
}
