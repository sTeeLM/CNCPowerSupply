#ifndef __XMETER_BEEPER_H__
#define __XMETER_BEEPER_H__

void beeper_initialize (void);

void beeper_beep(void);
void beeper_beep_beep(void);
void beeper_beep_beep_always(void);
void beeper_set_beep_enable(bit enable);
bit beeper_get_beep_enable(void);
void beeper_write_rom_beeper_enable(void);

#endif
