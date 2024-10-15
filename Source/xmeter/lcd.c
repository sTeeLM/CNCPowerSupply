#include <stdint.H>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "lcd.h"
#include "delay.h"
#include "debug.h"
#include "gpio.h"

#define LCD_LINE_BUFFER_SIZE 48
#define LCD_LINE_BUFFER_SIZE_DEFAULT 16

static uint8_t lcd_line[2][LCD_LINE_BUFFER_SIZE];
static uint8_t lcd_line_len[2];
static uint8_t lcd_line_offset[2];

static uint8_t de_cnt;

/* 1 to blink */
static uint32_t lcd_line_blink_bits[2];

static bit lcd_line0_scroll;
static bit lcd_line1_scroll;

static uint8_t lcd_read_data(void) 
{
  LCD_DATA=0xff;
  
  LCD_E    = 0;
  LCD_RS   = 0;
  LCD_RW   = 1;
  
  delay_10us(1);
  LCD_E    = 1;
  return LCD_DATA;
}

static bit lcd_check_busy()
{
  /*
  LCD_DATA=0xff;
  LCD_RS=0;
  LCD_RW=1;
  LCD_E=0;
  delay_10us(1);
  LCD_E=1;
  return (bit)(LCD_DATA & 0x80);
  */
  return lcd_read_data() & 0x80;
}

static void lcd_write_cmd(uint8_t val)
{
  while(lcd_check_busy());
  
  LCD_E    = 1;
  LCD_RS   = 0;
  LCD_RW   = 0;
  
  LCD_DATA = val;
  
  delay_10us(1);
  
  LCD_E    = 0;
}

static void lcd_write_data(uint8_t val)
{
  while(lcd_check_busy());
  
  LCD_E    = 1;
  LCD_RS   = 1;
  LCD_RW   = 0;
  
  LCD_DATA = val;
  
  delay_10us(1);
  
  LCD_E    = 0;
}



static const uint8_t code lcd_special_char[8][8] = 
{
  {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00}, // up   arraw
  {0x00, 0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04}, // down arraw
  {0x07, 0x05, 0x07, 0x1c, 0x10, 0x10, 0x10, 0x1c}, // 
  {0x0f, 0x09, 0x0f, 0x09, 0x0f, 0x09, 0x13, 0x00},
  {0x0f, 0x09, 0x0f, 0x09, 0x0f, 0x09, 0x13, 0x00},
  {0x0f, 0x09, 0x0f, 0x09, 0x0f, 0x09, 0x13, 0x00},
  {0x0f, 0x09, 0x0f, 0x09, 0x0f, 0x09, 0x13, 0x00},
  {0x0f, 0x09, 0x0f, 0x09, 0x0f, 0x09, 0x13, 0x00},    
};

/* 至多8个5X8的特殊字符 */
static void lcd_init_special_char()
{
  uint8_t i,j;
  lcd_write_cmd(0x40);
  for(i = 0 ; i < 8; i ++) {
    for(j = 0 ; j < 8; j ++) {
      lcd_write_data(lcd_special_char[i][j]);
    }
  }
}

static void lcd_initialize_internal()
{
  LCD_DATA = 0xFF;
  delay_ms(20);
  lcd_write_cmd(0x38); /* 8位，两行显示，5x7 */ 
  lcd_write_cmd(0x01); /* 清除屏幕 */
  lcd_write_cmd(0x06); /* 光标右移动，不移位 */
  lcd_write_cmd(0x0c); /* 开显示，关光标 */  
  
  lcd_init_special_char();
  
  lcd_clear();
}

void lcd_initialize(void)
{
  CDBG("lcd_initialize\n");
  lcd_initialize_internal();
  
  lcd_refresh();
}

void lcd_clear(void)
{
  lcd_line_len[0] = LCD_LINE_BUFFER_SIZE_DEFAULT;
  lcd_line_len[1] = LCD_LINE_BUFFER_SIZE_DEFAULT;
  
  memset(lcd_line[0], ' ', lcd_line_len[0]);
  memset(lcd_line[1], ' ', lcd_line_len[1]);
  
  lcd_line_offset[0] = lcd_line_offset[1] = 0;
  
  lcd_line_blink_bits[0] = lcd_line_blink_bits[1] = 0;
  
  lcd_line0_scroll = lcd_line1_scroll = 0;
  
  lcd_write_cmd(0x01);
}

void lcd_refresh(void) // every 250ms
{
  int i, index;
  
  de_cnt = (de_cnt + 1) % 4;
  
  // row 0
  lcd_write_cmd(0x80);
  for(i = 0 ; i < lcd_line_len[0] ; i ++) {
    index = (i + lcd_line_offset[0]) % lcd_line_len[0];
    if( lcd_line_blink_bits[0] & (1 << index) ) {
      if(!de_cnt) {
        lcd_write_data(lcd_line[0][index]);
      } else {
        lcd_write_data(' ');
      }
    } else {
      lcd_write_data(lcd_line[0][index]);
    }
  }
  // row 1
  lcd_write_cmd(0xC0);
  for(i = 0 ; i < lcd_line_len[1] ; i ++) {
    index = (i + lcd_line_offset[1]) % lcd_line_len[1];
    if( lcd_line_blink_bits[1] & (1 << index) ) {
      if(!de_cnt) {
        lcd_write_data(lcd_line[1][index]);
      } else {
        lcd_write_data(' ');
      }
    } else {
      lcd_write_data(lcd_line[1][index]);
    }
  } 
  
  if(lcd_line0_scroll && (de_cnt % 2) == 0) {
    lcd_line_offset[0] = (lcd_line_offset[0] + 1) % lcd_line_len[0];
  }
  
  if(lcd_line1_scroll && (de_cnt % 2) == 0) {
    lcd_line_offset[1] = (lcd_line_offset[1] + 1) % lcd_line_len[1];
  }  
}

