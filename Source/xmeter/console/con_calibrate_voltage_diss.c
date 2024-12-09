#include "con_calibrate_voltage_diss.h"
#include "con_common.h"
#include "console.h"
#include "xmeter.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int8_t con_cal_voltage_diss(char arg1, char arg2)
{
  uint16_t bits, index;
  double value;
  if(arg1 == 0) {
    for(index = 0 ; index < XMETER_GRID_SIZE; index ++) {
      console_printf("[ADC][%u] %04x -> %f\n", index, con_grid_adc[index].bits, con_grid_adc[index].val);
    }
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "reset") == 0) {
    xmeter_reset_adc_voltage_diss_config();
    xmeter_reload_adc_voltage_diss_config();
    
    console_printf("reset ok!\n");
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "load") == 0) {
    xmeter_read_rom_adc_voltage_diss_g(con_grid_adc, XMETER_GRID_SIZE);
    console_printf("load ok!\n");
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "save") == 0) {
    xmeter_write_rom_adc_voltage_diss_g(con_grid_adc, XMETER_GRID_SIZE);

    xmeter_reload_adc_voltage_diss_config();

    console_printf("save ok!\n");
    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "max") == 0 && arg2 > 0) {

    sscanf(&console_buf[arg2], "%f", &value);
    bits = xmeter_get_adc_bits_voltage_diss();    
    xmeter_cal_grid(0x0, bits, 1, 0.0, value, con_grid_adc, XMETER_GRID_SIZE);
    console_printf("cal ok!\n");
    
  } else {
    return 1;
  }
  return 0;
}