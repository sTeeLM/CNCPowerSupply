#include <STC89C5xRC.H>
#include <stdio.h>

#include "task.h"
#include "debug.h"
#include "cext.h"
#include "delay.h"
#include "clock.h"
#include "sm.h"
#include "lcd.h"

// ISR里不能调带参数函数。。。
// 2000~2099年
static unsigned char code date_table[2][12] = 
{
{31,29,31,30,31,30,31,31,30,31,30,31,}, // 2000
{31,28,31,30,31,30,31,31,30,31,30,31,},
};

static struct clock_struct idata clk;
static unsigned char idata sec_256; // 用于 time_diff
static unsigned char idata giff;

static bit in_shell;
static bit clock_tick_enabled;

#pragma NOAREGS
static void clock_inc_ms39(void)
{
  unsigned int y;
  
  clk.ms39 ++;
  
  if((clk.ms39 % 16) == 0) {
    task_set(EV_KEY_SCAN);
  }
  
  if((clk.ms39 % 64) == 0) {
    task_set(EV_250MS);
  }
    
  if(clk.ms39 == 0 ) {
    clk.sec = (++ clk.sec) % 60;
    sec_256 ++;
    task_set(EV_1S);
    if(clk.sec == 0) {
      clk.min = (++ clk.min) % 60;
      if(clk.min == 0) {
        clk.hour = (++ clk.hour) % 24;
        if(clk.hour == 0) {
          y = clk.year + CLOCK_YEAR_BASE;
          if(is_leap_year(y)) {
            clk.date = (++ clk.date) % date_table[0][clk.mon];
          } else {
            clk.date = (++ clk.date) % date_table[1][clk.mon];
          }
          clk.day = (++ clk.day) % 7;
          if(clk.day == 0) {
            clk.mon = (++ clk.mon) % 12;
            if(clk.mon == 0) {
              clk.year = (++ clk.year) % 100;
            }
          }
        }
      }
    } 
  }
}
#pragma AREGS 

void clock_dump(void)
{
  CDBG("clk.year = %bu\n", clk.year);
  CDBG("clk.mon  = %bu\n", clk.mon);
  CDBG("clk.date = %bu\n", clk.date); 
  CDBG("clk.day  = %bu\n", clk.day);
  CDBG("clk.hour = %bu\n", clk.hour);
  CDBG("clk.min  = %bu\n", clk.min);
  CDBG("clk.sec  = %bu\n", clk.sec);  
  CDBG("clk.ms39 = %bu\n", clk.ms39);
}

// 计算某年某月某日星期几,  经典的Zeller公式
// year 0-99
// mon 0-11
// date 0-30
// return 0-6, 0 is monday, 6 is sunday
unsigned char clock_yymmdd_to_day(unsigned char year, unsigned char mon, unsigned char date)
{
  unsigned int d,m,y,c;
  d = date + 1;
  m = mon + 1;
  y = CLOCK_YEAR_BASE + year;
  
  if(m < 3){
    y -= 1;
    m += 12;
  }
  
  c = (unsigned int)(y / 100);
  y = y - 100 * c;
  
  c = (unsigned int)(c / 4) - 2 * c + y + (unsigned int) ( y / 4 ) + (26 * (m + 1) / 10) + d - 1;
  c = (c % 7 + 7) % 7;

  if(c == 0) {
    return 6;
  }
  
  return c - 1;
}


unsigned char clock_get_ms39(void)
{
  return clk.ms39;
}

unsigned char clock_get_sec_256(void)
{
  return sec_256;
}

unsigned char clock_get_sec(void)
{
  return clk.sec;
}
void clock_clr_sec(void)
{
   clk.sec = 0;
}

unsigned char clock_get_min(void)
{
  return clk.min;
}
void clock_inc_min(void)
{
  clk.min = (++ clk.min) % 60;
}

unsigned char clock_get_hour(void)
{
  return clk.hour;
}
void clock_inc_hour(void)
{
  clk.hour = (++ clk.hour) % 24;
}


unsigned char clock_get_date(void)
{
  return clk.date + 1;
}
void clock_inc_date(void)
{
  clk.date = ( ++ clk.date) % clock_get_mon_date(clk.year, clk.mon);
  clk.day = clock_yymmdd_to_day(clk.year, clk.mon, clk.date);
}

unsigned char clock_get_day(void)
{
  return clk.day + 1;
}

unsigned char clock_get_month(void)
{
  return clk.mon + 1;
}
void clock_inc_month(void)
{
  clk.mon = (++ clk.mon) % 12;
  clk.day = clock_yymmdd_to_day(clk.year, clk.mon, clk.date);
}

unsigned char clock_get_year(void)
{
  return clk.year;
}
void clock_inc_year(void)
{
  clk.year = (++ clk.year) % 100;
  clk.day = clock_yymmdd_to_day(clk.year, clk.mon, clk.date);
}

static void clock0_ISR (void) interrupt 1 using 1
{  

  giff ++;
  
  if(in_shell) {
    TF0 = 0;
    return;
  }
  if((giff % 2) && clock_tick_enabled) {
    clock_inc_ms39();
  }
  TF0 = 0;
}


// 辅助函数
bit clock_is_leap_year(unsigned char year)
{
  unsigned int y;
  if(year >= 100) year = 99;
  y = year + CLOCK_YEAR_BASE;
  return is_leap_year(y);
}

// 返回某一年某一月有几天
unsigned char clock_get_mon_date(unsigned char year, unsigned char mon)
{
  if(year >= 100) year = 99;
  if(mon >= 12) mon = 11;
  if(clock_is_leap_year(year))
    return date_table[0][mon];
  else
    return date_table[1][mon];
}

void clock_enable_interrupt(bit enable)
{
  ET0 = enable;
  TR0 = enable;
}

void clock_enable_tick(bit enable)
{
  clock_tick_enabled = enable;
}

void clock_initialize(void)
{
  CDBG("clock_initialize\n");
  giff = 0;
  // GATE = 0
  // CT = 1
  // M1 = 1
  // M2 = 0
  TMOD |= 0x06; // 工作在模式2
  TL0 = (256 - 128 + 64); // 32768HZ方波输入，0.001953125s中断一次（512个中断是1s）
  TH0 = (256 - 128 + 64);
  IPH &= ~0x2;
  PT0 = 1; // 次高优先级 
  clock_enable_interrupt(1);
  clock_enable_tick(1);
  clock_dump();
}

void clock_time_proc(enum task_events ev)
{
  if(ev == EV_250MS) {
    lcd_refresh();
  }
  sm_run(ev);
}

void clock_enter_shell(void)
{
  in_shell = 1;
}

void clock_leave_shell(void)
{
  in_shell = 0;
  clock_enable_interrupt(0);
  clock_enable_interrupt(1);
}

unsigned long clock_get_now_sec(void)
{
  return sec_256;
}


unsigned long clock_diff_now_sec(unsigned long sec)
{
  return (unsigned long)(sec_256 - sec);
}