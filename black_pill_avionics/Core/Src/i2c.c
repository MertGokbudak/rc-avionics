#include "i2c.h"
#include "gpio.h"

/* I2C master driver, polled, with timeouts and bus recovery.
 *
 * Every wait has a timeout: if the slave or the bus misbehaves the function
 * gives up, sends STOP and returns 0 instead of hanging.
 * If the bus stays stuck (a slave holding SDA low), call I2C_recover().
 */

#define I2C_TIMEOUT  20000      // loop counts, roughly 5 ms at 16 MHz


void I2C_init(I2C_TypeDef *i2c)
{
	if (i2c == I2C1) RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
	if (i2c == I2C2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
	if (i2c == I2C3) RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;

	i2c->CR2 = 16;      //Bits 5:0 FREQ[5:0]: Peripheral clock frequency
	i2c->CCR = 80;      //Bits 11:0 CCR[11:0]: Clock control register in Fm/Sm mode (controller mode)
	i2c->TRISE = 17;    //Bits 5:0 TRISE[5:0]: Maximum rise time in Fm/Sm mode (controller mode)

	i2c->CR1 |= (I2C_CR1_PE);
}


// Wait until a bit in SR1 is set.
// 1 = OK, 0 = slave answered NACK (AF) or timeout
static uint8_t wait_sr1(I2C_TypeDef *i2c, uint32_t mask)
{
	uint32_t t = I2C_TIMEOUT;

	while (!(i2c->SR1 & mask)) {
		if (i2c->SR1 & I2C_SR1_AF) {        // slave did not acknowledge
			i2c->SR1 &= ~I2C_SR1_AF;
			return 0;
		}
		t--;
		if (t == 0)                         // nothing happened for too long
			return 0;
	}
	return 1;
}

// Something went wrong: release the bus, return 0
static uint8_t fail(I2C_TypeDef *i2c)
{
	i2c->CR1 |= I2C_CR1_STOP;
	return 0;
}

// Wait until the STOP we asked for has been sent
static uint8_t wait_stop(I2C_TypeDef *i2c)
{
	uint32_t t = I2C_TIMEOUT;

	while (i2c->CR1 & I2C_CR1_STOP) {
		t--;
		if (t == 0)
			return fail(i2c);
	}
	return 1;
}


uint8_t I2C_read(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
		uint8_t *data_array,
		uint8_t len)
{
	if (len == 0) return 0;

	if (i2c->SR2 & I2C_SR2_BUSY)             // bus is held by someone: don't even try
		return fail(i2c);

	// 1. write the register address
	i2c->CR1 |= I2C_CR1_START;
	if (!wait_sr1(i2c, I2C_SR1_SB))   return fail(i2c);
	i2c->DR = (adress << 1) | 0;              // address + write
	if (!wait_sr1(i2c, I2C_SR1_ADDR)) return fail(i2c);
	(void)i2c->SR1;                           // reading SR1 then SR2 clears ADDR
	(void)i2c->SR2;

	i2c->DR = reg;
	if (!wait_sr1(i2c, I2C_SR1_BTF))  return fail(i2c);

	// 2. repeated START, read the data
	i2c->CR1 |= I2C_CR1_START;
	if (!wait_sr1(i2c, I2C_SR1_SB))   return fail(i2c);
	i2c->DR = (adress << 1) | 1;              // address + read
	if (!wait_sr1(i2c, I2C_SR1_ADDR)) return fail(i2c);

	if (len > 1) i2c->CR1 |=  (I2C_CR1_ACK);
	else         i2c->CR1 &= ~(I2C_CR1_ACK);  // single byte: NACK it
	(void)i2c->SR1;
	(void)i2c->SR2;

	for (uint8_t i = 0; i < len; i++) {
		if (i == len - 1) {                   // last byte: NACK + STOP
			i2c->CR1 &= ~(I2C_CR1_ACK);
			i2c->CR1 |=  (I2C_CR1_STOP);
		}
		if (!wait_sr1(i2c, I2C_SR1_RXNE)) return fail(i2c);
		data_array[i] = i2c->DR;
	}

	return wait_stop(i2c);
}


uint8_t I2C_write(I2C_TypeDef *i2c,
		uint8_t reg,
		uint8_t adress,
		uint8_t value)
{
	if (i2c->SR2 & I2C_SR2_BUSY)
		return fail(i2c);

	i2c->CR1 |= I2C_CR1_START;
	if (!wait_sr1(i2c, I2C_SR1_SB))   return fail(i2c);
	i2c->DR = (adress << 1) | 0;
	if (!wait_sr1(i2c, I2C_SR1_ADDR)) return fail(i2c);
	(void)i2c->SR1;
	(void)i2c->SR2;

	i2c->DR = reg;
	if (!wait_sr1(i2c, I2C_SR1_TXE))  return fail(i2c);
	i2c->DR = value;
	if (!wait_sr1(i2c, I2C_SR1_BTF))  return fail(i2c);

	i2c->CR1 |= (I2C_CR1_STOP);
	return wait_stop(i2c);
}


static void i2c_delay(void)                  // ~10 us: half a clock period at ~50 kHz
{
	for (volatile int i = 0; i < 20; i++);
}

/* Bus recovery (I2C spec UM10204, "bus clear"):
 * A slave stopped in the middle of a byte keeps SDA low and waits for clocks.
 * We take the pins away from the I2C peripheral, send up to 9 clocks by hand
 * so the slave can finish its byte and let SDA go, then make a STOP,
 * give the pins back and reset the peripheral.
 * Pins must be the ones used by this I2C, alternate function 4.
 */
void I2C_recover(I2C_TypeDef *i2c, GPIO_TypeDef *port, uint32_t scl, uint32_t sda)
{
	i2c->CR1 &= ~I2C_CR1_PE;                  // peripheral off

	gpio_high(port, scl);                     // open drain high = released
	gpio_high(port, sda);
	gpio_init(port, scl, gpio_output, gpio_open_drain, 0);
	gpio_init(port, sda, gpio_output, gpio_open_drain, 0);
	i2c_delay();

	for (int i = 0; i < 9; i++) {             // 9 clocks
		gpio_low(port, scl);  i2c_delay();
		gpio_high(port, scl); i2c_delay();
	}

	// STOP: SDA goes low -> high while SCL is high
	gpio_low(port, scl);  i2c_delay();
	gpio_low(port, sda);  i2c_delay();
	gpio_high(port, scl); i2c_delay();
	gpio_high(port, sda); i2c_delay();

	gpio_init(port, scl, gpio_af, gpio_open_drain, 4);   // pins back to I2C
	gpio_init(port, sda, gpio_af, gpio_open_drain, 4);

	i2c->CR1 |=  I2C_CR1_SWRST;               // software reset: clears a stuck BUSY flag
	i2c->CR1 &= ~I2C_CR1_SWRST;
	I2C_init(i2c);                            // registers were cleared, set them again
}
