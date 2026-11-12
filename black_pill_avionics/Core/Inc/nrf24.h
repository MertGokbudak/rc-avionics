#ifndef INC_NRF24_H_
#define INC_NRF24_H_

#include <stdint.h>

void    nrf_csn_init(void);                         // before any SPI traffic
void    nrf_init(void);                             // TX (remote)
void    nrf_init_rx(uint8_t len);                   // RX (plane)
void    nrf_send(uint8_t *buf, uint8_t len);
uint8_t nrf_receive(uint8_t *buf, uint8_t len);     // 1 = new packet in buf

extern uint8_t nrf_ch;                              // debug: must read 80

#endif /* INC_NRF24_H_ */
