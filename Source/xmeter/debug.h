#ifndef __XMETER_DEBUG_H__
#define __XMETER_DEBUG_H__

#include <stdio.h>

void debug_onoff(bit enable);
void debug_initialize(void);


#ifdef __XMETER_DEBUG__
void debug_printf(const char * fmt, ...);
#define CDBG debug_printf
#else
#define CDBG
#endif

void debug_printf(const char * fmt, ...);

//#define CDBG debug_printf

#endif
