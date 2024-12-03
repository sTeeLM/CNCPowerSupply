#include "con_calibrate_voltage_diss.h"
#include "con_common.h"
#include "console.h"
#include "xmeter.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int8_t con_cal_voltage_diss(char arg1, char arg2)
{
  double adc_k, adc_b;

  uint8_t index;
  
  if(arg1 == 0) {
    xmeter_read_rom_adc_voltage_diss_kb(&adc_k, &adc_b);
    console_printf("[ADC] k = %f b = %f\n", adc_k, adc_b);
  } else if(arg1 > 0 && (console_buf[arg1] == '0' || console_buf[arg1] == '1') && arg2 > 0) {
    sscanf(&console_buf[arg2], "%bu", &index);
    if(index > 2) index = 1;
    sscanf(&console_buf[arg2], "%f", &val[index]);
    adc_bits[index] = xmeter_get_adc_bits_voltage_diss();
    console_printf("[ADC][%bu] bits = %04x\n", index, adc_bits[index]);
    console_printf("[VAL][%bu] val  = %f V\n", index, val[index]);    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "done") == 0 && arg2 == 0) {
    if(xmeter_cal(adc_bits[0], adc_bits[1], 1, val[0], val[1], &adc_k, &adc_b)) {
      console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] => k = %f b = %f\n", 
        adc_bits[0], adc_bits[1], val[0], val[1], adc_k, adc_b);
      xmeter_write_rom_adc_voltage_diss_kb(adc_k, adc_b);
      console_printf("save rom done!\n");
    } else {
        console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] failed\n", 
          adc_bits[0], adc_bits[1], val[0], val[1]);
    }
  } else {
    return 1;
  }
  return 0;
}