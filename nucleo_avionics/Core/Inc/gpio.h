#ifndef INC_GPIO_H_
#define INC_GPIO_H_

#include "stm32f4xx.h"

typedef enum{
	gpio_input,
	gpio_output,
	gpio_af,
	gpio_analog,
} gpio_mode_t;

typedef enum{
	gpio_push_pull,
	gpio_open_drain,
} gpio_pp_od_t;


void gpio_init(GPIO_TypeDef *port,
		uint32_t pin,
		gpio_mode_t mode,
		gpio_pp_od_t pp_od,
		uint32_t af);

void gpio_low(GPIO_TypeDef *port, uint32_t pin);

void gpio_high(GPIO_TypeDef *port, uint32_t pin);

#endif /* INC_GPIO_H_ */
