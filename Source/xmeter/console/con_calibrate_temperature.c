#include "con_calibrate_temperature.h"
#include "console.h"
#include "xmeter.h"
#include "com.h"
#include "debug.h"
#include "delay.h"

#include <stdio.h>
#include <stdint.h>

int8_t con_cal_temp(char arg1, char arg2)
{
  uint8_t break_flag = 0, i;
  console_printf("hit any key to stop\n");
  while(!break_flag) {
    xmeter_read_adc_temp();
    xmeter_dump_value("adc temp", &xmeter_adc_temp, 1);
    for(i = 0 ; i < 4; i++) {
      delay_ms(0xFF);   
      if(console_try_get_char()) {
        break_flag = 1;
        console_printf("quit!\n");
        break;
      }
    }
  }
  
  return 0;
}