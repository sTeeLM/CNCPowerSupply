#ifndef __XMETER_ROM_H__
#define __XMETER_ROM_H__

#include <stdint.h>

#define ROM_XMETER_ADC_CURRENT_G 0
#define ROM_XMETER_ADC_VOLTAGE_OUT_G 36
#define ROM_XMETER_ADC_VOLTAGE_DISS_G 72
#define ROM_XMETER_DAC_CURRENT_G 108
#define ROM_XMETER_DAC_VOLTAGE_G 144

#define ROM_XMETER_DAC_PRESET_CURRENT 180
#define ROM_XMETER_DAC_PRESET_VOLTAGE 204

#define ROM_XMETER_DAC_LAST_CURRENT 228
#define ROM_XMETER_DAC_LAST_VOLTAGE 234

#define ROM_XMETER_TEMP_HI          240
#define ROM_XMETER_TEMP_LO          246
#define ROM_XMETER_TEMP_OVERHEAT    252

#define ROM_XMETER_MAX_POWER_DISS   258

#define ROM_BEEPER_ENABLE 264

uint8_t rom_read(uint8_t addr);
void rom_write(uint8_t addr, uint8_t val);

void rom_read16(uint8_t addr, void * val);
void rom_write16(uint8_t addr, void * val);

void rom_read32(uint8_t addr, void * val);
void rom_write32(uint8_t addr, void * val);

void rom_read_struct(uint8_t addr, void * pval, uint8_t len);
void rom_write_struct(uint8_t addr, void * pval, uint8_t len);

void rom_initialize(void);
bit rom_is_factory_reset(void);

#endif
