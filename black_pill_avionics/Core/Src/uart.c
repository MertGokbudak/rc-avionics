#include "uart.h"

void uart_init(USART_TypeDef *uart, uint32_t brr){

	if (uart == USART1) RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
	if (uart == USART6) RCC->APB2ENR |= RCC_APB2ENR_USART6EN;

	if (uart == USART2) RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

	uart->BRR = brr;
	uart->CR1 |= USART_CR1_TE;
	uart->CR1 |= USART_CR1_RE;
	uart->CR1 |= USART_CR1_UE;
}


void uart_transmit(USART_TypeDef *uart, uint8_t control_signal){

	while (!(uart->SR & (1U << 7)));
	uart->DR = control_signal;
}

uint8_t uart_receive(USART_TypeDef *uart){
	while (!(uart->SR & (1U << 5)));

	return uart->DR;
}

uint8_t uart_available(USART_TypeDef *uart)
{
    if (uart->SR & USART_SR_RXNE)
        return 1;
    return 0;
}


