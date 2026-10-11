#include "gpio.h"


void gpio_init(GPIO_TypeDef *port,
		uint32_t pin,
		gpio_mode_t mode,
		gpio_pp_od_t pp_od,
		uint32_t af)
{

	if (port == GPIOA) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	if (port == GPIOB) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	if (port == GPIOC) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	port->OTYPER &= ~(1U << pin);
	port->OTYPER |= ((uint32_t)pp_od << pin);

	if (mode == gpio_af) {
		port->AFR[pin / 8] &= ~(0xFU << (pin % 8) * 4);
		port->AFR[pin / 8] |= ((uint32_t)af << (pin % 8) * 4);
	}
	port->MODER &= ~(0x3U << (pin * 2));
	port->MODER |= ((uint32_t)mode << (pin * 2));

}
