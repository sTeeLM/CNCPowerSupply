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
#include "console.h"
#include "version.h"

int main(void)
{
  EA = 1;                                       // enable global interrupts
  gpio_initialize();
  debug_initialize();                           // initialize debug system
  com_initialize(); 
  
  lcd_initialize();
  
	rom_initialize();
  
  lcd_clear();
  lcd_set_string(0,0, "++XMETER V"  __XMETER_VERSION_STRING__ " ++");
  lcd_refresh();
  
  lcd_show_progress(1,1);
  
  clock_initialize();
  task_initialize();
  sm_initialize();
  
  lcd_show_progress(1,1); 
  
  button_initialize();
  beeper_initialize();
  control_initilize();
  console_initilize();
  
  lcd_show_progress(1,2); 
  
  xmeter_initialize();
  
  lcd_show_progress(1,16);
  
	while(1) {
    console_run();
    task_run();
    control_run();
	}
}
