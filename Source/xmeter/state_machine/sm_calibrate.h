#ifndef __XMETER_SM_CALIBRATE_H__
#define __XMETER_SM_CALIBRATE_H__

#include <stdint.h>

#include "task.h"

enum sm_states_calbrate
{
  SM_CALIBRATE_MAIN,    // select calibrate voltage out/current/voltage diss/temp
  SM_CALIBRATE_VAL,  // set val
  SM_CALIBRATE_BITS, // set bits to val
  SM_CALIBRATE_TEMP, // show temp
  SM_CALIBRATE_SAVE, // done
};

const struct sm_state_slot code sm_function_calibrate[];

extern void do_calibrate_main(uint8_t to_func, uint8_t to_state, enum task_events ev);

#endif
