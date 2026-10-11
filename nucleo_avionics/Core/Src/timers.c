#include "timers.h"

void pwm_init(TIM_TypeDef *timer,
		uint8_t chanel,
		uint32_t psc,
		uint32_t arr)

{
	if (timer == TIM2) RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
	if (timer == TIM3) RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	if (timer == TIM4) RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
	if (timer == TIM5) RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

	timer->CR1 &= ~(1U << 7);
	timer->CR1 |= TIM_CR1_ARPE;


	if (chanel == 1)      { timer->CCMR1 |= (6U << 4)  | (1U << 3); }
	else if (chanel == 2) { timer->CCMR1 |= (6U << 12) | (1U << 11); }
	else if (chanel == 3) { timer->CCMR2 |= (6U << 4)  | (1U << 3); }
	else if (chanel == 4) { timer->CCMR2 |= (6U << 12) | (1U << 11); }

	timer->CCER |= (1U << ((chanel -1)*4));

	timer->PSC = psc;
	timer->ARR = arr;

	timer->EGR |= TIM_EGR_UG;
	timer->CR1 |= TIM_CR1_CEN;
}

void pwm_set(TIM_TypeDef *timer,
		uint8_t chanel,
		uint16_t us)

{
	if (chanel == 1)      { timer->CCR1 = us; }
		else if (chanel == 2) { timer->CCR2 = us; }
		else if (chanel == 3) { timer->CCR3 = us; }
		else if (chanel == 4) { timer->CCR4 = us; }

}
