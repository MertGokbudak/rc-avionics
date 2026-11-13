/* CC1101 driver: fixed length + CRC, polled (no GDO interrupts).
 * RX on the remote, TX on the plane.
 * Register table: TI SmartRF Studio values for 434.4 MHz, GFSK 38.4 kbps.
 * Written with help from an AI assistant (Claude).
 */

#include "cc1101.h"
#include "gpio.h"
#include "spi.h"
#include "timers.h"

// pins - Black Pill: CSN PB12   (Nucleo: CSN PA10)
#define CC_SPI       SPI1
#define CC_CSN_PORT  GPIOB
#define CC_CSN_PIN   12

uint8_t cc_version;


void cc_strobe(uint8_t c)                         // one-byte command
{
	gpio_low(CC_CSN_PORT, CC_CSN_PIN);
	spi_transfer(CC_SPI, c);
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
}

void cc_write(uint8_t reg, uint8_t value)
{
	gpio_low(CC_CSN_PORT, CC_CSN_PIN);
	spi_transfer(CC_SPI, reg);
	spi_transfer(CC_SPI, value);
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
}

uint8_t cc_status(uint8_t reg)                    // status register 0x30..0x3D
{
	gpio_low(CC_CSN_PORT, CC_CSN_PIN);
	spi_transfer(CC_SPI, 0xC0 | reg);             // read + burst = status, not strobe
	uint8_t value = spi_transfer(CC_SPI, 0x00);
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
	return value;
}

uint8_t cc_read(uint8_t reg)                      // config register 0x00..0x2E
{
	gpio_low(CC_CSN_PORT, CC_CSN_PIN);
	spi_transfer(CC_SPI, 0x80 | reg);             // 0x80 = read, single byte
	uint8_t value = spi_transfer(CC_SPI, 0x00);
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
	return value;
}

// 434.4 MHz, GFSK 38.4 kbps, fixed length + CRC (same table on both boards)
const uint8_t cc_config[][2] = {
	{0x03, 0x47}, {0x08, 0x04}, {0x0B, 0x06},
	{0x0D, 0x10}, {0x0E, 0xB5}, {0x0F, 0x2B},
	{0x10, 0xCA}, {0x11, 0x83}, {0x12, 0x13}, {0x15, 0x35},
	{0x18, 0x18}, {0x19, 0x16}, {0x1B, 0x43}, {0x20, 0xFB},
	{0x23, 0xE9}, {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F},
	{0x2C, 0x81}, {0x2D, 0x35}, {0x2E, 0x09},
};

void cc_csn_init(void)
{
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
	gpio_init(CC_CSN_PORT, CC_CSN_PIN, gpio_output, gpio_push_pull, 0);
}

void cc_init(uint8_t len)
{
	cc_strobe(0x30);                              // SRES: reset
	delay5_us(1000);
	for (int i = 0; i < (int)(sizeof(cc_config) / 2); i++)
		cc_write(cc_config[i][0], cc_config[i][1]);
	cc_write(0x06, len);                          // PKTLEN
	cc_write(0x3E, 0x60);                         // PATABLE: 0 dBm (only used when sending)
	cc_version = cc_status(0x31);                 // VERSION
}


/* ---- remote: receive ---- */

void cc_listen(void)
{
	cc_strobe(0x3A);                              // SFRX: empty RX FIFO
	cc_strobe(0x34);                              // SRX: listen
}

uint8_t cc_receive(uint8_t *buf, uint8_t len)
{
	uint8_t state = cc_status(0x35) & 0x1F;       // MARCSTATE
	if (state != 0x01 && state != 0x11)           // still listening
		return 0;

	uint8_t ok = 0;
	if (state == 0x01) {                          // IDLE = a packet arrived
		gpio_low(CC_CSN_PORT, CC_CSN_PIN);
		spi_transfer(CC_SPI, 0xFF);               // RX FIFO, burst read
		for (uint8_t i = 0; i < len; i++)
			buf[i] = spi_transfer(CC_SPI, 0x00);
		spi_transfer(CC_SPI, 0x00);               // RSSI
		ok = spi_transfer(CC_SPI, 0x00) >> 7;     // bit 7 = CRC OK
		gpio_high(CC_CSN_PORT, CC_CSN_PIN);
	}
	cc_listen();                                  // listen again
	return ok;
}


/* ---- plane: send ---- */

uint8_t cc_send(uint8_t *buf, uint8_t len)
{
	if ((cc_status(0x35) & 0x1F) != 0x01)         // not IDLE: last packet still going
		return 0;

	cc_strobe(0x3B);                              // SFTX: empty TX FIFO
	gpio_low(CC_CSN_PORT, CC_CSN_PIN);
	spi_transfer(CC_SPI, 0x7F);                   // TX FIFO, burst write
	for (uint8_t i = 0; i < len; i++)
		spi_transfer(CC_SPI, buf[i]);
	gpio_high(CC_CSN_PORT, CC_CSN_PIN);
	cc_strobe(0x35);                              // STX: send (back to IDLE when done)
	return 1;
}
