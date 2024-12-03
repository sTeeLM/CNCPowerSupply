#ifndef __IVCLOCK_TASK_H__
#define __IVCLOCK_TASK_H__

#include <stdint.h>

// max 32
enum task_events
{
  EV_INIT                = 0,
  EV_KEY_SCAN            = 1, // 扫描按键   
  EV_250MS               = 2, // 大约每250ms转一下
  EV_1S                  = 3, // 大约每1s转一下  
  EV_KEY_SET_C           = 4, // set顺时针旋转
  EV_KEY_SET_CC          = 5, // set逆时针旋转  
  EV_KEY_SET_PRESS       = 6, // set键短按
  EV_KEY_SET_LPRESS      = 7, // set键长按
  EV_KEY_MOD_C           = 8, // mod顺时针旋转
  EV_KEY_MOD_CC          = 9, // mod逆时针旋转  
  EV_KEY_MOD_PRESS       = 10, // mod键短按
  EV_KEY_MOD_LPRESS      = 11, // mod键长按
  EV_KEY_MOD_SET_PRESS   = 12, // mod set 键同时短按
  EV_KEY_MOD_SET_LPRESS  = 13, // mod set 键同时长按
  EV_OVER_HEAT           = 14, // 超过最高温度限制，只要超过就会持续发过热消息
  EV_OVER_PD             = 15, // 计算耗散功率大于最大限制（可能温度计还没有反应过来），并且对外开关处于打开状态
  EV_TEMP_HI             = 16, // 超过风扇温度上限，并且风扇处于关闭状态
  EV_TEMP_LO             = 17, // 低于风扇温度下线，并且风扇处于打开状态
  EV_CC_CHANGE           = 18, // CC状态变化
  EV_TIMEO               = 19, // 超时  
  EV_CNT  
};

extern uint16_t ev_bits0;
extern uint16_t ev_bits1;

extern const char * task_names[];

typedef void (*TASK_PROC)(enum task_events);

void task_initialize (void);

void task_dump(void);

// 这些宏也在中断里被调用，所以不能是带参数函数，只能拿宏实现了
#define task_set(ev1)             \
  do{                             \
    if(ev1 < 16)                  \
      ev_bits0 |= 1<<ev1;         \
    else                          \
      ev_bits1 |= 1<<(ev1 - 16);  \
  }while(0)

#define task_clr(ev1)               \
  do{                               \
    if(ev1 < 16)                    \
      ev_bits0 &= ~(1<<ev1);        \
    else                            \
      ev_bits1 &= ~(1<<(ev1 - 16)); \
  }while(0)
    
#define  task_test(ev1)             \
  (ev1 < 16 ? (ev_bits0 & (1<<ev1)) : (ev_bits1 & (1<<(ev1 - 16))))

void task_run(void);

#endif