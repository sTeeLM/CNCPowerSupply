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

#define CONTROL_MAX_IDLE_SEC 50 // second
#define CONTROL_MAX_WAIT_MS  200 // ms

static control_msg_t control_cmd;
static control_msg_t control_res;

static uint32_t control_last_msg_sec;
static uint32_t control_msg_cnt;
static bit control_is_enter_bit;
static bit control_remote_override;

typedef bool (* control_fun_t)(void);
 
bool control_do_none(void)
{
  return false;
}

static void control_fail_msg(uint8_t msg_status)
{
  control_res.msg_header.msg_code |= 0x80;
  control_res.msg_header.msg_body_len = 0;
  control_res.msg_header.msg_status  = msg_status;
  control_fill_msg_crc(&control_res);
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

static bool control_do_remote_override(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  control_remote_override = control_cmd.msg_body.enable.enable;
  
  control_finish_msg(sizeof(control_msg_body_enable_t));

  return true;
}

static bool control_do_start_stop_fan(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_remote_override) {
    if(control_cmd.msg_body.enable.enable) {
      xmeter_fan_on();
    } else {
      xmeter_fan_off();
    }
  } else {
    control_fail_msg(CONTROL_MSG_STATUS_LOCKED);
  }
  control_res.msg_body.enable.enable = xmeter_fan_status();
  
  control_finish_msg(sizeof(control_msg_body_enable_t));

  return true;
}

static bool control_do_start_stop_switch(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  if(control_remote_override) {
    if(control_cmd.msg_body.enable.enable) {
      xmeter_output_on();
    } else {
      xmeter_output_off();
    }
  } else {
    control_fail_msg(CONTROL_MSG_STATUS_LOCKED);
  }
  
  control_res.msg_body.enable.enable = xmeter_output_status();
  
  control_finish_msg(sizeof(control_msg_body_enable_t));
  
  return true;
}

