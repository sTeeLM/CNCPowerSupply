#ifndef __XMETER_CLOCK_H__
#define __XMETER_CLOCK_H__

#include "task.h"

void clock_initialize ();
void clock_enter_shell(void);
void clock_leave_shell(void);

struct clock_struct 
{
  unsigned char year;   // 0 - 99 (2000 ~ 2099)
  unsigned char mon;    // 0 - 11
  unsigned char date;   // 0 - 30(29/28/27)
  unsigned char day;    // 0 - 6
  unsigned char hour;   // 0 - 23
  unsigned char min;    // 0 - 59
  unsigned char sec;    // 0 - 59
  unsigned char ms39;   // 0 - 255
};

#define CLOCK_YEAR_BASE 2000
#define is_leap_year(y) \
(( ((y%100) !=0) && ((y%4)==0)) || ( (y%400) == 0))

void clock_enable_interrupt(bit enable);
void clock_enable_tick(bit enable);

unsigned char clock_get_sec(void);
unsigned char clock_get_sec_256(void);
void clock_clr_sec(void);
unsigned char clock_get_min(void);
void clock_inc_min(void);
unsigned char clock_get_hour(void);
void clock_inc_hour(void);
unsigned char clock_get_date(void);
void clock_inc_date(void);
unsigned char clock_get_day(void);
unsigned char clock_get_month(void);
void clock_inc_month(void);
unsigned char clock_get_year(void);
void clock_inc_year(void);
void clock_dump(void);
unsigned char clock_get_ms39(void);

bit clock_is_leap_year(unsigned char year); // year 0-99
unsigned char clock_get_mon_date(unsigned char year, unsigned char mon); // mon 0-11
unsigned char clock_yymmdd_to_day(unsigned char year, unsigned char mon, unsigned char date);

void clock_time_proc(enum task_events ev);

unsigned long clock_get_now_sec(void);
unsigned long clock_diff_now_sec(unsigned long sec);

#endif
