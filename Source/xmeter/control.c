#include "control.h"
#include "debug.h"
#include "com.h"
#include "clock.h"
#include "delay.h"
#include "../protocol/protocol.h"
#include "xmeter.h"
#include "lcd.h"
#include "sm.h"
#include "cext.h"

#define CONTROL_MAX_IDLE_SEC 10 // second
#define CONTROL_MAX_WAIT_MS  10 // ms

static control_msg_t cmd;
static control_msg_t res;

static uint32_t last_msg_sec;

typedef bool (* control_fun_t)(const control_msg_t * cmd, control_msg_t * res);
 
bool control_do_none(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_hearbeat(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_start_stop_fan(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_start_stop_switch(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_xmeter_status(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_adc_current(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_adc_voltage_in(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_adc_voltage_out(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_adc_temp(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_power_out(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_power_diss(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_dac_voltage(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_dac_current(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_param_temp_hi(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_param_temp_hi(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_param_temp_lo(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_param_temp_lo(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_param_over_heat(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_param_over_heat(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_param_max_power_diss(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_param_max_power_diss(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_param_beep(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_param_beep(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_preset_current(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_preset_current(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_get_preset_voltage(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

bool control_do_set_preset_voltage(const control_msg_t * cmd, control_msg_t * res)
{
  return 0;
}

static control_fun_t code control_funs[CONTROL_MSG_CODE_CNT] =
{
  control_do_none,
  control_do_hearbeat,
  control_do_start_stop_fan,
  control_do_start_stop_switch,
  control_do_get_xmeter_status,
  control_do_get_adc_current,
  control_do_get_adc_voltage_in, 
  control_do_get_adc_voltage_out,
  control_do_get_adc_temp, 
  control_do_get_power_out,
  control_do_get_power_diss,
  control_do_set_dac_voltage,
  control_do_set_dac_current,
  control_do_get_param_temp_hi,
  control_do_set_param_temp_hi,
  control_do_get_param_temp_lo,
  control_do_set_param_temp_lo,
  control_do_get_param_over_heat,
  control_do_set_param_over_heat,
  control_do_get_param_max_power_diss,
  control_do_set_param_max_power_diss, 
  control_do_get_param_beep,
  control_do_set_param_beep,
  control_do_get_preset_current,
  control_do_set_preset_current,
  control_do_get_preset_voltage,
  control_do_set_preset_voltage,  
};

void control_initilize(void)
{
	CDBG(("control_initilize\n"));
}

static void control_enter(void)
{
  debug_onoff(0);
  xmeter_output_on();
  xmeter_fan_off();
  lcd_clear();
  lcd_set_string(0, 0, " REMOTE CONTROL ");
  lcd_refresh();
}

static void control_leave(void)
{
  debug_onoff(1);
  xmeter_output_off();
  xmeter_fan_off();
  lcd_clear();
  lcd_refresh();
  sm_initialize();
}

static void control_run_cmd(const control_msg_t * cmd, control_msg_t * res)
{
  
}

static void control_send_response(const control_msg_t * res)
{
  
}

static void control_do_basic_operation(void)
{
  
}

void control_run(void)
{
  uint16_t len;
  len = sizeof(control_msg_t);
	if(com_recv_buffer((uint8_t *)&cmd, &len, CONTROL_MAX_WAIT_MS)) {
    last_msg_sec = clock_get_now_sec();
    control_enter();
    control_run_cmd(&cmd, &res);
    control_send_response(&res);
    while(1) {
      len = sizeof(control_msg_t);
      if(com_recv_buffer((uint8_t *)&cmd, &len, CONTROL_MAX_WAIT_MS)) {
        control_run_cmd(&cmd, &res);
        control_send_response(&res);
        last_msg_sec = clock_get_now_sec();
      } else {
        if(clock_diff_now_sec(last_msg_sec) > CONTROL_MAX_IDLE_SEC) {
          break;
        }
        control_do_basic_operation();
      }
    }
    control_leave();
  }
}

 
 