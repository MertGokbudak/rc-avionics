#ifndef INC_CC1101_H_
#define INC_CC1101_H_

#include <stdint.h>

void    cc_csn_init(void);                          // before any SPI traffic
void    cc_init(uint8_t len);                       // leaves the chip in IDLE
void    cc_listen(void);                            // remote: start listening
uint8_t cc_receive(uint8_t *buf, uint8_t len);      // remote: 1 = good packet in buf
uint8_t cc_send(uint8_t *buf, uint8_t len);         // plane: 1 = started, 0 = busy
uint8_t cc_read(uint8_t reg);                       // read a config register

extern uint8_t cc_version;

#endif /* INC_CC1101_H_ */
