#ifndef __XMETER_ROM_H__
#define __XMETER_ROM_H__

#include <stdint.h>

#define ROM_XMETER_ADC_CURRENT_K 0
#define ROM_XMETER_ADC_CURRENT_B 4
#define ROM_XMETER_ADC_VOLTAGE_OUT_K 8
#define ROM_XMETER_ADC_VOLTAGE_OUT_B 12
#define ROM_XMETER_ADC_VOLTAGE_DISS_K 16
#define ROM_XMETER_ADC_VOLTAGE_DISS_B 20

#define ROM_XMETER_DAC_CURRENT_K 24
#define ROM_XMETER_DAC_CURRENT_B 28
#define ROM_XMETER_DAC_VOLTAGE_K 32
#define ROM_XMETER_DAC_VOLTAGE_B 36

#define ROM_XMETER_DAC_PRESET_CURRENT 60
#define ROM_XMETER_DAC_PRESET_VOLTAGE 84

#define ROM_XMETER_DAC_LAST_CURRENT 108
#define ROM_XMETER_DAC_LAST_VOLTAGE 114

#define ROM_XMETER_TEMP_HI          120
#define ROM_XMETER_TEMP_LO          126
#define ROM_XMETER_TEMP_OVERHEAT    132

#define ROM_XMETER_MAX_POWER_DISS   138

#define ROM_BEEPER_ENABLE 200

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
