# RC Avionics

A remote-controlled aircraft flight system written from scratch in **bare-metal C** on two STM32 boards: a ground station that reads joysticks and talks to a PC, and an onboard computer that drives the servos and reads the sensors.

No HAL, no RTOS: every peripheral driver (GPIO, UART, ADC, I2C, SPI, PWM timers, LCD) is written directly against the registers using the CMSIS device headers.

## Goal

- Move the aircraft's servos from the ground with joysticks, over a radio link
- Send live telemetry back (attitude, altitude, GPS, link quality)
- Show the data on the ground station screen and on a PC

## Hardware

| Part | Role |
|---|---|
| STM32F446RE Nucleo-64 | Ground station |
| STM32F411 Black Pill | Onboard flight computer |
| 2× HC-12 (433 MHz UART radio) | Radio link (current) |
| 2× CC1101 (433 MHz) | Planned radio link |
| 2× analog joystick | Pilot input |
| 2.8" ILI9341 320×240 SPI display | Ground station screen |
| 2× SG90 servo | Control surfaces |
| MPU6050 | Accelerometer + gyroscope |
| BMP280 | Barometric altitude |
| NEO-6M | GPS |

## Repository layout

```
nucleo_avionics/        ground station (STM32CubeIDE project)
  Core/Inc, Core/Src    gpio, uart, adc, i2c, spi, timers, lcd, main
black_pill_avionics/    aircraft (STM32CubeIDE project)
  Core/Inc, Core/Src    gpio, uart, adc, i2c, spi, timers, main
```

The aircraft waits for `0xAA`, reads the next four bytes, joins them into two 12-bit ADC values and maps them to a servo pulse.

## Pinout

### Ground station — Nucleo F446RE

| Pin | Peripheral | Connected to |
|---|---|---|
| PA0 / PA1 | ADC1 ch0 / ch1 | Joystick 1 X / Y |
| PA4 / PB0 | ADC1 ch4 / ch8 | Joystick 2 X / Y |
| PB3 / PB5 | SPI1 SCK / MOSI (AF5) | Display |
| PB6 / PA9 / PB8 | GPIO | Display CS / DC / RESET |
| PC10 (CN7-1) | USART3 TX (AF7) | HC-12 RXD |
| PC11 (CN7-2) | USART3 RX (AF7) | HC-12 TXD |
| PA2 | USART2 TX | PC via ST-Link |

### Aircraft — Black Pill F411

| Pin | Peripheral | Connected to |
|---|---|---|
| PB4 / PB5 | TIM3 CH1 / CH2 (AF2) | Servo 1 / 2 |
| PA9 | USART1 TX (AF7) | HC-12 RXD |
| PA10 | USART1 RX (AF7) | HC-12 TXD |
| PB8 / PB9 | I2C1 SCL / SDA (AF4) | MPU6050 |
| PA3 | USART2 RX (AF7) | GPS TX |


