#ifndef __XMETER_BUTTON_H__
#define __XMETER_BUTTON_H__

#include "cext.h"
#include "task.h"

void button_initialize(void);
void button_scan_proc(enum task_events ev);
void button_mod_proc(enum task_events ev);
void button_set_proc(enum task_events ev);
void button_mod_set_proc(enum task_events ev);
bool button_is_factory_reset(void);

#endif
