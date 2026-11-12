# RC Avionics

Bare-metal STM32 RC plane. Nucleo F446RE = remote, Black Pill F411CE = plane.

## Nucleo F446RE (remote)

| | Pin |
|---|---|
| SPI1 SCK / MISO / MOSI | PB3 / PB4 / PB5 |
| LCD CS / DC / RST | PB10 / PA9 / PA8 |
| nRF24 CSN / CE | PB12 / PC7 |
| CC1101 CSN | PA10 |
| Joystick X / Y | PA0 / PA1 |
| Pot | PB0 |
| Button | PC2 |

## Black Pill F411CE (plane)

| | Pin |
|---|---|
| SPI1 SCK / MISO / MOSI | PA5 / PA6 / PA7 |
| nRF24 CSN / CE | PA4 / PB0 |
| CC1101 CSN | PB12 |
| I2C1 SCL / SDA (MPU6050, BMP280) | PB8 / PB9 |
| GPS RX (USART2) | PA3 |
| Servo 1 / 2 (TIM3) | PB4 / PB5 |
| LED | PC13 |