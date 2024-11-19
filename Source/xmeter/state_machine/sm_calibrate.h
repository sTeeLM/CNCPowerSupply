#ifndef __XMETER_SM_CALIBRATE_H__
#define __XMETER_SM_CALIBRATE_H__

#include "task.h"

#include "task.h"

enum sm_states_calibrate
{
	SM_CALIBRATE_PHY_VOLTAGE,
  SM_CALIBRATE_VOLTAGE_ZERO,
  SM_CALIBRATE_VOLTAGE_MAX,	
  SM_CALIBRATE_CURRENT_ZERO,
  SM_CALIBRATE_CURRENT_MAX,
  SM_CALIBRATE_VOLTAGE_DISS,  
  SM_CALIBRATE_BK,
};

const struct sm_state_slot code sm_function_calibrate[];

extern void do_calibrate_phy_voltage(unsigned char to_func, unsigned char to_state, enum task_events ev);

#endif
