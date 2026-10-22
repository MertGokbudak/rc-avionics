#include <stdint.h>
#include "gpio.h"
#include "spi.h"
#include "uart.h"
#include "adc.h"
#include "lcd.h"

void joystick_init(void){

	gpio_init(GPIOA,0,gpio_analog,gpio_push_pull,0);
	gpio_init(GPIOA,1,gpio_analog,gpio_push_pull,0);
	//gpio_init(GPIOA,4,gpio_analog,gpio_push_pull,0);
	//gpio_init(GPIOB,0,gpio_analog,gpio_push_pull,0);
	adc_init(ADC1);

	// Sample time = 84 cycles for channels 0, 1(SMPR2: 3 bits per channel)
	ADC1->SMPR2 |= (4 << 0) | (4 << 3);
	//ADC1->SMPR2 |= (4 << 12) | (4 << 24); //4,8
}

uint16_t joystick_values[4];

void joystick_read(void){

	joystick_values[0] = adc_read(ADC1, 0);
	joystick_values[1] = adc_read(ADC1, 1);
	//joystick_values[2] = adc_read(ADC1, 4);
	//joystick_values[3] = adc_read(ADC1, 8);
}

void hc12_init(void){

	gpio_init(GPIOC, 10, gpio_af, gpio_push_pull, 7);
	gpio_init(GPIOC, 11, gpio_af, gpio_push_pull, 7);

	uart_init(USART3, (16000000/19200));
}

void nrf24_init(void){

	//SCK and MOSI using same pins with lcd (SPI1)
	gpio_init(GPIOB,4,gpio_af,gpio_push_pull,5);//MISO
	gpio_high(GPIOB,12);
	gpio_init(GPIOB,12,gpio_output,gpio_push_pull,0);//CS
	gpio_low(GPIOC,7);
	gpio_init(GPIOC,7,gpio_output,gpio_push_pull,0);//CE

}

// Single-byte command, returns STATUS
uint8_t nrf_command(uint8_t cmd)
{
	gpio_low(GPIOB, 12);
	uint8_t status = spi_transfer(SPI1, cmd);
	gpio_high(GPIOB, 12);
	return status;
}

uint8_t nrf24_read(uint8_t reg)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1,0x00 | reg);// read reg
	uint8_t spi_response = spi_transfer(SPI1, 0xFF); // dummy byte to read
	gpio_high(GPIOB, 12);
	return spi_response;
}

void nrf24_write(uint8_t reg, uint8_t value)
{
	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, 0x20 | reg);//write to reg
	spi_transfer(SPI1, value);
	gpio_high(GPIOB, 12);
}


void nrf24_send_packet(uint16_t *data, uint8_t len)
{
	nrf24_write(0x07, 0x30);       // clear old TX_DS / MAX_RT
	nrf_command(0xE1);             // FLUSH_TX: drop old unsent packet

	gpio_low(GPIOB, 12);
	spi_transfer(SPI1, 0xA0);      // W_TX_PAYLOAD
	for (uint8_t i = 0; i < len; i++){//NRF sends 8 byte now can be increased
		spi_transfer(SPI1, data[i] & 0xFF);
		spi_transfer(SPI1, data[i] >> 8);
	}
	gpio_high(GPIOB, 12);

	gpio_high(GPIOC, 7);           // CE pulse > 10 us: send
	for (volatile uint32_t i = 0; i < 50; i++);
	gpio_low(GPIOC, 7);
}

uint8_t telemetry[39];
uint8_t telemetry_index= 0;
uint8_t packet_ready = 0;

void receive_telemetry(void){

	if (!uart_available(USART3))
		return;

	uint8_t t = uart_receive(USART3);

	if (telemetry_index == 0 && t != 0x55)
		return;

	telemetry[telemetry_index++] = t;

	if (telemetry_index == 39) {
		packet_ready = 1;
		telemetry_index = 0;
	}
}


