#ifndef INC_PINS_H_
#define INC_PINS_H_

#include "stm32f4xx.h"

// Nucleo F446 - remote

// shared SPI1: PB3 SCK, PB4 MISO, PB5 MOSI

// nRF24
#define NRF_SPI       SPI1
#define NRF_CSN_PORT  GPIOB
#define NRF_CSN_PIN   12
#define NRF_CE_PORT   GPIOC
#define NRF_CE_PIN    7

// CC1101
#define CC_SPI        SPI1
#define CC_CSN_PORT   GPIOA
#define CC_CSN_PIN    10

#endif /* INC_PINS_H_ */
