#ifndef __XMETER_COM_H__
#define __XMETER_COM_H__

#include <stdint.h>

void com_initialize (void);
void com_enter_powersave(void);
void com_leave_powersave(void);
char com_try_get_key(void);

bit com_recv_buffer(uint8_t * buffer, uint16_t * len, uint16_t timeoms);
void com_send_buffer(uint8_t * buffer, uint16_t len);

#endif
