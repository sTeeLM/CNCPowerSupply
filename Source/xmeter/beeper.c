#include "clock.h"
#include "beeper.h"
#include "debug.h"
#include "cext.h"
#include "delay.h"
#include "rom.h"
#include "gpio.h"

static bit beep_enable;

void beeper_initialize (void)
{
  CDBG(("beeper_initialize\n"));
  
  beep_enable = rom_read(ROM_BEEPER_ENABLE);
  
  beeper_beep_beep_always();
  
  BEEPER_OUT  = 1;
}


bit beeper_get_beep_enable(void)
{
  return beep_enable;
}

void beeper_set_beep_enable(bit enable)
{
  beep_enable = enable;
}

void beeper_write_rom_beeper_enable(void)
{
  CDBG(("beeper_write_rom_beeper_enable\n"));
  rom_write(ROM_BEEPER_ENABLE, beeper_get_beep_enable());
}

void beeper_beep(void)
{ 
  unsigned char c = 30;
  CDBG("beeper_beep!\n");
  if(!beep_enable)
    return;
  BEEPER_OUT = 1;
  while(c --) {
    BEEPER_OUT=~BEEPER_OUT;
    delay_10us(2 * 0x13); 
  }
  BEEPER_OUT = 1;
}

void beeper_beep_beep_always(void)
{
  unsigned char c = 30;
  CDBG("beeper_beep_beep_always!\n");
  BEEPER_OUT = 1;
  while(c --) {
    BEEPER_OUT=~BEEPER_OUT;
    delay_10us(2 * 0x10); 
  }
  
  delay_ms(100);
  
  while(c --) {
    BEEPER_OUT=~BEEPER_OUT;
    delay_10us(2 * 0x10); 
  }
  BEEPER_OUT = 1;
}

void beeper_beep_beep(void)
{
  unsigned char c = 30;
  CDBG("beeper_beep_beep!\n");
  if(!beep_enable)
    return;
  BEEPER_OUT =0;
  while(c --) {
    BEEPER_OUT=~BEEPER_OUT;
    delay_10us(2 * 0x10); 
  }
  
  delay_ms(100);
  
  while(c --) {
    BEEPER_OUT=~BEEPER_OUT;
    delay_10us(2 * 0x10); 
  }
  BEEPER_OUT = 1;
}