static bool control_do_get_xmeter_status(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  control_res.msg_body.status.override_on = control_remote_override;
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

static bool control_do_get_adc_voltage_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_assign_value(&xmeter_adc_voltage_diss, &control_res.msg_body.xmeter.xmeter_val);
  
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

bool control_do_get_limits_current(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_current_limits(&control_res.msg_body.limits.min, &control_res.msg_body.limits.max);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_limits_voltage_out(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_voltage_out_limits(&control_res.msg_body.limits.min, &control_res.msg_body.limits.max);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_limits_temp(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_temp_limits(&control_res.msg_body.limits.min, &control_res.msg_body.limits.max);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_limits_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_get_power_diss_limits(&control_res.msg_body.limits.min, &control_res.msg_body.limits.max);
  
  control_finish_msg(sizeof(control_msg_body_steps_t));
  return true;
}

bool control_do_get_param_temp_hi(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  memset(&control_res.msg_body, 0, sizeof(control_msg_body_t));
  
  xmeter_assign_value(&xmeter_temp_hi, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_temp_hi(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_hi(&control_res.msg_body.param.xmeter_val);
  xmeter_assign_value(&xmeter_temp_hi, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_hi();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_temp_lo(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  memset(&control_res.msg_body, 0, sizeof(control_msg_body_t));
  
  xmeter_assign_value(&xmeter_temp_lo, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_temp_lo(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_lo(&control_res.msg_body.param.xmeter_val);
  xmeter_assign_value(&xmeter_temp_lo, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_lo();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_over_heat(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  memset(&control_res.msg_body, 0, sizeof(control_msg_body_t));
  
  xmeter_assign_value(&xmeter_temp_overheat, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_over_heat(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_temp_overheat(&control_res.msg_body.param.xmeter_val);
  xmeter_assign_value(&xmeter_temp_overheat, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_temp_overheat();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_max_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  memset(&control_res.msg_body, 0, sizeof(control_msg_body_t));
  
  xmeter_assign_value(&xmeter_max_power_diss, &control_res.msg_body.param.xmeter_val);
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_set_param_max_power_diss(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  xmeter_set_max_power_diss(&control_res.msg_body.param.xmeter_val);
  xmeter_assign_value(&xmeter_max_power_diss, &control_res.msg_body.param.xmeter_val);
  xmeter_write_rom_max_power_diss();
  
  control_finish_msg(sizeof(control_msg_body_param_t));
  return true;
}

bool control_do_get_param_beep(void)
{
  memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
  
  memset(&control_res.msg_body, 0, sizeof(control_msg_body_t));
  
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
  control_do_remote_override,
  control_do_start_stop_fan,
  control_do_start_stop_switch,
  control_do_get_xmeter_status,
  control_do_get_adc_current,
  control_do_get_adc_voltage_diss, 
  control_do_get_adc_voltage_out,
  control_do_get_adc_temp, 
  control_do_get_power_out,
  control_do_get_power_diss,
  control_do_get_dac_current,
  control_do_set_dac_current,  
  control_do_get_dac_voltage,
  control_do_set_dac_voltage,
  control_do_get_steps_current,  
  control_do_get_steps_voltage,
  control_do_get_steps_temp,
  control_do_get_steps_power,
  control_do_get_limits_current,
  control_do_get_limits_voltage_out,
  control_do_get_limits_temp,
  control_do_get_limits_power_diss,
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
  CDBG("control_initilize, sizeof(control_msg_t) is %bu\n", sizeof(control_msg_t));
  control_is_enter_bit = 0;
  control_remote_override = 0;
}

static bit control_is_enter(void)
{
  return control_is_enter_bit;
}

static void control_enter(void)
{
  if(!control_is_enter_bit) {
    CDBG("control_enter\n");
    xmeter_output_on();
    xmeter_fan_off();
    lcd_enter_control();
    sm_enter_control();
    control_is_enter_bit = 1;
    control_remote_override = 0;
    control_last_msg_sec = clock_get_now_sec();
    control_msg_cnt = 0;
  }
}

static void control_leave(void)
{
  if(control_is_enter_bit) {
    xmeter_output_off();
    xmeter_fan_off();
    lcd_leave_control();
    sm_leave_control();
    control_is_enter_bit = 0;
    CDBG("control_leave\n");
  }
}

static void control_run_cmd(void)
{
  control_fun_t fun;
  
  if(!control_verify_msg(&control_cmd, sizeof(control_msg_t))) {
    memcpy(&control_res, &control_cmd, sizeof(control_msg_t));
    control_res.msg_header.msg_magic = CONTROL_MSG_MAGIC_CODE;   
    control_res.msg_header.msg_code |= 0x80;
    control_res.msg_header.msg_status  = CONTROL_MSG_STATUS_PROTO;
    /* channel not change */
    control_res.msg_header.msg_body_len = 0;
    control_fill_msg_crc(&control_res);
  } else {
    fun = control_funs[control_cmd.msg_header.msg_code];
    fun();
    if(control_cmd.msg_header.msg_code != CONTROL_MSG_CODE_HEATBEAT) {
      control_msg_cnt ++;
    }
  }
}

static void control_send_response(void)
{
  com_send_buffer((uint8_t *)&control_res, sizeof(control_msg_t));
}

static void control_do_basic_operation(void)
{
  xmeter_read_adc();
  
  if(!control_remote_override) {
  
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
}
static void control_update_lcd(uint32_t msg_cnt, uint32_t idle_sec)
{
  lcd_set_integer(1, 4, 4, msg_cnt);
  lcd_set_integer(1, 14, 2, idle_sec);
  lcd_refresh();
}

void control_run(void)
{
  uint16_t len;
  uint32_t diff_sec;
  while(1) {
    len = sizeof(control_msg_t);
    if(com_recv_buffer((uint8_t *)&control_cmd, &len, CONTROL_MAX_WAIT_MS)) {
      control_enter();
      control_run_cmd();
      control_send_response();
      control_last_msg_sec = clock_get_now_sec();
    } else {
      if(control_is_enter()) {
        diff_sec = clock_diff_now_sec(control_last_msg_sec);
        if(diff_sec > CONTROL_MAX_IDLE_SEC) {
          control_leave();
          break;
        }
        control_do_basic_operation();
      } else {
        break;
      }
    }
    control_update_lcd(control_msg_cnt, diff_sec);
  }
}

 
 