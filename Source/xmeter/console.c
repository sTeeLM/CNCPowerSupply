#include "console.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "com.h"
#include "clock.h"
#include "lcd.h"
#include "debug.h"
#include "sm.h"

#include "con_calibrate_current.h"
#include "con_calibrate_voltage_out.h"
#include "con_calibrate_voltage_diss.h"
#include "con_calibrate_temperature.h"

#define CONSOLE_BUFFER_SIZE 41

void console_initilize(void)
{
  CDBG("console_initilize\n");
}


char console_buf[CONSOLE_BUFFER_SIZE];

static int8_t con_quit(char arg1, char arg2)
{
  console_printf("quit!\n");
  return 0;
}

static int8_t con_help(char arg1, char arg2)
{
  int8_t i; 

  if(arg1 == 0) {
    console_dump_cmd();
  } else {
    i = console_search_cmd_by_name(console_buf + arg1);
    if(i != -1) {
      console_printf("usage:\n%s\n", cmds[i].usage);
    } else {
      console_printf("unknown cmd '%s'\n",console_buf + arg1);
    }
  }
  return 0;
}

struct console_cmds cmds[] = 
{
  {"?",  "show help", "?: list cmd\r\n"
                      "? <cmd>: show usage of cmd",
                      con_help},  
  {"!", "quit the console", "!", 
                      con_quit},
  {"cc", "calibrate current", 
                      "cc: dump paramenters\n"
                      "cc load: load paramenters from rom\n"
                      "cc reset: factory reset\n"
                      "cc n xx: set record n value\n" 
                      "cc save: save paramenters to rom",
                      con_cal_current}, 
  {"cvo", "calibrate voltage out", 
                      "cvo: dump paramenters\n"
                      "cvo load: load paramenters from rom\n"
                      "cvo reset: factory reset\n"
                      "cvo n xx: set record n value\n" 
                      "cvo save: save paramenters to rom",
                      con_cal_voltage_out},
  {"cvd", "calibrate voltage diss", 
                      "cvd: dump paramenters\n" 
                      "cvd load: load paramenters from rom\n"
                      "cvd reset: factory reset\n"
                      "cvd max xx: record max value (min value always 0)\n"
                      "cvd save: do calibrate and save rom",
                      con_cal_voltage_diss}, 
  {"ct",  "calibrate temperature", 
                      "ct: dump temperature repeatlly until hit key",
                      con_cal_temp},   
}; 

int8_t console_search_cmd_by_name(char * cmd)
{
  char i;
  for (i = 0 ; i < sizeof(cmds)/sizeof(struct console_cmds) ; i ++) {
    if(strcmp(cmd, cmds[i].cmd) == 0) {
      return i;
    }
  }
  return -1;
}

void console_dump_cmd(void)
{
  char i;
  for (i = 0 ; i < sizeof(cmds)/sizeof(struct console_cmds) ; i ++) {
    console_printf("%s: %s\n", cmds[i].cmd, cmds[i].desc);
  }
}

static void console_call_cmd(char * buf, char arg1, char arg2)
{
  int8_t i;
  i = console_search_cmd_by_name(buf);
  if(i != -1) {
    if(cmds[i].proc(arg1, arg2) != 0) { // C212
      console_printf("%s:\n%s\n", cmds[i].cmd, cmds[i].usage);
    }
  } else {
    console_printf("unknown cmd '%s'\n", buf);
  }
}

int16_t console_get_char(void)
{
  char c;
  uint16_t len = 1;
  com_recv_buffer(&c, &len, 0);
  return c;
}

int16_t console_try_get_char(void)
{
  char c;
  bit ret = com_try_get_char(&c);
  return ret ? c : 0;
}

static void console_gets(char * buffer, uint16_t len)
{
  uint16_t i = 0;
  char c;
  static char erase_in_line [2]={0x9b, 0x4b}; 
  while((c = console_get_char()) > 0 && i < len) {
    if(c != '\r' && c != '\n' && c != 0x8 && c != 0x7f) {
      com_send_buffer((uint8_t *)&c, 1);
    } else if((c == 0x8 ||  c == 0x7f) && i > 0) {
      com_send_buffer((uint8_t *)&c, 1);
      com_send_buffer(erase_in_line, 2);
    }
    if(c != '\r' && c != '\n' && c != 0x8 && c != 0x7f) {
      buffer[i++] = c;
    } else if((c == 0x8 ||  c == 0x7f)){ // backspace or del
      if(i > 0)
        buffer[--i] = 0;
    } else if(c == '\r' || c == '\n'){
      buffer[i++] = 0;
      break;
    }
  }
}



void console_printf(const char * fmt /*format*/, ...)
{
  va_list arg_ptr;
  
  va_start (arg_ptr, fmt); /* format string */
  vprintf(fmt, arg_ptr);
  va_end (arg_ptr);
}

void console_run(void)
{
#ifdef __XMETER_DEBUG__    
  char arg1_pos, arg2_pos, c;

  if((c = console_try_get_char()) != 'c' || c == 0) {
    return;
  }
  
  console_printf("++++++++++++++++++++++++++++++++++++++++\n");
  console_printf("+             tini CONSOLE             +\n");
  console_printf("++++++++++++++++++++++++++++++++++++++++\n");
  
  // stop the clock
  clock_enter_shell();
  lcd_enter_shell();
  sm_enter_shell();
  
  do {
    console_printf("console>");
    memset(console_buf, 0, sizeof(console_buf));
    console_gets(console_buf, sizeof(console_buf)-1);
    console_buf[sizeof(console_buf)-1] = 0;
    console_printf("\r\n");
    if(console_buf[0] == 0)
      continue;
    
    arg1_pos = 0;
    arg2_pos = 0;
    
    while(arg1_pos < sizeof(console_buf) && console_buf[arg1_pos] != ' ') arg1_pos ++;
    
    if(arg1_pos >= sizeof(console_buf)) {
      console_call_cmd(console_buf, 0, 0);
      continue;
    }
    
    while(arg1_pos < sizeof(console_buf) && console_buf[arg1_pos] == ' ') {
      console_buf[arg1_pos] = 0;
      arg1_pos ++;
    }
    
    if(arg1_pos >= sizeof(console_buf) || console_buf[arg1_pos] == 0) {
      console_call_cmd(console_buf, 0, 0);
      continue;
    }
    
    arg2_pos = arg1_pos;
    
    while(arg2_pos < sizeof(console_buf) && console_buf[arg2_pos] != ' ') arg2_pos ++;
    
    if(arg2_pos >= sizeof(console_buf)) {
      console_call_cmd(console_buf, arg1_pos, 0);
      continue;
    }
    
    while(arg2_pos < sizeof(console_buf) && console_buf[arg2_pos] == ' ') {
      console_buf[arg2_pos] = 0;
      arg2_pos ++;
    }
    
    if(arg2_pos >= sizeof(console_buf) || console_buf[arg2_pos] == 0) {
      console_call_cmd(console_buf, arg1_pos, 0);
      continue;
    }
    
    console_call_cmd(console_buf, arg1_pos, arg2_pos);
    
    console_printf("\r\n");
    
  } while (strcmp(console_buf, "!") != 0);

  sm_leave_shell();
  lcd_leave_shell();
  clock_leave_shell(); 
#endif
}
