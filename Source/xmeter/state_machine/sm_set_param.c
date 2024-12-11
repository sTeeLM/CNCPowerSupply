#include "sm_set_param.h"
#include "sm_xmeter.h"
#include "task.h"
#include "sm.h"
#include "cext.h"
#include "xmeter.h"
#include "beeper.h"
#include "lcd.h"
/*   
  Hi +xx.xx C
  Li +xx.xx C
*/
static void sm_set_param_init_fan()
{
  lcd_clear();
  lcd_set_string(0, 0, "T Hi +xx.xx C");
  lcd_set_string(1, 0, "T Lo +xx.xx C");
  lcd_set_special_char(0, 12, LCD_CELSIUS);
  lcd_set_special_char(1, 12, LCD_CELSIUS);
}

static void sm_set_param_fill_fan()
{
  lcd_set_digit(0, 5, &xmeter_temp_hi);
  lcd_set_digit(1, 5, &xmeter_temp_lo);  
}

static void sm_set_param_init_over()
{
  lcd_clear();
  lcd_set_string(0, 0, "T Max +xx.xx C");
  lcd_set_string(1, 0, "Pd Max xx.xx W");
  lcd_set_special_char(0, 13, LCD_CELSIUS);
}

static void sm_set_param_fill_over()
{
  lcd_set_digit(0, 6, &xmeter_temp_overheat);
  lcd_set_digit(1, 6, &xmeter_max_power_diss);
}

static void sm_set_param_init_sel_pre_vc(bit is_c)
{
  lcd_clear();  
  if(is_c) {
    lcd_set_string(0, 0, "C 0.000 0.000 A");
    lcd_set_string(1, 0, "s 0.000 0.000 A");
    xmeter_reset_preset_index_c();
    xmeter_load_preset_dac_c();  
    lcd_set_digit(0, 1, &xmeter_dac_current); 
    xmeter_next_preset_dac_c();
    lcd_set_digit(0, 7, &xmeter_dac_current);
    xmeter_next_preset_dac_c();
    lcd_set_digit(1, 1, &xmeter_dac_current);
    xmeter_next_preset_dac_c();
    lcd_set_digit(1, 7, &xmeter_dac_current);
    xmeter_reset_preset_index_c();
  } else {
    lcd_set_string(0, 0, "V 0.000 0.000 V");
    lcd_set_string(1, 0, "s 0.000 0.000 V");
    xmeter_reset_preset_index_v();
    xmeter_load_preset_dac_v();  
    lcd_set_digit(0, 1, &xmeter_dac_voltage); 
    xmeter_next_preset_dac_v();
    lcd_set_digit(0, 7, &xmeter_dac_voltage);
    xmeter_next_preset_dac_v();
    lcd_set_digit(1, 1, &xmeter_dac_voltage);
    xmeter_next_preset_dac_v();
    lcd_set_digit(1, 7, &xmeter_dac_voltage);
    xmeter_reset_preset_index_v();
  }
}

static void sm_set_param_fill_sel_pre_vc(bit is_c)
{
  uint8_t index;
  if(is_c) {
    index = xmeter_get_preset_index_c();
  } else {
    index = xmeter_get_preset_index_v();
  }
  lcd_set_blink_range(0, 0, 15, 0);
  lcd_set_blink_range(1, 0, 15, 0);
  if(index == 0) {
    lcd_set_blink_range(0, 2, 6, 1); 
  } else if(index == 1){
    lcd_set_blink_range(0, 8, 13, 1);      
  } else if(index == 2){
    lcd_set_blink_range(1, 2, 6, 1);  
  } else {
    lcd_set_blink_range(1, 8, 13, 1);  
  }

}

