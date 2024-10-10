#ifndef __XMETER_XMETER_H__
#define __XMETER_XMETER_H__

#include "stdint.h"
#include "protocol.h"


/*
formated adc reading
*/
extern xmeter_value_t xmeter_adc_current;   // 输出电流
extern xmeter_value_t xmeter_adc_voltage_out;   // 输出电压
extern xmeter_value_t xmeter_adc_voltage_in;   // 输入电压
extern xmeter_value_t xmeter_power_out; // 输出功率
extern xmeter_value_t xmeter_power_diss; // 耗散功率
extern xmeter_value_t xmeter_adc_temp;      // 温度

extern xmeter_value_t xmeter_temp_hi;
extern xmeter_value_t xmeter_temp_lo;
extern xmeter_value_t xmeter_temp_overheat;
extern xmeter_value_t xmeter_max_power_diss;

void xmeter_rom_factory_reset(void);

void xmeter_initialize(void);
void xmeter_load_config(void);

extern xmeter_value_t xmeter_dac_voltage;
extern xmeter_value_t xmeter_dac_current;

/* read adc, and update  xmeter_adc_*** */
void xmeter_read_adc(void);

bit xmeter_output_on(void);
bit xmeter_output_off(void);
bit xmeter_output_status(void);
bit xmeter_fan_on(void);
bit xmeter_fan_off(void);
bit xmeter_fan_status(void);
bit xmeter_cc_status(void);

/* directlly read adc bits */
uint16_t xmeter_get_adc_bits_voltage_in(void);
uint16_t xmeter_get_adc_bits_voltage_out(void);
uint16_t xmeter_get_adc_bits_current(void);

/*
  read dac, and update xmeter_dac_**
*/
void xmeter_read_dac_voltage(void);
void xmeter_read_dac_current(void);

/* directlly read / change dac bits */
uint16_t xmeter_get_dac_bits_v(void);
uint16_t xmeter_get_dac_bits_c(void);
void xmeter_inc_dac_bits_v(bit coarse);
void xmeter_dec_dac_bits_v(bit coarse);
void xmeter_inc_dac_bits_c(bit coarse);
void xmeter_dec_dac_bits_c(bit coarse);
void xmeter_set_dac_bits_v(uint16_t bits);
void xmeter_set_dac_bits_c(uint16_t bits);

/* write xmeter_dac_** to DAC */
void xmeter_write_dac_voltage(void);
void xmeter_write_dac_current(void);

/* save config */
void xmeter_write_rom_dac_voltage(void);
void xmeter_write_rom_dac_current(void);

/* chang xmeter_dac_**, not change DAC */
void xmeter_inc_dac_v(bit coarse);
void xmeter_dec_dac_v(bit coarse);
void xmeter_inc_dac_c(bit coarse);
void xmeter_dec_dac_c(bit coarse);
void xmeter_set_dac_v(const xmeter_value_t * val);
void xmeter_set_dac_c(const xmeter_value_t * val);

/* load xmeter_dac_ from preset slot, not change DAC */
void xmeter_load_preset_dac_v(void);
void xmeter_load_preset_dac_c(void);

void xmeter_load_preset_dac_v_by_index(uint8_t index);
void xmeter_load_preset_dac_c_by_index(uint8_t index);

/* load xmeter_dac_ with preset slot, and update preset slot index 
   not change DAC
*/
void xmeter_next_preset_dac_v(void);
void xmeter_prev_preset_dac_v(void);
void xmeter_next_preset_dac_c(void);
void xmeter_prev_preset_dac_c(void);

/* index things */
uint8_t xmeter_get_preset_index_c(void);
uint8_t xmeter_get_preset_index_v(void);
void xmeter_reset_preset_index_c(void);
void xmeter_reset_preset_index_v(void);
uint8_t xmeter_get_max_preset_index_c(void);
uint8_t xmeter_get_max_preset_index_v(void);
/* change preset config 
   not change DAC
*/
void xmeter_inc_preset_dac_v(bit coarse);
void xmeter_dec_preset_dac_v(bit coarse);
void xmeter_inc_preset_dac_c(bit coarse);
void xmeter_dec_preset_dac_c(bit coarse);
void xmeter_store_preset_dac_c(uint8_t index);
void xmeter_store_preset_dac_v(uint8_t index);
void xmeter_write_rom_preset_dac_v(void);
void xmeter_write_rom_preset_dac_c(void);

/* change temp hi and lo config*/
void xmeter_inc_temp_hi(void);
void xmeter_inc_temp_lo(void);
void xmeter_dec_temp_hi(void);
void xmeter_dec_temp_lo(void);
void xmeter_set_temp_hi(const xmeter_value_t * val);
void xmeter_set_temp_lo(const xmeter_value_t * val);
void xmeter_write_rom_temp_lo();
void xmeter_write_rom_temp_hi();


/* for eyes only, will not change xmeter states */
void xmeter_calculate_power_out(
  const xmeter_value_t * current, 
  const xmeter_value_t * voltage,
  xmeter_value_t * power_out);

/* is temp below temp_lo ? */
bit xmeter_temp_safe(void);  
  
/* change power diss limit config */  
void xmeter_inc_max_power_diss(void);
void xmeter_dec_max_power_diss(void);
void xmeter_set_max_power_diss(const xmeter_value_t * val);
void xmeter_write_rom_max_power_diss(void);
  
/* change overheat config */
void xmeter_inc_temp_overheat(void);  
void xmeter_dec_temp_overheat(void);
void xmeter_set_temp_overheat(const xmeter_value_t * val);  
void xmeter_write_rom_temp_overheat(void);

/* inc dec xmeter_value */
void xmeter_inc_value(xmeter_value_t * val, bit coarse);
void xmeter_dec_value(xmeter_value_t * val, bit coarse);  
void xmeter_assign_value(const xmeter_value_t * src, xmeter_value_t * dst);
  
/* save calibrate config */	
void xmeter_write_rom_adc_voltage_in_kb(double k, double b);
void xmeter_write_rom_adc_voltage_out_kb(double k, double b);
void xmeter_write_rom_adc_current_kb(double k, double b);
void xmeter_write_rom_dac_current_kb(double k, double b);
void xmeter_write_rom_dac_voltage_kb(double k, double b);

/* 调整步长 */
void xmeter_get_voltage_steps(xmeter_value_t * coarse, xmeter_value_t * fine);
void xmeter_get_current_steps(xmeter_value_t * coarse, xmeter_value_t * fine);
void xmeter_get_temp_steps(xmeter_value_t * coarse, xmeter_value_t * fine);
void xmeter_get_power_steps(xmeter_value_t * coarse, xmeter_value_t * fine);

/* 解线性方程，获得斜率k，偏移b */  
bit xmeter_cal(uint16_t x1, uint16_t x2, double y1, double y2, double * k, double * b);  

void xmeter_dump_value(const char * name, xmeter_value_t * pval, uint8_t cnt);


void xmeter_dac_raw_write_bits(uint8_t addr, uint16_t dat);
uint16_t xmeter_dac_raw_read_bits(uint8_t addr);

#endif

