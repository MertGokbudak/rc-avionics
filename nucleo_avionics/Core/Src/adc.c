
#include "adc.h"
#include "stm32f4xx.h"

void adc_init(ADC_TypeDef *adc)
{
	if (adc == ADC1) RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
	if (adc == ADC2) RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;
	if (adc == ADC3) RCC->APB2ENR |= RCC_APB2ENR_ADC3EN;

	adc->CR2 |= ADC_CR2_ADON;
	for (volatile int i = 0; i < 1000; i++);
}

uint16_t adc_read(ADC_TypeDef *adc, uint8_t channel)
{
    adc->SQR3 &= ~(0x1FU);
    adc->SQR3 |= channel;

    adc->CR2 |= ADC_CR2_SWSTART;

	while (!(adc->SR & (ADC_SR_EOC)));

	return adc->DR;
}