static void sm_set_param_init_set_pre_vc(bit is_c)
{
  uint8_t index;
  lcd_set_blink_range(0, 0, 15, 0);
  lcd_set_blink_range(1, 0, 15, 0);
  if(is_c) {
    index = xmeter_get_preset_index_c();
    xmeter_load_preset_dac_c();  
  } else {
    index = xmeter_get_preset_index_v();
    xmeter_load_preset_dac_v();  
  }
  if(index == 0) {
    if(is_c)
      lcd_set_digit(0, 1, &xmeter_dac_current); 
    else
      lcd_set_digit(0, 1, &xmeter_dac_voltage);
  } else if(index == 1){
    if(is_c)
      lcd_set_digit(0, 7, &xmeter_dac_current); 
    else
      lcd_set_digit(0, 7, &xmeter_dac_voltage);    
  } else if(index == 2){
    if(is_c)
      lcd_set_digit(1, 1, &xmeter_dac_current); 
    else
      lcd_set_digit(1, 1, &xmeter_dac_voltage);
  } else {
    if(is_c)
      lcd_set_digit(1, 7, &xmeter_dac_current); 
    else
      lcd_set_digit(1, 7, &xmeter_dac_voltage);
  }
}

static void sm_set_param_fill_set_pre_vc(bit is_c)
{
  uint8_t index;
  if(is_c) {
    index = xmeter_get_preset_index_c();
  } else {
    index = xmeter_get_preset_index_v();
  }
  switch(index) {
    case 0:
    if(is_c) {
      lcd_set_digit(0, 1, &xmeter_dac_current); 
    } else {
      lcd_set_digit(0, 1, &xmeter_dac_voltage); 
    }
    break;
    case 1:
    if(is_c) {
      lcd_set_digit(0, 7, &xmeter_dac_current); 
    } else {
      lcd_set_digit(0, 7, &xmeter_dac_voltage); 
    }
    break;
    case 2:
    if(is_c) {
      lcd_set_digit(1, 1, &xmeter_dac_current); 
    } else {
      lcd_set_digit(1, 1, &xmeter_dac_voltage); 
    }
    break;
    case 3:
    if(is_c) {
      lcd_set_digit(1, 7, &xmeter_dac_current); 
    } else {
      lcd_set_digit(1, 7, &xmeter_dac_voltage); 
    }
    break;    
  }
}


static void sm_set_param_init_beeper()
{
  lcd_clear();
  lcd_set_string(0, 0, "Beeper ON ");
}

static void sm_set_param_fill_beeper()
{
  lcd_set_string(0, 7, beeper_get_beep_enable() ? "ON " : "OFF");
}

void do_set_param_fan(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_function != SM_SET_PARAM || sm_cur_state != SM_SET_PARAM_FAN) { /* switch from other function or state */
    xmeter_output_off();
    xmeter_fan_off();
    sm_set_param_init_fan();
    sm_set_param_fill_fan();
    /* save beeper param */
    if(sm_cur_function == SM_SET_PARAM && sm_cur_state == SM_SET_PARAM_BEEPER) {
      if(ev == EV_KEY_SET_PRESS)
        beeper_write_rom_beeper_enable();
    /* save over protect param */
    } else if (sm_cur_function == SM_SET_PARAM && sm_cur_state == SM_SET_PARAM_OVER) {
      if(ev == EV_KEY_SET_PRESS) {
        xmeter_write_rom_max_power_diss();
        xmeter_write_rom_temp_overheat();
      }
    }
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_temp_hi();
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_temp_hi();
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_temp_lo();
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_temp_lo();
  }
  
  sm_set_param_fill_fan();
}

static void do_set_param_over(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_OVER) {
    sm_set_param_init_over();
    sm_set_param_fill_over();
    if(sm_cur_state == SM_SET_PARAM_FAN) {
      xmeter_write_rom_temp_lo();
      xmeter_write_rom_temp_hi();
    }
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_temp_overheat();
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_temp_overheat();
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_max_power_diss();
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_max_power_diss();
  }
  
  sm_set_param_fill_over();
}

static void do_set_param_sel_prev(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_SEL_PREV) {
    sm_set_param_init_sel_pre_vc(0);
    sm_set_param_fill_sel_pre_vc(0);
    
    if(sm_cur_state == SM_SET_PARAM_SET_PREV) {
      xmeter_write_rom_preset_dac_v();
    } else if(sm_cur_state == SM_SET_PARAM_FAN) {
      xmeter_write_rom_temp_lo();
      xmeter_write_rom_temp_hi();
    }
    return;
  }
  
  if (ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    xmeter_next_preset_dac_v();
  } else if (ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    xmeter_prev_preset_dac_v();
  }
  
  sm_set_param_fill_sel_pre_vc(0);
}

