#include <STC89C5xRC.H>
#include <stdio.h>

#include "debug.h"
#include "com.h"
#include "delay.h"
#include "rom.h"
#include "clock.h"
#include "task.h"
#include "sm.h"
#include "button.h"
#include "beeper.h"
#include "lcd.h"
#include "xmeter.h"
#include "gpio.h"
#include "control.h"

int main(void)
{
  EA = 1;                                       // enable global interrupts
  gpio_initialize();
  debug_initialize();                           // initialize debug system
  com_initialize(); 
	rom_initialize();
  lcd_initialize();
  
  lcd_set_string(0,0, "++XMETER V1.0++");
  lcd_set_string(1,0, "---by sTeeLM---");
  lcd_refresh();
  
  clock_initialize();
  task_initialize();
  sm_initialize();
  button_initialize();
  beeper_initialize();
  control_initilize();
  
  xmeter_initialize();
  
	while(1) {
    task_run();
    control_run();
	}
}
