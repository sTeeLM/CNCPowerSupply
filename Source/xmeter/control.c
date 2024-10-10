#include "control.h"
#include "debug.h"
#include "com.h"
#include "clock.h"
#include "delay.h"
#include "protocol.h"
#include "xmeter.h"
#include "lcd.h"
#include "sm.h"
#include "cext.h"
#include "beeper.h"

#include <string.h>

#define CONTROL_MAX_IDLE_SEC 10 // second
#define CONTROL_MAX_WAIT_MS  10 // ms

static control_msg_t control_cmd;
static control_msg_t control_res;

static uint32_t last_msg_sec;

typedef bool (* control_fun_t)(void);
 
bool control_do_none(void)
{
  return false;
}

static void control_finish_msg(uint16_t msg_body_len)
{
  control_res.msg_header.msg_code |= 0x80;
  control_res.msg_header.msg_body_len = msg_body_len;
  control_res.msg_header.msg_status  = CONTROL_MSG_STATUS_SUCCESS;
  control_fill_msg_crc(&control_res);
}

static bool control_do_hearbeat(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_read_adc();
  
  control_finish_msg(0);

  return true;
}

static bool control_do_start_stop_fan(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_cmd.msg_body.enable.enable) {
    xmeter_fan_on();
  } else {
    xmeter_fan_off();
  }
  control_res.msg_body.enable.enable = xmeter_fan_status();
  
  control_finish_msg(sizeof(control_msg_body_enable_t));

  return true;
}

static bool control_do_start_stop_switch(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_cmd.msg_body.enable.enable) {
    xmeter_output_on();
  } else {
    xmeter_output_off();
  }

  control_res.msg_body.enable.enable = xmeter_output_status();
  
  control_finish_msg(sizeof(control_msg_body_enable_t));
  
  return true;
}

static bool control_do_get_xmeter_status(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  control_res.msg_body.status.fan_on = xmeter_fan_status();
  control_res.msg_body.status.switch_on = xmeter_output_status();  
  control_res.msg_body.status.cc_on = xmeter_cc_status();  
  
  control_finish_msg(sizeof(control_msg_body_status_t));
  return true;
}

