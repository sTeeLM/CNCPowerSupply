#ifndef __XMETER_SM_H__
#define __XMETER_SM_H__

#include <stdint.h>
#include <stddef.h>
#include "task.h"


enum sm_functions
{
  SM_XMETER  = 0,         // 电压电路显示功能
  SM_SET_PARAM,           // 参数设置  
  SM_CALIBRATE,           // 校准
};

typedef void (*SM_PROC)(uint8_t to_func, uint8_t to_state, enum task_events ev);


/* function: array of state */
/* state: array of trans */

struct sm_trans_slot
{
  enum task_events event;
  uint8_t to_function;
  uint8_t to_state;
  SM_PROC sm_proc;
};

struct sm_state_slot {
  const char * state_name;
  struct sm_trans_slot * state_trans_array;
};

struct sm_function_slot {
  const char * function_name;
  struct sm_state_slot * function_states_array;
};

extern uint8_t sm_cur_function;
extern uint8_t sm_cur_state;

void sm_initialize(void);

void sm_run(enum task_events ev);

#endif
