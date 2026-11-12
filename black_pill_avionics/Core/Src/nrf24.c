/* nRF24L01+ driver: fixed payload, auto-ACK. TX on the remote, RX on the plane.
 * Register values from the nRF24L01+ datasheet.
 * Written with help from an AI assistant (Claude).
 */

#include "nrf24.h"
#include "gpio.h"
#include "spi.h"
#include "timers.h"

// pins - Black Pill: CSN PA4, CE PB0   (Nucleo: CSN PB12, CE PC7)
#define NRF_SPI       SPI1
#define NRF_CSN_PORT  GPIOA
#define NRF_CSN_PIN   4
#define NRF_CE_PORT   GPIOB
#define NRF_CE_PIN    0

uint8_t nrf_ch;


void nrf_csn_init(void)
{
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);
	gpio_init(NRF_CSN_PORT, NRF_CSN_PIN, gpio_output, gpio_push_pull, 0);
}

void nrf_cmd(uint8_t c)
{
	gpio_low(NRF_CSN_PORT, NRF_CSN_PIN);
	spi_transfer(NRF_SPI, c);
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);
}

void nrf_write(uint8_t reg, uint8_t value)
{
	gpio_low(NRF_CSN_PORT, NRF_CSN_PIN);
	spi_transfer(NRF_SPI, 0x20 | reg);           // 0x20 = write
	spi_transfer(NRF_SPI, value);
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);
}

uint8_t nrf_read(uint8_t reg)
{
	gpio_low(NRF_CSN_PORT, NRF_CSN_PIN);
	spi_transfer(NRF_SPI, 0x00 | reg);           // 0x00 = read
	uint8_t value = spi_transfer(NRF_SPI, 0xFF); // dummy byte
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);
	return value;
}


/* ---- remote: send ---- */

void nrf_init(void)
{
	gpio_low(NRF_CE_PORT, NRF_CE_PIN);
	gpio_init(NRF_CE_PORT, NRF_CE_PIN, gpio_output, gpio_push_pull, 0);

	delay5_us(100000);                    // power-on time
	nrf_write(0x00, 0x0E);                // CONFIG: CRC 2 bytes, power up, TX
	delay5_us(2000);                      // wake-up time
	nrf_write(0x05, 80);                  // RF_CH: 2480 MHz
	nrf_write(0x06, 0x00);                // RF_SETUP: 1 Mbps, lowest power (desk)
	nrf_write(0x1C, 0x00);                // DYNPD off   } clear old settings:
	nrf_write(0x1D, 0x00);                // FEATURE off } chip keeps them across MCU resets

	nrf_ch = nrf_read(0x05);              // 80 = SPI + wiring OK
}

void nrf_send(uint8_t *buf, uint8_t len)
{
	nrf_write(0x07, 0x70);                // clear flags (MAX_RT blocks sending)
	nrf_cmd(0xE1);                        // FLUSH_TX: drop an old unsent packet

	gpio_low(NRF_CSN_PORT, NRF_CSN_PIN);
	spi_transfer(NRF_SPI, 0xA0);          // W_TX_PAYLOAD
	for (uint8_t i = 0; i < len; i++)
		spi_transfer(NRF_SPI, buf[i]);
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);

	gpio_high(NRF_CE_PORT, NRF_CE_PIN);   // CE pulse > 10 us = send
	delay5_us(15);
	gpio_low(NRF_CE_PORT, NRF_CE_PIN);
}


/* ---- plane: receive ---- */

void nrf_init_rx(uint8_t len)
{
	gpio_low(NRF_CE_PORT, NRF_CE_PIN);
	gpio_init(NRF_CE_PORT, NRF_CE_PIN, gpio_output, gpio_push_pull, 0);

	delay5_us(100000);
	nrf_write(0x00, 0x0F);                // CONFIG: CRC 2 bytes, power up, RX
	delay5_us(2000);
	nrf_write(0x05, 80);                  // RF_CH: same as remote
	nrf_write(0x06, 0x00);                // RF_SETUP: same as remote
	nrf_write(0x1C, 0x00);                // DYNPD off
	nrf_write(0x1D, 0x00);                // FEATURE off
	nrf_write(0x11, len);                 // RX_PW_P0: packet size
	nrf_write(0x07, 0x70);                // clear flags
	nrf_cmd(0xE2);                        // FLUSH_RX
	gpio_high(NRF_CE_PORT, NRF_CE_PIN);   // CE high = listen (stays high)

	nrf_ch = nrf_read(0x05);
}

uint8_t nrf_receive(uint8_t *buf, uint8_t len)
{
	if (nrf_read(0x17) & 0x01)            // FIFO_STATUS bit 0: RX FIFO empty
		return 0;

	gpio_low(NRF_CSN_PORT, NRF_CSN_PIN);
	spi_transfer(NRF_SPI, 0x61);          // R_RX_PAYLOAD
	for (uint8_t i = 0; i < len; i++)
		buf[i] = spi_transfer(NRF_SPI, 0xFF);
	gpio_high(NRF_CSN_PORT, NRF_CSN_PIN);

	nrf_write(0x07, 0x40);                // clear RX_DR
	return 1;
}
