#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "task.h"
#include "debug.h"
#include "sm.h"

/* hardware*/
#include "clock.h"
#include "button.h"


const char * code task_names[] =
{
  "EV_INIT",
  "EV_KEY_SCAN",  
  "EV_250MS",
  "EV_1S",
  "EV_KEY_SET_DOWN",  
  "EV_KEY_SET_C",
  "EV_KEY_SET_CC",
  "EV_KEY_SET_PRESS",
  "EV_KEY_SET_LPRESS",
  "EV_KEY_SET_UP",
  "EV_KEY_MOD_DOWN",
  "EV_KEY_MOD_C",
  "EV_KEY_MOD_CC",
  "EV_KEY_MOD_PRESS",
  "EV_KEY_MOD_LPRESS",
  "EV_KEY_MOD_UP",  
  "EV_KEY_MOD_SET_PRESS",
  "EV_KEY_MOD_SET_LPRESS",
  "EV_OVER_HEAT",
  "EV_OVER_PD",
  "EV_TEMP_HI",
  "EV_TEMP_LO",
  "EV_CC_CHANGE",
  "EV_TIMEO",
  "EV_EVKEY"
};

static void null_proc(enum task_events ev)
{
  sm_run(ev);
}


static const TASK_PROC task_procs[EV_CNT] = 
{
  null_proc,
  /* EV_SCAN_KEY         = 1, // 扫描按键 */
  button_scan_proc,  
  /* EV_250MS            = 2, // 大约每250ms转一下 */
  clock_time_proc,
  /* EV_1S               = 3, // 大约每1s转一下   */
  clock_time_proc,
  /* EV_KEY_SET_XX */
  button_set_proc,
  button_set_proc,
  button_set_proc,
  button_set_proc,
  button_set_proc,
  button_set_proc,
  /* EV_KEY_MOD_XX */
  button_mod_proc,
  button_mod_proc,
  button_mod_proc,  
  button_mod_proc,
  button_mod_proc,
  button_mod_proc,
  /* EV_KEY_MOD_SET_XX */
  button_mod_set_proc,
  button_mod_set_proc, 
  null_proc,
  null_proc,
  null_proc,
  null_proc,
  null_proc,
  null_proc,
  null_proc,
};


uint16_t ev_bits0;
uint16_t ev_bits1;

void task_initialize (void)
{
  CDBG("task_initialize\n");
  ev_bits0 = 0;
  ev_bits1 = 0;
}

void task_run(void)
{
  uint8_t c;
  for(c = 0; c < EV_CNT; c++) {
    if(task_test(c)) {
      task_clr(c);
      task_procs[c](c);
    }
  }
}

void task_dump(void)
{
  uint8_t i;
  for (i = 0 ; i < EV_CNT; i ++) {
    CDBG("[%02bd][%s] %c\n", i, task_names[i], task_test(i) ? '1' : '0');
  }
}
