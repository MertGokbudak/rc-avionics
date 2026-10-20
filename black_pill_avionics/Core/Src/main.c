#include <stdint.h>
#include "gpio.h"
#include "timers.h"
#include "uart.h"
#include "i2c.h"
#include "spi.h"
#include "bmp280.h"

uint8_t bmp_id;
int32_t bmp_temp, bmp_press, bmp_alt;

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
int16_t ax, ay, az, gx, gy, gz;


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

char lat_s[10] = {'0','0','0','0','0','0','0','0','0','0'};
char lon_s[11] = {'0','0','0','0','0','0','0','0','0','0','0'};

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

void nrf24_init(void){

	gpio_init(GPIOA,5,gpio_af,gpio_push_pull,5);//SCK
	gpio_init(GPIOA,6,gpio_af,gpio_push_pull,5);//MISO
	gpio_init(GPIOA,7,gpio_af,gpio_push_pull,5);//MOSI
	gpio_high(GPIOA,4);
	gpio_init(GPIOA,4,gpio_output,gpio_push_pull,0);//CS
	gpio_low(GPIOB,0);
	gpio_init(GPIOB,0,gpio_output,gpio_push_pull,0);//CE

	spi_init(SPI1, 1);
	GPIOA->PUPDR |= (1 << 12);// PA6 pull-up: reads 1 if nRF24 missing
}


uint8_t nrf_command(uint8_t cmd){

	gpio_low(GPIOA, 4);
	uint8_t status = spi_transfer(SPI1, cmd);
	gpio_high(GPIOA, 4);
	return status;
}

uint8_t nrf24_read(uint8_t reg){

	gpio_low(GPIOA, 4);
	spi_transfer(SPI1,0x00 | reg);// read reg
	uint8_t spi_response = spi_transfer(SPI1, 0xFF); // dummy byte to read
	gpio_high(GPIOA, 4);
	return spi_response;
}

void nrf24_write(uint8_t reg, uint8_t value){

	gpio_low(GPIOA, 4);
	spi_transfer(SPI1, 0x20 | reg);//write to reg
	spi_transfer(SPI1, value);
	gpio_high(GPIOA, 4);
}

void nrf24_config(void){
	for (volatile uint32_t i = 0; i < 400000; i++);  // ~100 ms power-on wait
	nrf24_write(0x00, 0x0F);   // CONFIG: CRC on, 2-byte CRC, power up, RX mode
	for (volatile uint32_t i = 0; i < 10000; i++);   // >1.5 ms
	nrf24_write(0x11, 4);      // RX_PW_P0: packet size = 4 bytes
	nrf24_write(0x07, 0x70);   // clear old flags
	nrf_command(0xE2);         // FLUSH_RX
	gpio_high(GPIOB, 0);       // CE high: start listening
}

uint8_t nrf24_receive(uint8_t *data, uint8_t len){

	if ((nrf_command(0xFF) & 0x0E) == 0x0E)  // RX FIFO empty
		return 0;

	gpio_low(GPIOA, 4);
	spi_transfer(SPI1, 0x61);                // R_RX_PAYLOAD
	for (uint8_t i = 0; i < len; i++)
		data[i] = spi_transfer(SPI1, 0xFF);
	gpio_high(GPIOA, 4);

	nrf24_write(0x07, 0x40);                 // clear RX_DR
	return 1;
}

volatile uint32_t ms_ticks = 0;
void SysTick_Handler(void) { ms_ticks++; }


int main(void){

	hc12_init();
	servo_init();
	mpu6050_init();
	gps_init();
	nrf24_init();
	nrf24_config();

	bmp_id = bmp280_init();
	bmp280_zero();

	SysTick_Config(16000);
	uint32_t last_telemetry = 0;

	while(1){


		uint8_t pkt[4];
		if (nrf24_receive(pkt, 4)) {
			uint16_t x1 = pkt[0] | (pkt[1] << 8);
			uint16_t y1 = pkt[2] | (pkt[3] << 8);
			pwm_set(TIM3, 1, 1000 + (uint32_t)x1 * 1000 / 4095);
			pwm_set(TIM3, 2, 1000 + (uint32_t)y1 * 1000 / 4095);
		}

		if (ms_ticks - last_telemetry >= 100) {
			last_telemetry = ms_ticks;


			I2C_read(I2C1, 0x3B, MPU_ADDR, mpu_raw, 14);
			bmp280_read(&bmp_temp, &bmp_press);
			bmp_alt = bmp280_altitude_dm(bmp_press);

			ax = (int16_t)((mpu_raw[0] << 8) | mpu_raw[1]);
			ay = (int16_t)((mpu_raw[2] << 8) | mpu_raw[3]);
			az = (int16_t)((mpu_raw[4] << 8) | mpu_raw[5]);
			gx = (int16_t)((mpu_raw[8]  << 8) | mpu_raw[9]);
			gy = (int16_t)((mpu_raw[10] << 8) | mpu_raw[11]);
			gz = (int16_t)((mpu_raw[12] << 8) | mpu_raw[13]);


			if (gps_ready){

				if (gps_character[gps_field(2)] != ',') {

					for (int x = 0; x < 10; x++)
					{ lat_s[x] = gps_character[gps_field(2) + x];}
					for (int y = 0; y< 11; y++)
					{ lon_s[y ] = gps_character[gps_field(4) + y];}
				}
				gps_ready = 0;
			}

			uint8_t packet[39];

			packet[0]  = 0x55;
			packet[1]  = ax >> 8;  packet[2]  = ax & 0xFF;
			packet[3]  = ay >> 8;  packet[4]  = ay & 0xFF;
			packet[5]  = az >> 8;  packet[6]  = az & 0xFF;
			packet[7]  = gx >> 8;  packet[8]  = gx & 0xFF;
			packet[9]  = gy >> 8;  packet[10] = gy & 0xFF;
			packet[11] = gz >> 8;  packet[12] = gz & 0xFF;

			for (int k = 0; k < 10; k++) {packet[13 + k] = lat_s[k];}
			for (int k = 0; k < 11; k++) {packet[23 + k] = lon_s[k];}

			packet[34] = bmp_temp >> 8;  packet[35] = bmp_temp & 0xFF;
			packet[36] = bmp_alt  >> 8;  packet[37] = bmp_alt  & 0xFF;

			uint8_t sum = 0;
			for (int i = 0; i < 38; i++){
				sum += packet[i];
			}
			packet[38] = sum;

			for (int i = 0; i < 39; i++){
				uart_transmit(USART1, packet[i]);}

		}
	}
}
