#ifndef __XMETER_CONSOLE_H__
#define __XMETER_CONSOLE_H__

#include <stdint.h>

typedef int8_t (*CONSOLE_CMD_PROC)(char arg1, char arg2);

struct console_cmds
{
  char * cmd;
  char * desc;
  char * usage;
  CONSOLE_CMD_PROC proc;
};


void console_initilize(void);
void console_run(void);
void console_dump_cmd(void);
int8_t console_search_cmd_by_name(char * cmd);
int16_t console_try_get_key(void);
void console_printf(const char * fmt /*format*/, ...);

extern char console_buf[];
extern struct console_cmds cmds[];

int16_t console_get_char(void);
int16_t console_try_get_char(void);
#endif