static bool control_do_get_adc_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_adc_current, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_adc_voltage_in(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_adc_voltage_in, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

bool control_do_get_adc_voltage_out(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_adc_voltage_out, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_adc_temp(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_adc_temp, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_power_out(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_power_out, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_power_diss, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_dac_voltage(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_read_dac_voltage();
  xmeter_assign_value(&xmeter_dac_voltage, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_set_dac_voltage(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_dac_v(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_dac_voltage, &control_res.msg_body.xmeter.xmeter_val);
  xmeter_write_dac_voltage();
  xmeter_write_rom_dac_voltage();
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_get_dac_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_read_dac_current();
  xmeter_assign_value(&xmeter_dac_current, &control_res.msg_body.xmeter.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

static bool control_do_set_dac_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_dac_c(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_dac_current, &control_res.msg_body.xmeter.xmeter_val);
  xmeter_write_dac_current();
  xmeter_write_rom_dac_current();
  
  control_finish_msg(sizeof(control_msg_body_xmeter_t));
  return true;
}

bool control_do_get_steps_voltage(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_voltage_steps(&control_res.msg_body.steps.coarse, &control_res.msg_body.steps.fine);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_steps_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_current_steps(&control_res.msg_body.steps.coarse, &control_res.msg_body.steps.fine);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_steps_temp(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_temp_steps(&control_res.msg_body.steps.coarse, &control_res.msg_body.steps.fine);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_steps_power(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_power_steps(&control_res.msg_body.steps.coarse, &control_res.msg_body.steps.fine);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_param_temp_hi(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_temp_hi, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_temp_hi(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_hi(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_temp_hi, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_hi();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_temp_lo(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_temp_lo, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_temp_lo(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_lo(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_temp_lo, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_lo();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_over_heat(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_temp_overheat, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_over_heat(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_overheat(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_temp_overheat, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_overheat();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_max_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_max_power_diss, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_max_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_max_power_diss(&control_res.msg_body.xmeter.xmeter_val);
  xmeter_assign_value(&xmeter_max_power_diss, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_max_power_diss();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_beep(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  control_res.msg_body.param.uint8_val = beeper_get_beep_enable();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_beep(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  beeper_set_beep_enable(control_res.msg_body.param.uint8_val);
  control_res.msg_body.param.uint8_val = beeper_get_beep_enable();
  beeper_write_rom_beeper_enable();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_preset_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_res.msg_body.preset.preset_index > xmeter_get_max_preset_index_c()) {
    control_res.msg_body.preset.preset_index = xmeter_get_max_preset_index_c();
  }  
  
  xmeter_load_preset_dac_c_by_index(control_res.msg_body.preset.preset_index);
  xmeter_assign_value(&xmeter_dac_current, &control_res.msg_body.preset.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_preset_t));
  return true;
}

bool control_do_set_preset_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_res.msg_body.preset.preset_index > xmeter_get_max_preset_index_c()) {
    control_res.msg_body.preset.preset_index = xmeter_get_max_preset_index_c();
  }  
  
  xmeter_set_dac_c(&control_res.msg_body.preset.xmeter_val);
  xmeter_store_preset_dac_c(control_res.msg_body.preset.preset_index);
  xmeter_assign_value(&xmeter_dac_current, &control_res.msg_body.preset.xmeter_val);
  xmeter_write_rom_preset_dac_c();
  
  control_finish_msg(sizeof(control_msg_body_preset_t));
  return true;
}

bool control_do_get_preset_voltage(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_res.msg_body.preset.preset_index > xmeter_get_max_preset_index_v()) {
    control_res.msg_body.preset.preset_index = xmeter_get_max_preset_index_v();
  }  
  
  xmeter_load_preset_dac_v_by_index(control_res.msg_body.preset.preset_index);
  xmeter_assign_value(&xmeter_dac_voltage, &control_res.msg_body.preset.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_preset_t));
  return true;
}

bool control_do_set_preset_voltage(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_res.msg_body.preset.preset_index > xmeter_get_max_preset_index_v()) {
    control_res.msg_body.preset.preset_index = xmeter_get_max_preset_index_v();
  }    
  
  xmeter_set_dac_v(&control_res.msg_body.preset.xmeter_val);
  xmeter_store_preset_dac_v(control_res.msg_body.preset.preset_index);
  xmeter_assign_value(&xmeter_dac_voltage, &control_res.msg_body.preset.xmeter_val);
  xmeter_write_rom_preset_dac_v();
  
  control_finish_msg(sizeof(control_msg_body_preset_t));
  return true;
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
  control_do_get_dac_voltage,
  control_do_set_dac_voltage,
  control_do_get_dac_current,
  control_do_set_dac_current,
  control_do_get_steps_voltage,
  control_do_get_steps_current,
  control_do_get_steps_temp,
  control_do_get_steps_power,
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

static void control_run_cmd(void)
{
  control_fun_t fun;
  
  if(!control_verify_msg(&control_res, sizeof(control_msg_t))) {
    return;
  }
  
  fun = control_funs[control_res.msg_header.msg_code];
  
  fun();
}

static void control_send_response(void)
{
  com_send_buffer((uint8_t *)&control_res, sizeof(control_msg_t));
}

static void control_do_basic_operation(void)
{
  xmeter_read_adc();
  
  if(task_test(EV_TEMP_LO)) {
    xmeter_fan_off();
    task_clr(EV_TEMP_LO);
  }
  
  if(task_test(EV_TEMP_HI)) {
    xmeter_fan_on();
    task_clr(EV_TEMP_HI);
  }
  
  if(task_test(EV_OVER_HEAT)) {
    xmeter_fan_on();
    xmeter_output_off();
    task_clr(EV_OVER_HEAT);
  }  
  
  if(task_test(EV_OVER_PD)) {
    xmeter_fan_on();
    xmeter_output_off();
    task_clr(EV_OVER_PD);
  }   
}

void control_run(void)
{
  uint16_t len;
  len = sizeof(control_msg_t);
	if(com_recv_buffer((uint8_t *)&control_cmd, &len, CONTROL_MAX_WAIT_MS)) {
    last_msg_sec = clock_get_now_sec();
    control_enter();
    control_run_cmd();
    control_send_response();
    while(1) {
      len = sizeof(control_msg_t);
      if(com_recv_buffer((uint8_t *)&control_cmd, &len, CONTROL_MAX_WAIT_MS)) {
        control_run_cmd();
        control_send_response();
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

 
 