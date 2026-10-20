# RC Avionics

A remote-controlled aircraft flight system written from scratch in **bare-metal C** on two STM32 boards: a ground station that reads joysticks and shows telemetry, and an onboard computer that drives the servos and reads the sensors.

No HAL, no RTOS: every peripheral driver (GPIO, UART, ADC, I2C, SPI, PWM timers, LCD) is written directly against the registers using the CMSIS device headers.

## Goal

- Move the aircraft's servos from the ground with joysticks, over a radio link
- Send live telemetry back (attitude, altitude, GPS)
- Show the data on the ground station screen and on a PC

## Current status

- ✅ Joystick → nRF24L01+ → servo control link working
- ✅ Telemetry over HC-12: MPU6050 raw accel/gyro, GPS position, BMP280 temperature and altitude
- ✅ Ground station display shows joystick and telemetry values

## Hardware

| Part | Role |
|---|---|
| STM32F446RE Nucleo-64 | Ground station |
| STM32F411 Black Pill | Onboard flight computer |
| 2× nRF24L01+ PA+LNA (2.4 GHz, on 5 V adapter boards) | Control link (ground → aircraft) |
| 2× HC-12 (433 MHz UART radio) | Telemetry link (aircraft → ground) |
| Analog joystick | Pilot input |
| 2.8" ILI9341 320×240 SPI display | Ground station screen |
| 2× SG90 servo | Control surfaces |
| MPU6050 | Accelerometer + gyroscope |
| BMP280 | Temperature + barometric altitude |
| NEO-6M | GPS |

## How it works

**Control (nRF24, ground → aircraft).** The Nucleo reads the joystick ADC and sends a 4-byte packet: X and Y as raw 12-bit values, `uint16`, low byte first. The nRF24 handles CRC, auto-ACK and retransmission in hardware. The Black Pill maps each value to a 1000–2000 µs servo pulse (TIM3, 50 Hz).

**Telemetry (HC-12, aircraft → ground).** Every 100 ms the Black Pill sends a 39-byte packet at 19200 baud:

| Bytes | Content |
|---|---|
| 0 | Start byte `0x55` |
| 1–12 | AX, AY, AZ, GX, GY, GZ — `int16`, high byte first (raw MPU6050) |
| 13–22 | Latitude, NMEA text (`ddmm.mmmmm`) |
| 23–33 | Longitude, NMEA text (`dddmm.mmmmm`) |
| 34–35 | Temperature, 0.01 °C |
| 36–37 | Relative altitude, 0.1 m |
| 38 | Checksum: 8-bit sum of bytes 0–37 |

**Ground station display.** To keep the control link responsive, the screen is redrawn one value per main-loop pass instead of all at once.

## Repository layout

```
nucleo_avionics/        ground station (STM32CubeIDE project)
  Core/Inc, Core/Src    gpio, uart, adc, spi, timers, lcd, main
black_pill_avionics/    aircraft (STM32CubeIDE project)
  Core/Inc, Core/Src    gpio, uart, adc, i2c, spi, timers, bmp280, main
```

## Pinout

### Ground station — Nucleo F446RE

| Pin | Peripheral | Connected to |
|---|---|---|
| PA0 / PA1 | ADC1 ch0 / ch1 | Joystick X / Y |
| PB3 / PB5 | SPI1 SCK / MOSI (AF5) | Display + nRF24 (shared bus) |
| PB4 | SPI1 MISO (AF5) | nRF24 only (display SDO not connected) |
| PB6 / PA9 / PB8 | GPIO | Display CS / DC / RESET |
| PB12 | GPIO | nRF24 CSN |
| PC7 | GPIO | nRF24 CE |
| PC10 / PC11 | USART3 TX / RX (AF7) | HC-12 RXD / TXD |
| PA2 | USART2 TX | PC via ST-Link |

### Aircraft — Black Pill F411

| Pin | Peripheral | Connected to |
|---|---|---|
| PA5 / PA6 / PA7 | SPI1 SCK / MISO / MOSI (AF5) | nRF24 |
| PA4 | GPIO | nRF24 CSN |
| PB0 | GPIO | nRF24 CE |
| PB4 / PB5 | TIM3 CH1 / CH2 (AF2) | Servo 1 / 2 |
| PA9 / PA10 | USART1 TX / RX (AF7) | HC-12 RXD / TXD |
| PB8 / PB9 | I2C1 SCL / SDA (AF4) | MPU6050 (0x68), BMP280 (0x76) |
| PA3 | USART2 RX (AF7) | GPS TX |
