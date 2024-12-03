#include "con_calibrate_temperature.h"
#include "console.h"
#include "xmeter.h"
#include "com.h"

#include <stdio.h>
#include <stdint.h>

int8_t con_cal_temp(char arg1, char arg2)
{
  char c;
  console_printf("hit any key to stop\n");
  while(com_recv_buffer(&c, 1, 1000) == 0) {
    xmeter_read_adc_temp();
    xmeter_dump_value("adc temp", &xmeter_adc_temp, 1);
  }
  
  return 0;
}