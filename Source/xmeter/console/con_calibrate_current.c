#include "con_calibrate_current.h"
#include "con_common.h"
#include "console.h"
#include "xmeter.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int8_t con_cal_current(char arg1, char arg2)
{
  double adc_k, adc_b, dac_k, dac_b;

  uint8_t index;
  
  if(arg1 == 0) {
    xmeter_read_rom_adc_current_kb(&adc_k, &adc_b);
    xmeter_read_rom_dac_current_kb(&dac_k, &dac_b);
    console_printf("[ADC] k = %f b = %f\n", adc_k, adc_b);
    console_printf("[DAC] k = %f b = %f\n", dac_k, dac_b);
  } else if(arg1 > 0 && (console_buf[arg1] == '0' || console_buf[arg1] == '1') && arg2 > 0) {
    sscanf(&console_buf[arg2], "%bu", &index);
    if(index > 2) index = 1;
    sscanf(&console_buf[arg2], "%f", &val[index]);
    adc_bits[index] = xmeter_get_adc_bits_current();
    dac_bits[index] = xmeter_get_dac_bits_c();
    console_printf("[ADC][%bu] bits = %04x\n", index, adc_bits[index]);
    console_printf("[DAC][%bu] bits = %04x\n", index, dac_bits[index]);
    console_printf("[VAL][%bu] val  = %f A\n", index, val[index]);    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "done") == 0 && arg2 == 0) {
    if(xmeter_cal(adc_bits[0], adc_bits[1], 1, val[0], val[1], &adc_k, &adc_b)) {
      console_printf("[ADC] [%04x] [%04x] -> [%f A] [%f A] => k = %f b = %f\n", 
        adc_bits[0], adc_bits[1], val[0], val[1], adc_k, adc_b);
      if(xmeter_cal(dac_bits[0], dac_bits[1], 0, val[0], val[1], &dac_k, &dac_b)) {
        console_printf("[DAC] [%04x] [%04x] -> [%f A] [%f A] => k = %f b = %f\n", 
          dac_bits[0], dac_bits[1], val[0], val[1], dac_k, dac_b);
        xmeter_write_rom_adc_current_kb(adc_k, adc_b);
        xmeter_write_rom_dac_current_kb(dac_k, dac_b);
        console_printf("save rom done!\n");
      } else {
        console_printf("[DAC] [%04x] [%04x] -> [%f A] [%f A] failed\n", 
          dac_bits[0], dac_bits[1], val[0], val[1]);
      }
    } else {
        console_printf("[ADC] [%04x] [%04x] -> [%f A] [%f A] failed\n", 
          adc_bits[0], adc_bits[1], val[0], val[1]);
    }
  } else {
    return 1;
  }
  return 0;
}