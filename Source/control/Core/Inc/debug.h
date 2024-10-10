#ifndef __CONTROL_DEBUG_H__
#define __CONTROL_DEBUG_H__

#include <stdio.h>
#include <stdint.h>

//#include <EventRecorder.h>
#ifdef __cplusplus
extern "C" {
#endif
	
typedef enum {
  CLOG_ERROR,
  CLOG_INFO,
  CLOG_DBG
} clog_type_t;

uint8_t debug_init(void);
void debug_printf(uint8_t newline, clog_type_t level, const char * fmt, ...);

#ifndef C_DEBUG_LEVEL
  #define C_DEBUG_LEVEL 4
#endif

#if (C_DEBUG_LEVEL >= 1)
#define CERR(...)     debug_printf(0, CLOG_ERROR, __VA_ARGS__)
#define CERR_R(...)   debug_printf(1, CLOG_ERROR, __VA_ARGS__)
#define CERR_RH       debug_printf(1, CLOG_ERROR, "[%10d][ERR ] ", HAL_GetTick())
#define CERR_RT       debug_printf(1, CLOG_ERROR, "%s", "\r\n")
#else
#define CERR(...)     do{}while(0)
#define CERR_R(...)   do{}while(0)
#define CERR_RH
#define CERR_RT
#endif

#if (C_DEBUG_LEVEL >= 2)
#define CINFO(...)    debug_printf(0, CLOG_INFO, __VA_ARGS__) 
#define CINFO_R(...)  debug_printf(1, CLOG_INFO, __VA_ARGS__) 
#define CINFO_RH      debug_printf(1, CLOG_INFO, "[%10d][INFO] ", HAL_GetTick())
#define CINFO_RT      debug_printf(1, CLOG_INFO, "%s", "\r\n")
#else
#define CINFO(...)  do{}while(0)
#define CINFO_R(...)  do{}while(0)
#define CINFO_RH
#define CINFO_RT
#endif

#if (C_DEBUG_LEVEL >= 3)
#define CDBG(...)     debug_printf(0, CLOG_DBG, __VA_ARGS__)
#define CDBG_R(...)   debug_printf(1, CLOG_DBG, __VA_ARGS__) 
#define CDBG_RH       debug_printf(1, CLOG_DBG, "[%10d][DBG ] ", HAL_GetTick())
#define CDBG_RT       debug_printf(1, CLOG_DBG, "\r\n")
#else
#define CDBG(...)   do{}while(0)
#define CDBG_R(...) do{}while(0)
#define CDBG_RH
#define CDBG_RT
#endif

#ifdef __cplusplus
}
#endif

#endif