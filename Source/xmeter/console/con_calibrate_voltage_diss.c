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

  if(arg1 == 0) {
    xmeter_read_rom_adc_voltage_diss_kb(&adc_k, &adc_b);
    console_printf("[ADC] k = %f b = %f\n", adc_k, adc_b);
  } else if(arg1 > 0 && console_buf[arg1] == '1' && arg2 > 0) {
    con_adc_bits[0] = 0;
    con_val[0] = 0.0;
    con_adc_bits[1] = xmeter_get_adc_bits_voltage_diss();
    sscanf(&console_buf[arg2], "%f", &con_val[1]);
    console_printf("[ADC][0] bits = %04x\n", con_adc_bits[0]);
    console_printf("[VAL][0] val  = %f V\n", con_val[0]);
    console_printf("[ADC][1] bits = %04x\n", con_adc_bits[1]);
    console_printf("[VAL][1] val  = %f V\n", con_val[1]);
  } else if(arg1 > 0 && strcmp(&console_buf[arg1], "done") == 0 && arg2 == 0) {
    if(xmeter_cal(con_adc_bits[0], con_adc_bits[1], 1, con_val[0], con_val[1], &adc_k, &adc_b)) {
      console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] => k = %f b = %f\n", 
        con_adc_bits[0], con_adc_bits[1], con_val[0], con_val[1], adc_k, adc_b);
      xmeter_write_rom_adc_voltage_diss_kb(adc_k, adc_b);
      console_printf("save rom done!\n");
    } else {
        console_printf("[ADC] [%04x] [%04x] -> [%f V] [%f V] failed\n", 
          con_adc_bits[0], con_adc_bits[1], con_val[0], con_val[1]);
    }
  } else {
    return 1;
  }

  return 0;
}