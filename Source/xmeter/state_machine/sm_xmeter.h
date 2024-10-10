#ifndef __XMETER_SM_XMETER_H__
#define __XMETER_SM_XMETER_H__

#include <stdint.h>

#include "task.h"

enum sm_states_xmeter
{
  SM_XMETER_MAIN,
  SM_XMETER_AUX0,
  SM_XMETER_AUX1,	
  SM_XMETER_PICK_V,
  SM_XMETER_PICK_C,	
  SM_XMETER_SET_V,
  SM_XMETER_SET_C,		
  SM_XMETER_OVER_HEAT,
  SM_XMETER_OVER_PD,
};

const struct sm_state_slot code sm_function_xmeter[];

extern void do_xmeter_main(uint8_t to_func, uint8_t to_state, enum task_events ev);

#endif
