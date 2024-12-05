#ifndef __XMETER_LCD_H__
#define __XMETER_LCD_H__

#include <stdint.h>

enum lcd_special_chars
{
  LCD_UP_ARROW,
  LCD_DOWN_ARROW,
  LCD_CELSIUS,
  LCD_E3,
  LCD_E4,
  LCD_E5,  
  LCD_E6,
  LCD_E7,  
  LCD_SPECIAL_CNT,    
};

void lcd_initialize(void);
void lcd_clear(void);
void lcd_refresh(void);

void lcd_set_string(uint8_t row, uint8_t col,  const char * str);

// if buffer_len > 16, scroll from left to right every 1s when display 
uint8_t lcd_set_buffer_len(uint8_t row, uint8_t len);

void lcd_set_char(uint8_t row, uint8_t col, char c);

void lcd_set_digit(uint8_t row, uint8_t col, uint16_t integer, uint16_t decimal);

void lcd_set_hex(uint8_t row, uint8_t col, uint16_t hex);

void lcd_set_integer(uint8_t row, uint8_t col, uint8_t len, uint32_t val);

bit lcd_set_scroll(uint8_t row, bit enable);
bit lcd_set_blink(uint8_t row, uint8_t col, bit blink);
void lcd_set_blink_range(uint8_t row, uint8_t from, uint8_t to, bit blink);

void lcd_set_special_char(uint8_t row, uint8_t col, uint8_t c);

void lcd_show_progress(uint8_t row, uint8_t progress);

void lcd_enter_shell();
void lcd_leave_shell();

void lcd_enter_control();
void lcd_leave_control();

#endif