#ifndef __CONTROL_H__
#define __CONTROL_H__

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif
	
bool control_parse_cmd(control_msg_t * cmd, const uint8_t * buffer, uint32_t len);
void control_relay_cmd(const control_msg_t * cmd, control_msg_t * res);
bool control_send_response(const control_msg_t * res);
void control_dump_msg(const char * fmt, const control_msg_t * msg);
void control_send_heatbeat(control_msg_t * msg, control_msg_t * res, uint8_t index);
#ifdef __cplusplus
}
#endif

#endif