#ifndef __XMETER_SM_SET_PARAM_H__
#define __XMETER_SM_SET_PARAM_H__

#include "task.h"

enum sm_states_set_param
{
  SM_SET_PARAM_FAN,
  SM_SET_PARAM_OVER,	
  SM_SET_PARAM_SEL_PREV,
  SM_SET_PARAM_SET_PREV,
  SM_SET_PARAM_SEL_PREC,
  SM_SET_PARAM_SET_PREC,
  SM_SET_PARAM_BEEPER
};

const struct sm_state_slot code sm_function_set_param[];

extern void do_set_param_fan(unsigned char to_func, unsigned char to_state, enum task_events ev);

#endif