static void do_set_param_set_prev(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_SET_PREV) {
    sm_set_param_init_set_pre_vc(0);
    sm_set_param_fill_set_pre_vc(0);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_preset_dac_v(0);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_preset_dac_v(0);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_preset_dac_v(1);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_preset_dac_v(1);
  }

  sm_set_param_fill_set_pre_vc(0);
}

static void do_set_param_sel_prec(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_SEL_PREC) {
    sm_set_param_init_sel_pre_vc(1);
    sm_set_param_fill_sel_pre_vc(1);
    if(sm_cur_state == SM_SET_PARAM_SET_PREC) {
      xmeter_write_rom_preset_dac_c();
    }    
    return;
  }
  
  if (ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    xmeter_next_preset_dac_c();
  } else if (ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    xmeter_prev_preset_dac_c();
  }
  
  sm_set_param_fill_sel_pre_vc(1);  
}

static void do_set_param_set_prec(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_SET_PREC) {
    sm_set_param_init_set_pre_vc(1);
    sm_set_param_fill_set_pre_vc(1);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_preset_dac_c(0);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_preset_dac_c(0);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_preset_dac_c(1);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_preset_dac_c(1);
  }

  sm_set_param_fill_set_pre_vc(1);
}

static void do_set_param_beeper(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_SET_PARAM_BEEPER) {
    sm_set_param_init_beeper();
    sm_set_param_fill_beeper();
    return;
  }
  
  if (ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C 
    || ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    beeper_set_beep_enable(!beeper_get_beep_enable());
  }
  
  sm_set_param_fill_beeper();
}

static const struct sm_trans_slot code  sm_trans_set_param_fan[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_OVER, do_set_param_over},  
  {EV_KEY_MOD_PRESS, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev},  
  {EV_KEY_SET_LPRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},    
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_over[] = {
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan},
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_OVER, do_set_param_over}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_OVER, do_set_param_over},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_OVER, do_set_param_over}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_OVER, do_set_param_over},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_sel_prev[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_SET_PREV, do_set_param_set_prev},  
  {EV_KEY_MOD_PRESS, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec},    
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_set_prev[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_SET_PREV, do_set_param_set_prev}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_SET_PREV, do_set_param_set_prev},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_SET_PREV, do_set_param_set_prev}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_SET_PREV, do_set_param_set_prev},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_SEL_PREV, do_set_param_sel_prev},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_sel_prec[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_SET_PREC, do_set_param_set_prec},  
  {EV_KEY_MOD_PRESS, SM_SET_PARAM, SM_SET_PARAM_BEEPER, do_set_param_beeper},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_set_prec[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_SET_PREC, do_set_param_set_prec}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_SET_PREC, do_set_param_set_prec},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_SET_PREC, do_set_param_set_prec}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_SET_PREC, do_set_param_set_prec},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_SEL_PREC, do_set_param_sel_prec},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_set_param_beeper[] = {
  {EV_KEY_MOD_C, SM_SET_PARAM, SM_SET_PARAM_BEEPER, do_set_param_beeper}, 
  {EV_KEY_MOD_CC, SM_SET_PARAM, SM_SET_PARAM_BEEPER, do_set_param_beeper},
  {EV_KEY_SET_C, SM_SET_PARAM, SM_SET_PARAM_BEEPER, do_set_param_beeper}, 
  {EV_KEY_SET_CC, SM_SET_PARAM, SM_SET_PARAM_BEEPER, do_set_param_beeper},
  {EV_KEY_SET_PRESS, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan},
  {EV_KEY_MOD_PRESS, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan},  
  {NULL, NULL, NULL, NULL}
};

const struct sm_state_slot code sm_function_set_param[] = {
  {"SM_SET_PARAM_FAN", sm_trans_set_param_fan},
  {"SM_SET_PARAM_OVER", sm_trans_set_param_over},
  {"SM_SET_PARAM_SEL_PREV", sm_trans_set_param_sel_prev},
  {"SM_SET_PARAM_SET_PREV", sm_trans_set_param_set_prev},
  {"SM_SET_PARAM_SEL_PREC", sm_trans_set_param_sel_prec},
  {"SM_SET_PARAM_SET_PREC", sm_trans_set_param_set_prec},
  {"SM_SET_PARAM_BEEPER", sm_trans_set_param_beeper},  
};