#ifndef INC_LCD_H_
#define INC_LCD_H_

#include <stdint.h>

// Colors (RGB565)
#define BLACK  0x0000
#define WHITE  0xFFFF
#define RED    0xF800
#define CYAN  0x07FF

void lcd_init(void);
void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void lcd_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t size);
void lcd_number(uint16_t x, uint16_t y, int32_t n, uint8_t width,
                uint16_t fg, uint16_t bg, uint8_t size);


#endif /* INC_LCD_H_ */
