#ifndef INC_LCD_H_
#define INC_LCD_H_

#include <stdint.h>

// Colors (RGB565)
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define CYAN    0x07FF
#define YELLOW  0xFFE0
#define ORANGE  0xFD20
#define MAGENTA 0xF81F
#define GRAY    0x8410
#define DKGREEN 0x03E0

void lcd_init(void);
void lcd_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void lcd_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void lcd_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t size);
void lcd_number(uint16_t x, uint16_t y, int32_t n, uint8_t width,
                uint16_t fg, uint16_t bg, uint8_t size);


#endif /* INC_LCD_H_ */
