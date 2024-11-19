#include "sm_xmeter.h"
#include "sm_set_param.h"
#include "sm_calibrate.h"
#include "task.h"
#include "sm.h"
#include "cext.h"
#include "lcd.h"
#include "xmeter.h"
#include "debug.h"
#include "clock.h"
/*
over diss power screen (关闭输出，直到手工按set置位)
 OVER DP
Press Set

over head screen (关闭输出，直到手工按set置位)
T: +xx.xx C
 OVER HEAT
*/

static uint32_t sm_xmeter_now;

#define SM_XMETER_MAX_IDLE_SEC 30

static void sm_xmeter_reset_timeo()
{
  sm_xmeter_now = clock_get_now_sec();
}

static void sm_xmeter_test_timeo()
{
  if(clock_diff_now_sec(sm_xmeter_now) > SM_XMETER_MAX_IDLE_SEC)
    task_set(EV_TIMEO);
}
/*
main screen (normal)
V xx.xx V Po  W
C xx.xx A xx.xx
*/
static void sm_xmeter_init_main()
{
  lcd_clear();
  lcd_set_string(0, 0, "V 0.000 V Po  W");
  lcd_set_string(1, 0, "C 0.000 A 0.000"); 
}

static void sm_xmeter_fill_main()
{
  lcd_set_char(0, 1, xmeter_adc_voltage_out.neg ? '-' : ' ');
  lcd_set_digit(0, 2, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);
  
  lcd_set_char(1, 1, xmeter_adc_current.neg ? '-' : ' ');
  lcd_set_digit(1, 2, xmeter_adc_current.integer, xmeter_adc_current.decimal); 
  
  lcd_set_digit(1, 10, xmeter_power_out.integer, xmeter_power_out.decimal);   
}  

/*
aux0 screen
Vi xx.xx V
Pd xx.xx W

aux1 screen
T +xx.xx C
Fan  OFF

*/
static void sm_xmeter_init_aux(uint8_t index)
{
  lcd_clear();
  if(index == 0) {
    lcd_set_string(0, 0, "Vd 0.000 V");
    lcd_set_string(1, 0, "Pd 0.000 W");   
  } else {
    lcd_set_string(0, 0, "T +xx.xx C ");
    lcd_set_string(1, 0, "Fan OFF    ");
    lcd_set_special_char(0, 9, LCD_CELSIUS);
  }
}

static void sm_xmeter_fill_aux(uint8_t index)
{
  if(index == 0) {
    lcd_set_char(0, 2, xmeter_adc_voltage_diss.neg ? '-' : ' ');
    lcd_set_digit(0, 3, xmeter_adc_voltage_diss.integer, xmeter_adc_voltage_diss.decimal); 
    lcd_set_digit(1, 3, xmeter_power_diss.integer, xmeter_power_diss.decimal);
  } else {
    lcd_set_char(0, 2, xmeter_adc_temp.neg ? '-' : '+');
    lcd_set_digit(0, 3, xmeter_adc_temp.integer, xmeter_adc_temp.decimal);  
    lcd_set_string(1, 4, xmeter_fan_status() ? "ON " : "OFF"); 
  }
} 

/*
pick screen
V xx.xx xx.xx V
  xx.xx xx.xx

C xx.xx xx.xx A
  xx.xx xx.xx
*/

