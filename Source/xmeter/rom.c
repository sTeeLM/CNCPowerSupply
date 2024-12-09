#include <stdint.H>

#include "rom.h"
#include "debug.h"
#include "i2c.h"
#include "delay.h"
#include "gpio.h"
#include "xmeter.h"
#include "lcd.h"

#define ROM_I2C_ADDR 0xA0

/*
#define ROM_1K  // AT24C01
#define ROM_2K  // AT24C02
#define ROM_4K  // AT24C04
#define ROM_8K  // AT24C08
#define ROM_16K  // AT24C16
*/

#define ROM_4K

#if defined ( ROM_1K ) || (defined ( ROM_2K ))
  #define PADDR_MASK 0x0
#elif defined (ROM_4K)
  #define PADDR_MASK 0x1
#elif defined (ROM_8K)
  #define PADDR_MASK 0x3
#elif defined (ROM_16K)
  #define PADDR_MASK 0x7
#endif

void rom_read_struct(uint16_t addr, void * pval, uint8_t len)
{
  uint8_t * p = (uint8_t *) pval;
  uint8_t index = 0;
  
  CDBG("rom_read_struct size = %bu\n", len);
  
  for(index = 0 ; index < len; index ++) {
    *p = rom_read(addr + index);
    p ++;
  }
}

void rom_write_struct(uint16_t addr, void * pval, uint8_t len)
{
  uint8_t * p = (uint8_t *) pval;
  uint8_t index = 0, val;
  
  CDBG("rom_write_struct size = %bu\n", len);
  
  for(index = 0 ; index < len; index ++) {
    val = *p;
    rom_write(addr + index, val);
    p ++;
  }
}

void rom_read32(uint16_t addr, void * pval)
{
  rom_read_struct(addr, pval, 4);    
}

void rom_write32(uint16_t addr, void * pval)
{
  rom_write_struct(addr, pval, 4);        
}


void rom_read16(uint16_t addr, void * pval)
{
  rom_read_struct(addr, pval, 2);
}

void rom_write16(uint16_t addr, void * pval)
{
  rom_write_struct(addr, pval, 2);
}

uint8_t rom_read(uint16_t addr)
{
  uint8_t dat;
  uint8_t baddr, paddr;
  
  baddr = addr & 0xff;
  paddr = (addr >> 8) & PADDR_MASK;
  
  I2C_Init();
  I2C_Get(ROM_I2C_ADDR | (paddr << 1), baddr, &dat);
  CDBG("rom_read [0x%02bx][0x%02bx] return 0x%02bx\n", paddr, baddr, dat);
  return dat;
}

void rom_write(uint16_t addr, uint8_t val)
{
  uint8_t baddr, paddr;
  
  baddr = addr & 0xff;
  paddr = (addr >> 8) & PADDR_MASK;
  
  I2C_Init();
  I2C_Put(ROM_I2C_ADDR | (paddr << 1), addr, val);
  CDBG("rom_write [0x%02bx][0x%02bx] with 0x%02bx\n", paddr, addr, val);
}

static void rom_reset(void)
{
  xmeter_rom_factory_reset();
  
  rom_write(ROM_BEEPER_ENABLE, 1);
}

bit rom_is_factory_reset(void)
{
  return ROM_RESET == 0;
}

void rom_initialize(void)
{
  CDBG("rom_initialize ROM_RESET = %bu(%s)\n", ROM_RESET == 1 ? 1 : 0, ROM_RESET == 1? "OFF" : "ON");
  if(rom_is_factory_reset()) { // reset rom
    CDBG("reset rom!\n");
    lcd_set_string(0, 0, "   RESETING...  ");
   
    rom_reset();
  }
}
