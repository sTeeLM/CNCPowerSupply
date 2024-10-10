#include "sm.h"

#include "clock.h"
#include "debug.h"

#include "sm_xmeter.h"
#include "sm_set_param.h"
#include "sm_calibrate.h"

uint8_t sm_cur_function;
uint8_t sm_cur_state;

static const struct sm_function_slot code sm_function[] =
{
  {"SM_XMETER", sm_function_xmeter},
  {"SM_SET_PARAM", sm_function_set_param},
  {"SM_CALIBRATE", sm_function_calibrate}  
};

uint8_t sm_cur_function;
uint8_t sm_cur_state;

void sm_initialize(void)
{
  CDBG(("sm_initialize\n"));
  sm_cur_function = SM_XMETER;
  sm_cur_state = SM_XMETER_MAIN;
  task_set(EV_INIT);
}

void sm_run(enum task_events ev)
{
  struct sm_function_slot * p_function;
  struct sm_state_slot * p_state;  
  struct sm_trans_slot * p_trans;
  uint8_t to_function, to_state;
  
  p_function = sm_function + sm_cur_function;
  
  p_state = p_function->function_states_array + sm_cur_state;
  
  p_trans = p_state->state_trans_array;
  
  while(p_trans && p_trans->sm_proc) {
    if(p_trans->event == ev) {
      to_function = p_trans->to_function;
      to_state    = p_trans->to_state;
      CDBG("[%s][%s][%s]->[%s][%s]\n",
        task_names[ev],
        p_function->function_name,
        p_state->state_name,
        sm_function[to_function].function_name,
        sm_function[to_function].function_states_array[to_state].state_name
      );
      p_trans->sm_proc(to_function, to_state, ev);
      sm_cur_function = to_function;
      sm_cur_state    = to_state;
      break;
    }
    p_trans ++;
  }
}

