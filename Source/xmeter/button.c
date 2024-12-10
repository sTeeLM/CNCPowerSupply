#include <STC89C5xRC.H>
#include "button.h"
#include "cext.h"
#include "task.h"
#include "clock.h"
#include "debug.h"
#include "sm.h"
#include "delay.h"
#include "beeper.h"
#include "gpio.h"

#define KEY_LPRESS_DELAY 1 // 长按时间

static bit mod_down;
static bit set_down;
static uint8_t key_down_cnt;
static bit mod_lpress_send;
static bit set_lpress_send;
static bit mod_set_press_send;
static bit mod_set_lpress_send;
static uint32_t set_down_sec;
static uint32_t mod_down_sec;
static uint32_t mod_set_down_sec;

void button_initialize(void)
{

  EX0 = 1;
  IT0 = 1;
  
  EX1 = 1;
  IT1 = 1;  
  
  PX0 = 0;
  IPH &= ~0x1; /* PX0H = 0 */
  
  PX1 = 0;
  IPH &= ~0x40; /* PX1H = 0 */
  
  mod_down = set_down = 0;
  key_down_cnt = 0;
  mod_lpress_send = 0;
  set_lpress_send = 0;
  mod_set_press_send = 0;
  mod_set_lpress_send = 0;
}

static void mod_A_ISR (void) interrupt 0 using 1
{

  if(KEY_MOD_B && !KEY_MOD_A) {
    task_set(EV_KEY_MOD_CC);
  } else if(!KEY_MOD_B && !KEY_MOD_A){
    task_set(EV_KEY_MOD_C);
  }
  IE0 = 0;
}

static void set_A_ISR (void) interrupt 2 using 1
{

  if(KEY_SET_B && !KEY_SET_A) {
    task_set(EV_KEY_SET_CC);
  } else if(!KEY_SET_B && !KEY_SET_A){
    task_set(EV_KEY_SET_C);
  }
  IE1 = 0;
}

static bit key_set_pressed()
{
  return KEY_SET_C == 0;
}

static bit key_mod_pressed()
{
  return KEY_MOD_C == 0;
}

void button_scan_proc(enum task_events ev)
{
  if(key_mod_pressed()) {
    if(mod_down == 0) {
      mod_down = 1;
      key_down_cnt ++;
      task_set(EV_KEY_MOD_DOWN);
      mod_down_sec = clock_get_now_sec();
      if(key_down_cnt == 2) {
        mod_set_down_sec = mod_down_sec;
      }
    }
  } else {
    if(mod_down == 1) {
      if(key_down_cnt == 2) {
        if(!mod_set_lpress_send) {
          task_set(EV_KEY_MOD_SET_PRESS);
          mod_set_press_send = 1;
        }
      } else {
        if(!mod_lpress_send && !mod_set_lpress_send && !mod_set_press_send) {
          task_set(EV_KEY_MOD_PRESS);
        }
       
      }
      mod_down = 0;
      key_down_cnt --;
      mod_lpress_send = 0;
      if(key_down_cnt == 0) {
        mod_set_lpress_send = 0;
        mod_set_press_send  = 0;
      }
      task_set(EV_KEY_MOD_UP);
    }
  }
  
  if(key_set_pressed()) {
    if(set_down == 0) {
      set_down = 1;
      key_down_cnt ++;
      task_set(EV_KEY_SET_DOWN);
      set_down_sec = clock_get_now_sec();
      if(key_down_cnt == 2) {
        mod_set_down_sec = set_down_sec;
      }     
    }
  } else {
    if(set_down == 1) {
      if(key_down_cnt == 2) {
        if(!mod_set_lpress_send) {
          task_set(EV_KEY_MOD_SET_PRESS);
          mod_set_press_send = 1;
        }
      } else {
        if(!set_lpress_send && !mod_set_lpress_send && !mod_set_press_send) {
          task_set(EV_KEY_SET_PRESS);
        }
      }      
      set_down = 0;
      key_down_cnt --;
      set_lpress_send = 0;
      if(key_down_cnt == 0) {
        mod_set_lpress_send = 0;
        mod_set_press_send  = 0;
      }
      task_set(EV_KEY_SET_UP);    
    }
  }
  
  if(key_down_cnt == 2) {
    if(clock_diff_now_sec(mod_set_down_sec) > KEY_LPRESS_DELAY) {
      task_set(EV_KEY_MOD_SET_LPRESS);
      mod_set_lpress_send = 1;
    }
  } else {
    if(mod_down) {
      if(clock_diff_now_sec(mod_down_sec) > KEY_LPRESS_DELAY) {
        task_set(EV_KEY_MOD_LPRESS);
        mod_lpress_send = 1;
      }
    }
    if(set_down) {
      if(clock_diff_now_sec(set_down_sec) > KEY_LPRESS_DELAY) {
        task_set(EV_KEY_SET_LPRESS);
        set_lpress_send = 1;
      }
    }
  }
  
}
/*
void button_scan_proc(enum task_events ev)
{
  if(key_mod_pressed()) {
    if(mod_down == 0) {
      mod_down = 1;
      mod_down_sec = clock_get_now_sec();
    }
  } else {
    if(mod_down) {
      if(set_down) {
        mod_set_pressed = 1;
        mod_set_down_diff = clock_diff_now_sec(set_down_sec);
      } else {
        if(mod_set_pressed == 0) {
          mod_pressed = 1;
          mod_down_diff = clock_diff_now_sec(mod_down_sec);
        } else {
          mod_set_pressed = 0;
          mod_set_pressed_send = 0;
        }
      }
      mod_down = 0;
    }
  }
  
  if(key_set_pressed()) {
    if(set_down == 0) {
      set_down = 1;
      set_down_sec = clock_get_now_sec();
    }
  } else {
    if(set_down) {
      if(mod_down) {
        mod_set_pressed = 1;
        mod_set_down_diff = clock_diff_now_sec(mod_down_sec);
      } else {
        if(mod_set_pressed == 0) {
          set_pressed = 1;
          set_down_diff = clock_diff_now_sec(set_down_sec);
        } else {
          mod_set_pressed = 0;
          mod_set_pressed_send = 0;
        }
      }
      set_down = 0;
    }
  }  

  if(mod_pressed || set_pressed || (mod_set_pressed & !mod_set_pressed_send)) {
    CDBG("[%c][%bu] [%c][%bu] [%c][%bu]\n", 
    mod_pressed ? 'x':' ', mod_down_diff,
    set_pressed ? 'x':' ', set_down_diff,
    mod_set_pressed ? 'x':' ', mod_set_down_diff);
  }

  if(mod_set_pressed && !mod_set_pressed_send) {
    if(mod_set_down_diff >= KEY_LPRESS_DELAY) {
      CDBG("EV_KEY_MOD_SET_LPRESS\n");
      task_set(EV_KEY_MOD_SET_LPRESS);
    } else {
      CDBG("EV_KEY_MOD_SET_PRESS\n");
      task_set(EV_KEY_MOD_SET_PRESS);
    }
    set_pressed = 0;
    mod_pressed = 0;
    mod_set_pressed_send = 1;
  } else if(set_pressed) {
    if(set_down_diff >= KEY_LPRESS_DELAY) {
      CDBG("EV_KEY_SET_LPRESS\n");
      task_set(EV_KEY_SET_LPRESS);
    } else {
      CDBG("EV_KEY_SET_PRESS\n");
      task_set(EV_KEY_SET_PRESS);
    }
    set_pressed = 0;
  } else if(mod_pressed) {
    if(mod_down_diff >= KEY_LPRESS_DELAY) {
      CDBG("EV_KEY_MOD_LPRESS\n");
      task_set(EV_KEY_MOD_LPRESS);
    } else {
      CDBG("EV_KEY_MOD_PRESS\n");
      task_set(EV_KEY_MOD_PRESS);
    }
    mod_pressed = 0;
  }
}
*/


