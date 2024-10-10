#include <intrins.h>
#include "delay.h"
#include "cext.h"

#ifdef __OSCILLATO_6T__
// 时钟频率22118400
static void internal_delay_10us(void) 
{
  uint8_t i;

  _nop_();
  i = 6;
  while (--i);
}

static void internal_delay_ms(void)
{
  uint8_t i, j;

  _nop_();
  i = 4;
  j = 146;
  do
  {
    while (--j);
  } while (--i);
}

#else
// 时钟频率11059200
static void internal_delay_10us(void) 
{
  uint8_t i;

  i = 2;
  while (--i);
}

static void internal_delay_ms(void)
{
  uint8_t i, j;
  _nop_();
  i = 2;
  j = 199;
  do
  {
    while (--j);
  } while (--i);
}
#endif


void delay_10us(uint8_t t)
{
  while(t --) {
    internal_delay_10us();
  }
}

void delay_ms(uint8_t t) 
{     
  while(t--) {      
    internal_delay_ms();
  } 
}