int main(void){

	lcd_init();
	joystick_init();
	hc12_init();
	nrf24_init();


	for (volatile uint32_t i = 0; i < 400000; i++);  // ~100 ms: chip power-on, BEFORE first write

	nrf24_write(0x00, 0x0E);// CONFIG: CRC on, 2-byte CRC, power up, TX mode

	for (volatile uint32_t i = 0; i < 10000; i++);   // ~2.5 ms: PWR_UP settle, AFTER CONFIG


	lcd_fill(0, 0, 240, 320, BLACK);       // clear screen once
	// Group headers (small)
	lcd_text(10, 12,  "JOYSTICK", CYAN, BLACK, 1);
	lcd_text(10, 70,  "ACCEL",    CYAN, BLACK, 1);
	lcd_text(10, 148, "GYRO",     CYAN, BLACK, 1);
	lcd_text(10, 226, "GPS",      CYAN, BLACK, 1);
	lcd_text(10, 284, "BARO",     CYAN, BLACK, 1);
	// Labels
	lcd_text(10, 22,  "X1",  WHITE, BLACK, 2);
	lcd_text(10, 42,  "Y1",  WHITE, BLACK, 2);
	lcd_text(10, 80,  "AX",  WHITE, BLACK, 2);
	lcd_text(10, 100, "AY",  WHITE, BLACK, 2);
	lcd_text(10, 120, "AZ",  WHITE, BLACK, 2);
	lcd_text(10, 158, "GX",  WHITE, BLACK, 2);
	lcd_text(10, 178, "GY",  WHITE, BLACK, 2);
	lcd_text(10, 198, "GZ",  WHITE, BLACK, 2);
	lcd_text(10, 236, "LAT", WHITE, BLACK, 2);
	lcd_text(10, 256, "LON", WHITE, BLACK, 2);
	lcd_text(10,  294, "TE", WHITE, BLACK, 2);
	lcd_text(130, 294, "AL", WHITE, BLACK, 2);

	lcd_text(86,  294, ".", WHITE, BLACK, 2);
	lcd_text(206, 294, ".", WHITE, BLACK, 2);


	int16_t ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

	char lat_s[11] = "0000000000";
	char lon_s[12] = "00000000000";

	int16_t temp = 0, alt = 0;

	uint8_t draw_step = 0;

	while (1) {

		joystick_read();
		nrf24_send_packet(joystick_values, 2);

		for (uint32_t i = 0; i < 1000 && !packet_ready; i++)
			receive_telemetry();

		if (packet_ready) {
			packet_ready = 0;

			uint8_t sum = 0;
			for (int i = 0; i < 38; i++)
				sum += telemetry[i];

			if (sum == telemetry[38]) {

				ax = (int16_t)((telemetry[1]  << 8) | telemetry[2]);
				ay = (int16_t)((telemetry[3]  << 8) | telemetry[4]);
				az = (int16_t)((telemetry[5]  << 8) | telemetry[6]);
				gx = (int16_t)((telemetry[7]  << 8) | telemetry[8]);
				gy = (int16_t)((telemetry[9]  << 8) | telemetry[10]);
				gz = (int16_t)((telemetry[11] << 8) | telemetry[12]);

				for (int k = 0; k < 10; k++) lat_s[k] = telemetry[13 + k];
				for (int k = 0; k < 11; k++) lon_s[k] =telemetry[23 + k];

				temp = (int16_t)((telemetry[34] << 8) | telemetry[35]);
				alt  = (int16_t)((telemetry[36] << 8) | telemetry[37]);

				draw_step = 1;
			}
		}

		if (draw_step) {
			switch (draw_step) {
			case 1:  lcd_number(80, 22,  joystick_values[0], 4, WHITE, BLACK, 2); break;
			case 2:  lcd_number(80, 42,  joystick_values[1], 4, WHITE, BLACK, 2); break;
			case 3:  lcd_number(80, 80,  ax, 6, WHITE, BLACK, 2); break;
			case 4:  lcd_number(80, 100, ay, 6, WHITE, BLACK, 2); break;
			case 5:  lcd_number(80, 120, az, 6, WHITE, BLACK, 2); break;
			case 6:  lcd_number(80, 158, gx, 6, WHITE, BLACK, 2); break;
			case 7:  lcd_number(80, 178, gy, 6, WHITE, BLACK, 2); break;
			case 8:  lcd_number(80, 198, gz, 6, WHITE, BLACK, 2); break;
			case 9:  lcd_text(80, 236, lat_s, WHITE, BLACK, 2); break;
			case 10: lcd_text(80, 256, lon_s, WHITE, BLACK, 2); break;
			case 11: lcd_number(50,  294, temp / 100,        3, WHITE, BLACK, 2); break;
			case 12: lcd_number(98,  294, (temp % 100) / 10, 1, WHITE, BLACK, 2); break;
			case 13: lcd_number(170, 294, alt / 10,          3, WHITE, BLACK, 2); break;
			case 14: lcd_number(218, 294, alt % 10,          1, WHITE, BLACK, 2); break;
			}

			draw_step++;
			if (draw_step > 14){
				draw_step = 0;
			}

		}
	}
}

