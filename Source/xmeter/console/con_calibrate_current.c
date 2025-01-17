#include "con_calibrate_current.h"
#include "cal_common.h"
#include "console.h"
#include "xmeter.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int8_t con_cal_current(char arg1, char arg2)
{

  uint16_t index, bits;
  double float_val;
  
  if(arg1 == 0) {
    for(index = 0 ; index < XMETER_GRID_SIZE; index ++) {
      console_printf("[ADC][%u] %04x -> %f\n", index, cal_grid_adc[index].bits, cal_grid_adc[index].val);
    }
    for(index = 0 ; index < XMETER_GRID_SIZE; index ++) {
      console_printf("[DAC][%u] %04x -> %f\n", index, cal_grid_dac[index].bits, cal_grid_dac[index].val);
    }
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "sadc") == 0 && arg2 > 0) {
    sscanf(&console_buf[arg2], "%u:%x:%f", &index, &bits, &float_val);
    cal_grid_adc[index].bits = bits;
    cal_grid_adc[index].val = float_val;
    console_printf("[ADC][%u] %04x -> %f\n", index, cal_grid_adc[index].bits, cal_grid_adc[index].val);
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "sdac") == 0 && arg2 > 0) {
    sscanf(&console_buf[arg2], "%u:%x:%f", &index, &bits, &float_val);
    cal_grid_dac[index].bits = bits;
    cal_grid_dac[index].val = float_val;
    console_printf("[DAC][%u] %04x -> %f\n", index, cal_grid_dac[index].bits, cal_grid_dac[index].val);
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "reset") == 0) {
    xmeter_reset_adc_current_config();
    xmeter_reset_dac_current_config();
    xmeter_reload_adc_current_config();
    xmeter_reload_dac_current_config();
    console_printf("reset ok!\n");
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "load") == 0) {
    xmeter_read_rom_adc_current_g(cal_grid_adc, XMETER_GRID_SIZE);
    xmeter_read_rom_dac_current_g(cal_grid_dac, XMETER_GRID_SIZE); 
    console_printf("load ok!\n");
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "save") == 0) {
    
    xmeter_write_rom_adc_current_g(cal_grid_adc, XMETER_GRID_SIZE);
    xmeter_write_rom_dac_current_g(cal_grid_dac, XMETER_GRID_SIZE); 
    
    xmeter_reload_adc_current_config();
    xmeter_reload_dac_current_config();
    
    console_printf("save ok!\n");
    
  } else if(arg1 > 0 && arg2 > 0) {
    sscanf(&console_buf[arg1], "%u", &index);
    if(index > XMETER_GRID_SIZE) 
      return 1;
    sscanf(&console_buf[arg2], "%f", &cal_grid_adc[index].val);
    cal_grid_dac[index].val = cal_grid_adc[index].val;
    cal_grid_adc[index].bits = xmeter_get_adc_bits_current();
    cal_grid_dac[index].bits = xmeter_get_dac_bits_c();
    console_printf("[ADC][%u] %04x -> %f\n", index, cal_grid_adc[index].bits, cal_grid_adc[index].val);
    console_printf("[DAC][%u] %04x -> %f\n", index, cal_grid_dac[index].bits, cal_grid_dac[index].val);
  } else {
    return 1;
  }
  return 0;
}