void lcd_set_special_char(uint8_t row, uint8_t col, uint8_t c)
{
  row %= 2;
  c %= LCD_SPECIAL_CNT;
  lcd_line[row][col % lcd_line_len[row]] = c;
}

void lcd_set_char(uint8_t row, uint8_t col, char c)
{
  row %= 2;
  lcd_line[row][col % lcd_line_len[row]] = c;
}

void lcd_set_string(uint8_t row, uint8_t col,  const char * str)
{
  char * p = str;
  row %= 2;
  while(*p != 0) {
    lcd_set_char(row, col, *p);
    col ++;
    p ++;
  }
  
}

void lcd_set_integer(uint8_t row, uint8_t col, uint8_t len, uint32_t val)
{
  while(len) {
    lcd_set_char(row, col + len - 1, (val % 10) + 0x30);
    len --;
    val = (val / 10);
  }
}

/* convert float number to  5 len string, 4 digit and 1 point
0~  9.999 :               0.000       9.999
10~ 99.99 :              10.00       99.99
100~999.9 :              100.0       999.9

integer: 1 ~ 999
decimal: 0 ~ 999 (decimal / 1000 is real decimal)
*/
void lcd_set_digit(uint8_t row, uint8_t col, uint16_t integer, uint16_t decimal)
{
  char buf[6] = {' '};
  
  row %= 2;
  buf[5] = 0;
  
  if(decimal > 999 || integer > 999) {
    buf[0] = 'E';
    buf[1] = 'R'; 
    buf[2] = 'R';
    buf[3] = ' ';
    buf[4] = ' ';     
  } else if(integer >= 0 && integer <= 9) {
    buf[0] = integer + 0x30;
    buf[1] = '.';
    buf[2] = (decimal / 100) + 0x30;
    decimal %= 100;
    buf[3] = (decimal / 10) + 0x30;
    decimal %= 10;
    buf[4] = decimal + 0x30;
  } else if (integer >= 10 && integer <= 99) {
    buf[0] = (integer / 10) + 0x30;
    integer %= 10;
    buf[1] = integer + 0x30;
    buf[2] = '.';
    buf[3] = (decimal / 100) + 0x30;
    decimal %= 100;
    buf[4] = (decimal / 10) + 0x30;
  } else if (integer >= 100 && integer <= 999) {
    buf[0] = (integer / 100) + 0x30;
    integer %= 100;
    buf[1] = (integer / 10) + 0x30;
    integer %= 10;
    buf[2] = integer + 0x30;
    buf[3] = '.';
    buf[4] = (decimal / 100) + 0x30;
  }
  
  lcd_set_string(row, col, buf);
}

/*
0->0x30
9->0x39
A->0x41
F->0x46
*/
void lcd_set_hex(uint8_t row, uint8_t col, uint16_t hex)
{
  char buf[5] = {' '};
  uint8_t c, i;
  row %= 2;
  col %= lcd_line_len[row];
  buf[4] = 0;
  
  for(i = 0; i < 4; i ++) {
    c = hex & 0xF;
    hex = hex >> 4;
    if(c <= 9) {
      c += 0x30;
    } else if(c >= 10) {
      c+= 0x37;
    }
    buf[3 - i] = c;
  }
  
  lcd_set_string(row, col, buf);
}

uint8_t lcd_set_buffer_len(uint8_t row, uint8_t len)
{
  uint8_t tmp;
  if(len > LCD_LINE_BUFFER_SIZE)
    len = LCD_LINE_BUFFER_SIZE;

  tmp = lcd_line_len[row];
  lcd_line_len[row] = len;
  
  memset(lcd_line[row], ' ', lcd_line_len[row]);
  
  return tmp;
}

bit lcd_set_scroll(uint8_t row, bit enable)
{
  bit tmp;
  
  row %= 2;
  
  if(row) {
    tmp = lcd_line1_scroll;
    lcd_line1_scroll = enable;
  } else {
    tmp = lcd_line0_scroll;
    lcd_line0_scroll = enable;
  }
  return tmp;
}

bit lcd_set_blink(uint8_t row, uint8_t col, bit blink)
{
  bit tmp; 
  row %= 2;
  col %= lcd_line_len[row];
  
  tmp = (lcd_line_blink_bits[row] & (1 << col)) != 0;
  
  lcd_line_blink_bits[row] &= ~(1 << col);
  
  if(blink)
    lcd_line_blink_bits[row] |= (1 << col);
  
  return tmp;
}

void lcd_set_blink_range(uint8_t row, uint8_t from, uint8_t to, bit blink)
{
  int col;
  for(col = from ; col <= to; col ++) {
    lcd_set_blink(row, col, blink);
  }
}
