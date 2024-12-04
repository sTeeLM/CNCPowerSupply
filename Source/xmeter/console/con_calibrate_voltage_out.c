#include "con_calibrate_voltage_out.h"
#include "con_common.h"
#include "console.h"
#include "xmeter.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int8_t con_cal_voltage_out(char arg1, char arg2)
{
  double adc_k, adc_b, dac_k, dac_b;

  uint8_t index;
  
  if(arg1 == 0) {
    xmeter_read_rom_adc_voltage_out_kb(&adc_k, &adc_b);
    xmeter_read_rom_dac_voltage_kb(&dac_k, &dac_b);
    console_printf("[ADC] k = %f b = %f\n", adc_k, adc_b);
    console_printf("[DAC] k = %f b = %f\n", dac_k, dac_b);
  } else if(arg1 > 0 && (console_buf[arg1] == '0' || console_buf[arg1] == '1') && arg2 > 0) {
    sscanf(&console_buf[arg1], "%bu", &index);
    if(index > 2) index = 1;
    sscanf(&console_buf[arg2], "%f", &con_val[index]);
    con_adc_bits[index] = xmeter_get_adc_bits_voltage_out();
    con_dac_bits[index] = xmeter_get_dac_bits_v();
    console_printf("[ADC][%bu] bits = %04x\n", index, con_adc_bits[index]);
    console_printf("[DAC][%bu] bits = %04x\n", index, con_dac_bits[index]);
    console_printf("[VAL][%bu] val  = %f V\n", index, con_val[index]);    
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "done") == 0 && arg2 == 0) {
    if(xmeter_cal(con_adc_bits[0], con_adc_bits[1], 1, con_val[0], con_val[1], &adc_k, &adc_b)) {
      console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] => k = %f b = %f\n", 
        con_adc_bits[0], con_adc_bits[1], con_val[0], con_val[1], adc_k, adc_b);
      if(xmeter_cal(con_dac_bits[0], con_dac_bits[1], 0, con_val[0], con_val[1], &dac_k, &dac_b)) {
        console_printf("[DAC] [%04x] [%04x] -> [%f V] [%f V] => k = %f b = %f\n", 
          con_dac_bits[0], con_dac_bits[1], con_val[0], con_val[1], dac_k, dac_b);
        xmeter_write_rom_adc_voltage_out_kb(adc_k, adc_b);
        xmeter_write_rom_dac_voltage_kb(dac_k, dac_b);
        console_printf("save rom done!\n");
      } else {
        console_printf("[DAC] [%04x] [%04x] -> [%f V] [%f V] failed\n", 
          con_dac_bits[0], con_dac_bits[1], con_val[0], con_val[1]);
      }
    } else {
        console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] failed\n", 
          con_adc_bits[0], con_adc_bits[1], con_val[0], con_val[1]);
    }
  } else {
    return 1;
  }
  return 0;
}