void sm_xmeter_init_pick(bit is_c)
{
  lcd_clear();  
  if(is_c) {
    lcd_set_string(0, 0, "C 0.000 0.000 A");
    lcd_set_string(1, 0, "  0.000 0.000 A");
    xmeter_reset_preset_index_c();
    xmeter_load_preset_dac_c();  
    lcd_set_digit(0, 2, xmeter_dac_current.integer, xmeter_dac_current.decimal); 
    xmeter_next_preset_dac_c();
    lcd_set_digit(0, 8, xmeter_dac_current.integer, xmeter_dac_current.decimal);
    xmeter_next_preset_dac_c();
    lcd_set_digit(1, 2, xmeter_dac_current.integer, xmeter_dac_current.decimal);
    xmeter_next_preset_dac_c();
    lcd_set_digit(1, 8, xmeter_dac_current.integer, xmeter_dac_current.decimal);
    xmeter_reset_preset_index_c();
  } else {
    lcd_set_string(0, 0, "V 0.000 0.000 V");
    lcd_set_string(1, 0, "  0.000 0.000 V");
    xmeter_reset_preset_index_v();
    xmeter_load_preset_dac_v();  
    lcd_set_digit(0, 2, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal); 
    xmeter_next_preset_dac_v();
    lcd_set_digit(0, 8, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal);
    xmeter_next_preset_dac_v();
    lcd_set_digit(1, 2, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal);
    xmeter_next_preset_dac_v();
    lcd_set_digit(1, 8, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal);
    xmeter_reset_preset_index_v();
  }
}

void sm_xmeter_fill_pick(bit is_c)
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

/*
V xx.xx V Pmax W
C xx.xx A xx.xx
*/

void sm_xmeter_init_set(bit is_c)
{
  lcd_clear();
  lcd_set_string(0, 0, "V 0.000 V Pmax W");
  lcd_set_string(1, 0, "C 0.000 A 0.000"); 
}

/* 
注意，这个函数展示的DAC的数值，而不是ADC的数值，不要弄错！ 
功率展示的是如果在这个设置下的最大输出功率，不是实际输出功率
*/
void sm_xmeter_fill_set(bit is_c)
{
  struct xmeter_value power_max_out;
  xmeter_calculate_power_out(&xmeter_dac_current, &xmeter_dac_voltage, &power_max_out);
  if(xmeter_cc_status()) {
    lcd_set_digit(0, 2, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal);
    lcd_set_digit(1, 2, xmeter_adc_current.integer, xmeter_adc_current.decimal);  
  } else {
    lcd_set_digit(0, 2, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);
    lcd_set_digit(1, 2, xmeter_dac_current.integer, xmeter_dac_current.decimal);  
  }
  lcd_set_digit(1, 10, power_max_out.integer, power_max_out.decimal);
}


//
// 250ms 刷新
// aux0 / aux1 / pick_c / pick_v / set_c / set_v / over_heat / over_pd 切换过来
// 从其他外部功能切换过来
// 初始启动

void do_xmeter_main(uint8_t to_func, uint8_t to_state, enum task_events ev)
{ 
  /* 从内外部切换过来或者启动 */
  if((sm_cur_function == SM_XMETER && sm_cur_state == SM_XMETER_MAIN && ev == EV_INIT)
    || (sm_cur_function == SM_XMETER && sm_cur_state != SM_XMETER_MAIN)
    || (sm_cur_function != SM_XMETER)) {  
      
      if(sm_cur_function == SM_CALIBRATE && sm_cur_state == SM_CALIBRATE_BK) {
        lcd_clear();
        lcd_set_string(0, 0, "Reloading config");
        lcd_set_string(1, 0, "Please Wait.....");
        lcd_refresh();
        xmeter_load_config(); /* reload config after calibtate*/
      }      
      
      xmeter_output_on();
      xmeter_fan_off();
      xmeter_read_adc();
      sm_xmeter_init_main();
      sm_xmeter_fill_main();
      
      if(sm_cur_function == SM_XMETER && (sm_cur_state == SM_XMETER_PICK_V)) {
          if(ev != EV_TIMEO) {
            xmeter_write_dac_voltage();
            xmeter_write_rom_dac_voltage();
          } else {
            xmeter_read_dac_voltage();
          }
      } else if(sm_cur_function == SM_XMETER && (sm_cur_state == SM_XMETER_PICK_C)) {
          if(ev != EV_TIMEO) {
            xmeter_write_dac_current();
            xmeter_write_rom_dac_current();
          } else {
            xmeter_read_dac_current();
          }
      }
  }
    
  if(ev == EV_250MS) {
    xmeter_read_adc();
    sm_xmeter_fill_main();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  }
}