void button_mod_proc(enum task_events ev)
{
  switch (ev) {
    case EV_KEY_MOD_DOWN:
      CDBG("button_set_proc EV_KEY_MOD_DOWN\n");  
      break;      
    case EV_KEY_MOD_C:
      CDBG("button_set_proc EV_KEY_MOD_C\n");  
      break;    
    case EV_KEY_MOD_CC:
      CDBG("button_set_proc EV_KEY_MOD_CC\n");
      break;    
    case EV_KEY_MOD_PRESS:
      CDBG("button_mod_proc EV_KEY_MOD_PRESS\n");
      beeper_beep();    
      break;
    case EV_KEY_MOD_LPRESS:
      beeper_beep_beep();
      CDBG("button_mod_proc EV_KEY_MOD_LPRESS\n");
      break; 
    case EV_KEY_MOD_UP:
      CDBG("button_set_proc EV_KEY_MOD_UP\n");  
      break;  
    default:
      ;
  }
  
  sm_run(ev);
}

void button_set_proc(enum task_events ev)
{
  switch (ev) {
    case EV_KEY_SET_DOWN:
      CDBG("button_set_proc EV_KEY_SET_DOWN\n");  
      break;     
    case EV_KEY_SET_C:
      CDBG("button_set_proc EV_KEY_SET_C\n");  
      break;    
    case EV_KEY_SET_CC:
      CDBG("button_set_proc EV_KEY_SET_CC\n");
      break;
    case EV_KEY_SET_PRESS:
      CDBG("button_set_proc EV_KEY_SET_PRESS\n");
      beeper_beep();
      break;
    case EV_KEY_SET_LPRESS:
      beeper_beep_beep();
      CDBG("button_set_proc EV_KEY_SET_LPRESS\n");
      break; 
    case EV_KEY_SET_UP:
      CDBG("button_set_proc EV_KEY_SET_UP\n");  
      break;  
    default:
      ;
  }
  
  sm_run(ev);
}


void button_mod_set_proc(enum task_events ev)
{
  switch (ev) {
    case EV_KEY_MOD_SET_PRESS:
      beeper_beep_beep();
      CDBG("button_mod_set_proc EV_KEY_MOD_SET_PRESS\n");
      break;
    case EV_KEY_MOD_SET_LPRESS:
      beeper_beep_beep();
      CDBG("button_mod_set_proc EV_KEY_MOD_SET_LPRESS\n");
      break;
    default:
      ;    
  }

  sm_run(ev);
}

bool button_is_factory_reset(void)
{
  return key_set_pressed();
}
