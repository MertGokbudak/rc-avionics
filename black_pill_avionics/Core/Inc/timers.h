#ifndef INC_TIMERS_H_
#define INC_TIMERS_H_

#include "stm32f4xx.h"

void pwm_init(TIM_TypeDef *timer,
		uint8_t chanel,
		uint32_t psc,
		uint32_t arr);

void pwm_set(TIM_TypeDef *timer, uint8_t chanel, uint16_t us);



#endif /* INC_TIMERS_H_ */
