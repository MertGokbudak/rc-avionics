#ifndef INC_ADC_H_
#define INC_ADC_H_

#include "stm32f4xx.h"

void adc_init(ADC_TypeDef *adc);

uint16_t adc_read(ADC_TypeDef *adc,
		uint8_t channel);

#endif /* INC_ADC_H_ */
