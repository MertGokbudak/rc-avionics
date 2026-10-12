#ifndef INC_UART_H_
#define INC_UART_H_


#include "stm32f4xx.h"

void uart_init(USART_TypeDef *uart,
		uint32_t brr);

void uart_transmit(USART_TypeDef *uart,
		uint8_t control_signal);

uint8_t uart_receive(USART_TypeDef *uart);



#endif /* INC_UART_H_ */