static void do_xmeter_aux(uint8_t to_func, uint8_t to_state, enum task_events ev, uint8_t index)
{
  if(sm_cur_state != to_state) {
    xmeter_read_adc();
    sm_xmeter_init_aux(index);
    sm_xmeter_fill_aux(index);
    sm_xmeter_reset_timeo();
    return;
  }
  
  if(ev == EV_250MS) {
    xmeter_read_adc();
    sm_xmeter_fill_aux(index);
    sm_xmeter_test_timeo();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  }
}

static void do_xmeter_aux0(uint8_t to_func, uint8_t to_state, enum task_events ev)
{ 
  do_xmeter_aux(to_func, to_state, ev, 0);
}

static void do_xmeter_aux1(uint8_t to_func, uint8_t to_state, enum task_events ev)
{ 
  do_xmeter_aux(to_func, to_state, ev, 1);
}

static void do_xmeter_pick(uint8_t to_func, uint8_t to_state, enum task_events ev, bit is_c)
{
  if(sm_cur_state != to_state) {
    sm_xmeter_init_pick(is_c);
    sm_xmeter_fill_pick(is_c);
    sm_xmeter_reset_timeo();
    return;
  }
  
  if(ev == EV_250MS) {
    xmeter_read_adc();
    sm_xmeter_fill_pick(is_c);
    sm_xmeter_test_timeo();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  } else if(ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    sm_xmeter_reset_timeo();
    if(!is_c)
      xmeter_next_preset_dac_v();
    else
      xmeter_next_preset_dac_c();
  } else if(ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    sm_xmeter_reset_timeo();
    if(!is_c)
      xmeter_prev_preset_dac_v();
    else
      xmeter_prev_preset_dac_c();
  }  
}

static void do_xmeter_pick_v(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  do_xmeter_pick(to_func, to_state, ev, 0);
}


static void do_xmeter_pick_c(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  do_xmeter_pick(to_func, to_state, ev, 1);
}

static void do_xmeter_set(uint8_t to_func, uint8_t to_state, enum task_events ev, bit is_c)
{
  if(sm_cur_state != to_state) {
    if(!is_c)
      xmeter_read_dac_voltage();
    else
      xmeter_read_dac_current();
    sm_xmeter_init_set(is_c);
    sm_xmeter_fill_set(is_c);
    sm_xmeter_reset_timeo();
    return;
  }
  if(ev == EV_250MS) {
    xmeter_read_adc();
    sm_xmeter_fill_set(is_c);
    sm_xmeter_test_timeo();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  } else if(ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    if(!is_c) {
      xmeter_inc_dac_v(ev == EV_KEY_MOD_C);
      xmeter_write_dac_voltage();
    } else {
      xmeter_inc_dac_c(ev == EV_KEY_MOD_C);
      xmeter_write_dac_current();
    }
  } else if(ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    if(!is_c) {
      xmeter_dec_dac_v(ev == EV_KEY_MOD_CC);
      xmeter_write_dac_voltage();
    } else {
      xmeter_dec_dac_c(ev == EV_KEY_MOD_CC);
      xmeter_write_dac_current();
    }
  }  
}

static void do_xmeter_set_v(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  do_xmeter_set(to_func, to_state, ev, 0);
}


static void do_xmeter_set_c(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  do_xmeter_set(to_func, to_state, ev, 1);
}

static void do_xmeter_overheat(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_state != to_state) {
    lcd_clear();
    lcd_set_string(0, 0, "   OVER HEAT");
    lcd_set_string(1, 0, "COOLING  xx.xx C");
    lcd_set_special_char(1, 15, LCD_CELSIUS);
    
    lcd_set_blink_range(1, 0, 6, true);
    xmeter_output_off();
    xmeter_fan_on();
  } else if(ev == EV_250MS){
    xmeter_read_adc();
    if(xmeter_temp_safe()) {
      lcd_set_blink_range(1, 0, 15, true);
      lcd_set_string(1, 0, "   Press Set    ");
    } else {
      lcd_set_char(1, 8, xmeter_adc_temp.neg ? '-' : '+');
      lcd_set_digit(1, 9, xmeter_adc_temp.integer, xmeter_adc_temp.decimal); 
    }
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  } else if(ev == EV_KEY_SET_PRESS) {
    /* 比较adc的温度读数，小于安全温度，发送 EV_TIMEO */
    if(xmeter_temp_safe()) {
      task_set(EV_TIMEO);
    }
  }
}

