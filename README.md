# RC Avionics



https://github.com/user-attachments/assets/fbffd82b-b1e2-492d-a003-9c14ae281c42


<img width="440" alt="nucleo - Copy" src="https://github.com/user-attachments/assets/341cf253-f78a-4512-8c74-1143c5a6a59d" />

<img width="550" alt="blackpill - Copy" src="https://github.com/user-attachments/assets/832a784b-45a8-49b0-bd77-ac2ed5d4ab64" />


## Overview

Avionics for an RC plane, built from scratch. Two STM32 boards run bare-metal firmware: no HAL, every peripheral and device driver is written directly on the registers. Commands go up over an nRF24L01+ link at 2.4 GHz, telemetry comes down over a CC1101 link at 434 MHz. The remote forwards each telemetry frame over USB to a Raspberry Pi 5, which decodes it and serves a live web PFD (primary flight display) with a map.

## Video

Joystick input drives the servo on the plane through the 2.4 GHz command link. Tilting the plane box moves the artificial horizon on the iPad in real time: MPU6050 → Black Pill → CC1101 → Nucleo → USB → Raspberry Pi → browser. The remote LCD shows the same telemetry, and the map shows the GPS fix with reduced precision (privacy mode).

There is no cable between the plane and the remote. End-to-end latency is dominated by the 10 Hz telemetry rate. Only one servo is connected in the video; the second one broke.

## Remote and ground station

- **STM32F446 Nucleo:** 50 Hz control loop on a TIM5 microsecond timebase. Reads the joystick and the pot with ADC1, sends a 7-byte command over the nRF24 and checks its auto-ACK (TX_DS) to report command-link status.
- **Shared SPI1 bus (4 MHz)** for the ILI9341 LCD, the nRF24 and the CC1101, with a separate chip select for each.
- **2.8" ILI9341 LCD:** link status for both directions (CMD from nRF24 ACKs, TEL from a 0.5 s packet timeout), attitude, altitude, temperature, GPS (masked), speed and course. One line is redrawn per loop iteration, so drawing never blocks the radios.
- **USB to the Pi:** each telemetry packet is forwarded on USART2 through the ST-Link virtual COM port at 115200 baud, framed as `AA 55 | 20 bytes | XOR`.
- **Raspberry Pi 5:** a Python thread reads the serial port, resynchronizes on `AA 55`, verifies the checksum and unpacks the frame with `struct`. Flask serves the page and a JSON endpoint that the page polls at 10 Hz.

## Plane electronics

- **STM32F411 Black Pill:** flight computer, 10 Hz sensor and telemetry cycle.
- **MPU6050** (I2C1, 0x68): roll and pitch from the accelerometer (gyro fusion is the next step).
- **BMP280** (I2C1, 0x76): Bosch compensation formulas implemented from the datasheet; relative altitude against a ground reference taken after the IIR filter settles.
- **NEO-6M GPS** (USART2 RX interrupt): NMEA RMC parser for position, speed and course.
- **CC1101:** 434.4 MHz, GFSK 38.4 kbps, ±20.6 kHz deviation, fixed-length packets with hardware CRC.
- **nRF24L01+:** channel 80, hardware CRC and auto-ACK.
- **Servos** on TIM3 PWM (50 Hz, 1000–2000 µs), powered from a separate 5 V rail (see below).

*Prototype on perfboard. The next step is a custom carrier PCB for the Black Pill.*

## Problems I ran into

Most of the problems in this project were wiring problems: loose jumpers, long wires, shared power. This is why the next step is a PCB, especially for the Black Pill side.

**Servo current resetting the nRF24.** When a servo starts moving it draws a short current spike. On a shared supply this dropped the voltage enough to reset the nRF24, and the command link was lost. The fix:
- The servos take 5 V from a Seeed XIAO ESP32-S3, powered from its own USB port on the power bank, not from the Black Pill.
- The servo rail has 4 × 100 µF electrolytic capacitors in parallel (~400 µF) for the large dips, and a 100 nF ceramic for fast noise from the motor.
- All grounds are connected.

## Next steps

- **Custom PCB** for the Black Pill, the sensors and the power rails.
- **Wireless programming (planned, not started):** the XIAO ESP32-S3 on the plane, connected to the Black Pill's USART1 (PA9/PA10) and NRST, as a Wi-Fi bridge to flash and control the Black Pill. Today every update needs SWD cables to the Nucleo's ST-Link.
- **RF analysis with an RTL-SDR:** spectrum of both links, the ~50 kHz offset seen on the CC1101, signal strength against distance.
- **Gyro + accelerometer fusion** (complementary filter) for attitude.
- **A hardware watchdog** (IWDG).

## How I built it

My goal was to learn embedded systems by building a real project. I wrote the core of it myself, working from reference manuals, datasheets, user manuals, some YouTube videos and an AI assistant:

- **Peripheral drivers:** GPIO, SPI, I2C, UART, ADC and timers/PWM, directly on the registers.
- **Both main programs:** control loop, command and telemetry packets, failsafe, link status, screen layout, USB framing.
- **Hardware:** wiring, power, assembly, and all testing and debugging on the real boards.

Some parts were written with the help of an AI assistant (Claude): the BMP280, CC1101, nRF24, LCD and GPS drivers, the I2C timeouts and bus recovery, and the Raspberry Pi web page and server.

## Pin connections

**Nucleo F446RE (remote)**

| Function | Pin |
|---|---|
| SPI1 SCK / MISO / MOSI | PB3 / PB4 / PB5 (shared: LCD, nRF24, CC1101) |
| LCD CS / DC / RST | PB10 / PA9 / PA8 |
| nRF24 CSN / CE | PB12 / PC7 |
| CC1101 CSN | PA10 |
| Joystick X / Y | PA0 / PA1 (ADC1 ch0 / ch1) |
| Pot | PB0 (ADC1 ch8) |
| Button | PC2 (pull-up) |
| USART2 TX → ST-Link USB | PA2 |

**Black Pill F411CE (plane)**

| Function | Pin |
|---|---|
| SPI1 SCK / MISO / MOSI | PA5 / PA6 / PA7 (shared: nRF24, CC1101) |
| nRF24 CSN / CE | PA4 / PB0 |
| CC1101 CSN | PB12 |
| I2C1 SCL / SDA (MPU6050, BMP280) | PB8 / PB9 |
| GPS TX → USART2 RX | PA3 |
| Servo 1 / 2 (TIM3 CH1 / CH2) | PB4 / PB5 |
| LED | PC13 |

**Servo power rail**

| | |
|---|---|
| +5 V | XIAO ESP32-S3 5V pin → servo red wires, 4 × 100 µF + 100 nF |
| GND | XIAO GND, Black Pill GND, servo brown wires |
| Signal | servo orange wire → Black Pill PB4 / PB5 |
