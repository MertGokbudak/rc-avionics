/* GPS driver (NEO-6M): reads $..RMC lines with the USART2 RX interrupt
 * and takes latitude, longitude, speed and course out of them.
 * Sentence format: NMEA 0183 (RMC).
 * Written with help from an AI assistant (Claude).
 */
/* GPS (NEO-6M): reads $..RMC lines with the USART2 RX interrupt,
 * takes position, speed and course out of them.
 */

#include "gps.h"
#include "stm32f4xx.h"
#include "gpio.h"
#include "uart.h"

int32_t gps_lat, gps_lon;
int16_t gps_speed;
int16_t gps_course;

static volatile uint8_t gps_ready;   // 1 = a full RMC line is waiting (set in the interrupt)
static char    gps_line[100];
static uint8_t gps_pos;


void gps_init(void)
{
	gpio_init(GPIOA, 3, gpio_af, gpio_push_pull, 7);     // USART2 RX
	uart_init(USART2, 16000000 / 9600);
	USART2->CR1 |= USART_CR1_RXNEIE;                     // interrupt on every byte
	NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_IRQHandler(void)             // runs for every byte from the GPS
{
	(void)USART2->SR;                    // read SR then DR: also clears overrun
	char c = USART2->DR;

	if (gps_ready)
		return;                          // main has not used the last line yet
	if (c == '$')
		gps_pos = 0;                     // a new line starts
	if (gps_pos < 99)
		gps_line[gps_pos++] = c;
	if (c == '\n' && gps_line[3] == 'R' && gps_line[4] == 'M' && gps_line[5] == 'C')
		gps_ready = 1;                   // a full $..RMC line
}

// n-th field of the line as a number, the dot skipped: "4504.21870" -> 450421870
// empty field -> 0
static int32_t gps_value(int n)
{
	int i = 0;
	while (n > 0 && i < 99) {            // walk to the n-th comma
		if (gps_line[i] == ',')
			n--;
		i++;
	}

	int32_t v = 0;
	while (gps_line[i] != ',' && i < 99) {   // read until the next comma
		if (gps_line[i] >= '0' && gps_line[i] <= '9')
			v = v * 10 + (gps_line[i] - '0');
		i++;
	}
	return v;
}

// $GPRMC,time,status,lat,N,lon,E,speed,course,date,...
void gps_update(void)
{
	if (gps_ready == 0)
		return;

	gps_lat    = gps_value(3);
	gps_lon    = gps_value(5);
	gps_speed  = gps_value(7) * 1852 / 100000;   // knots x1000 -> km/h x10
	gps_course = gps_value(8) / 10;              // degrees x100 -> x10

	gps_ready = 0;                       // the interrupt may collect the next line
}