static void do_xmeter_overpd(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_state != to_state) {
    lcd_clear();
    lcd_set_string(0, 0, "OVER POWER DISSI");
    lcd_set_string(1, 0, "   Press Set");
    lcd_set_blink_range(1, 0, 16, true);
    xmeter_output_off();
    xmeter_fan_on();
  } else if(ev == EV_250MS){
    xmeter_read_adc();
  } else if(ev == EV_TEMP_LO) {
    xmeter_fan_off();
  } else if(ev == EV_TEMP_HI) {
    xmeter_fan_on();
  }
}

static const struct sm_trans_slot code sm_trans_xmeter_main[] = {
  {EV_INIT, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_250MS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v}, 
  {EV_KEY_SET_PRESS, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c},   
  {EV_KEY_MOD_LPRESS, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0}, 
  {EV_KEY_SET_LPRESS, SM_SET_PARAM, SM_SET_PARAM_FAN, do_set_param_fan}, /* 进入参数设置 */
  {EV_KEY_MOD_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage}, /* 进入校准功能 */  
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_aux0[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main}, 
  
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1},
  
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_aux1[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main}, 
  
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_AUX0, do_xmeter_aux0},
  
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_AUX1, do_xmeter_aux1},
  {NULL, NULL, NULL, NULL}
};


static const struct sm_trans_slot code  sm_trans_xmeter_pick_v[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v}, 
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},   
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_PICK_V, do_xmeter_pick_v},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_pick_c[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c},
  {EV_KEY_SET_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main}, 
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_PICK_C, do_xmeter_pick_c},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_set_v[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_SET_V, do_xmeter_set_v},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_set_c[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_KEY_MOD_C, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c}, 
  {EV_KEY_MOD_CC, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  {EV_KEY_SET_C, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c}, 
  {EV_KEY_SET_CC, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  {EV_KEY_SET_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {EV_OVER_HEAT, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_OVER_PD, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_SET_C, do_xmeter_set_c},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_overpd[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_OVER_PD, do_xmeter_overpd}, // refresh
  {EV_KEY_SET_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main}, // switch to main display
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_OVER_PD, do_xmeter_overpd},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_OVER_PD, do_xmeter_overpd},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_xmeter_overheat[] = {
  {EV_250MS, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat}, // refresh
  {EV_KEY_SET_PRESS, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat}, // switch to main display
  {EV_TEMP_LO, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},
  {EV_TEMP_HI, SM_XMETER, SM_XMETER_OVER_HEAT, do_xmeter_overheat},  
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},
  {NULL, NULL, NULL, NULL}
};

const struct sm_state_slot code sm_function_xmeter[] = {
  {"SM_XMETER_MAIN", sm_trans_xmeter_main},
  {"SM_XMETER_AUX0", sm_trans_xmeter_aux0}, 
  {"SM_XMETER_AUX1", sm_trans_xmeter_aux1},   
  {"SM_XMETER_PICK_V", sm_trans_xmeter_pick_v},
  {"SM_XMETER_PICK_C", sm_trans_xmeter_pick_c}, 
  {"SM_XMETER_SET_V", sm_trans_xmeter_set_v},
  {"SM_XMETER_SET_C", sm_trans_xmeter_set_c},  
  {"SM_XMETER_OVER_HEAT", sm_trans_xmeter_overheat},
  {"SM_XMETER_OVER_PD", sm_trans_xmeter_overpd},
};