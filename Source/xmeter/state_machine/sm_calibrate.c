#include "sm_calibrate.h"
#include "sm_xmeter.h"
#include "task.h"
#include "sm.h"
#include "cext.h"
#include "lcd.h"
#include "xmeter.h"
#include "debug.h"
#include "clock.h"
#include "cal_common.h"
#include "protocol.h"

typedef enum _calibrate_type_t {
  CALIBRATE_VO = 0,
  CALIBRATE_C,
  CALIBRATE_VD,  
  CALIBRATE_T, 
  CALIBRATE_CNT
}calibrate_type_t;

static uint8_t calibrate_type;
static uint8_t calibrate_index;

/*
VOL_OUT VOL_DISS
CURRENT TEMP
*/
static void calibrate_init_main(void)
{
  lcd_clear();
  lcd_set_string(0, 0, "VOL_OUT");
  lcd_set_string(0, 8, "VOL_DISS");
  lcd_set_string(1, 0, "CURRENT");
  lcd_set_string(1, 8, "TEMP    ");  
}

static void calibrate_fill_main(void)
{
  lcd_set_blink_range(0, 0, 15, 0);
  lcd_set_blink_range(1, 0, 15, 0);
  switch(calibrate_type){
    case CALIBRATE_VO:
      lcd_set_blink_range(0, 0, 7, 1);
      break;
    case CALIBRATE_C:
      lcd_set_blink_range(1, 0, 7, 1);
      break;
    case CALIBRATE_VD:
      lcd_set_blink_range(0, 8, 15, 1);
      break;
    case CALIBRATE_T:
      lcd_set_blink_range(1, 8, 15, 1);
      break;      
  }
}
/*
XXXXXXXXX A xxxx
1/6-xx.xx D xxxx

*/
static void calibrate_init_val_bits(bit is_bits)
{
  xmeter_value_t value;
  uint8_t res;
  
  lcd_clear();
  switch(calibrate_type){
    case CALIBRATE_VO:
      lcd_set_string(0, 0, "VOL_OUT   A");
      res = XMETER_RES_VOLTAGE;
      break;
    case CALIBRATE_C:
      lcd_set_string(0, 0, "CURRENT   A");
      res = XMETER_RES_CURRENT;
      break;
    case CALIBRATE_VD:
      lcd_set_string(0, 0, "VOL_DISS  A");
      res = XMETER_RES_VOLTAGE;
      break; 
    default:
        break;    
  }
  
  xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, res);
  
  lcd_set_string(1, 0, "1/1");
  lcd_set_digit(1, 3, &value);
  lcd_set_char(1, 2, calibrate_type == CALIBRATE_VD ? '1' :  XMETER_GRID_SIZE + 0x30);
  if(calibrate_type != CALIBRATE_VD) {
     lcd_set_string(1, 10, "D 0000");
  }
  
  lcd_set_blink_range(1, is_bits ? 12 : 3, is_bits ? 15 : 8, 1);
  
  if(is_bits) {
    switch(calibrate_type){
      case CALIBRATE_VO:
        xmeter_set_dac_bits_v(cal_grid_dac[calibrate_index].bits);
        break;
      case CALIBRATE_C: 
        xmeter_set_dac_bits_c(cal_grid_dac[calibrate_index].bits);
        break;   
      default:
        break;
    }
  }
}



static void calibrate_fill_val_bits(bit is_bits)
{
  xmeter_value_t value;
  uint8_t res;
  
  lcd_set_hex(0, 12, cal_grid_adc[calibrate_index].bits);
  

  if(calibrate_type != CALIBRATE_VD) {
    lcd_set_hex(1, 12, cal_grid_dac[calibrate_index].bits);
  }

  if(calibrate_type == CALIBRATE_VO || calibrate_type == CALIBRATE_VD) {
    res = XMETER_RES_VOLTAGE;
  } else {
    res = XMETER_RES_CURRENT;
  }
  xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, res);
  lcd_set_digit(1, 3, &value);
  lcd_set_char(1, 0, calibrate_index + 1 + 0x30);

}

static void calibrate_inc_val(bit coarse)
{
  xmeter_value_t value;
  if(calibrate_type == CALIBRATE_VO || calibrate_type == CALIBRATE_VD) {
    xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, XMETER_RES_VOLTAGE);
    xmeter_inc_voltage_value(&value, coarse);
  } else {
    xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, XMETER_RES_CURRENT);
    xmeter_inc_current_value(&value, coarse);
  }
  
  cal_grid_adc[calibrate_index].val = xmeter_val2float(&value);
  cal_grid_dac[calibrate_index].val = cal_grid_adc[calibrate_index].val;  
}

static void calibrate_dec_val(bit coarse)
{
  xmeter_value_t value;
  if(calibrate_type == CALIBRATE_VO || calibrate_type == CALIBRATE_VD) {
    xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, XMETER_RES_VOLTAGE);
    xmeter_dec_voltage_value(&value, coarse);
  } else {
    xmeter_float2val(cal_grid_adc[calibrate_index].val, &value, XMETER_RES_CURRENT);
    xmeter_dec_current_value(&value, coarse);
  }
  
  cal_grid_adc[calibrate_index].val = xmeter_val2float(&value);
  cal_grid_dac[calibrate_index].val = cal_grid_adc[calibrate_index].val; 
}

static void calibrate_load_adc_bits()
{
  switch(calibrate_type) {
    case CALIBRATE_VO:
      cal_grid_adc[calibrate_index].bits = xmeter_get_adc_bits_voltage_out();
      break;
    case CALIBRATE_VD:
      cal_grid_adc[calibrate_index].bits = xmeter_get_adc_bits_voltage_diss();
      break;
    case CALIBRATE_C:
      cal_grid_adc[calibrate_index].bits = xmeter_get_adc_bits_current();
      break;
    default:
      break;
  }
}

static void calibrate_inc_dac_bits(bit coarse)
{
  switch(calibrate_type) {
    case CALIBRATE_VO:
      xmeter_inc_dac_bits_v(coarse);
      cal_grid_dac[calibrate_index].bits = xmeter_get_dac_bits_v();
      break;
    case CALIBRATE_C:
      xmeter_inc_dac_bits_c(coarse);
      cal_grid_dac[calibrate_index].bits = xmeter_get_dac_bits_c();
      break;
    default:
      break;
  }
}

static void calibrate_dec_dac_bits(bit coarse)
{
  switch(calibrate_type) {
    case CALIBRATE_VO:
      xmeter_dec_dac_bits_v(coarse);
      cal_grid_dac[calibrate_index].bits = xmeter_get_dac_bits_v();
      break;
    case CALIBRATE_C:
      xmeter_dec_dac_bits_c(coarse);
      cal_grid_dac[calibrate_index].bits = xmeter_get_dac_bits_c();
      break;
    default:
      break;
  }
}

static void calibrate_init_save()
{
  lcd_clear();
  switch(calibrate_type){
    case CALIBRATE_VO:
      lcd_set_string(0, 0, "SAVE VOL_OUT...");
      break;
    case CALIBRATE_C:
      lcd_set_string(0, 0, "SAVE CURRENT...");
      break;
    case CALIBRATE_VD:
      lcd_set_string(0, 0, "SAVE VOL_DISS...");
      break; 
    default:
      break;      
  }
  lcd_set_string(1, 0, "Press Set");
}

static void calibrate_fill_save()
{
  lcd_clear();
  lcd_set_string(0, 0, "DONE");
  lcd_set_string(1, 0, "Press Mod");
}

static void calibrate_init_temp()
{
  lcd_clear();
  lcd_set_string(0, 0, "Temperature:");
  lcd_set_special_char(1, 6, LCD_CELSIUS);
}

static void calibrate_fill_temp()
{
  lcd_set_digit(1, 0, &xmeter_adc_temp);
}

static void calibrate_save_rom()
{
  uint16_t bits;
  double value;
  switch(calibrate_type){
    case CALIBRATE_VO:
      xmeter_write_rom_adc_voltage_out_g(cal_grid_adc, XMETER_GRID_SIZE);
      xmeter_write_rom_dac_voltage_g(cal_grid_dac, XMETER_GRID_SIZE); 
      xmeter_reload_adc_voltage_out_config();
      xmeter_reload_dac_voltage_config();
      break;
    case CALIBRATE_C:
      xmeter_write_rom_adc_current_g(cal_grid_adc, XMETER_GRID_SIZE);
      xmeter_write_rom_dac_current_g(cal_grid_dac, XMETER_GRID_SIZE); 
      xmeter_reload_adc_current_config();
      xmeter_reload_dac_current_config();
      break;
    case CALIBRATE_VD:
      bits = cal_grid_adc[0].bits;
      value = cal_grid_adc[0].val;
      xmeter_cal_grid(0x0, bits, 1, 0.0, value, cal_grid_adc, XMETER_GRID_SIZE);
      xmeter_write_rom_adc_voltage_diss_g(cal_grid_adc, XMETER_GRID_SIZE);
      xmeter_reload_adc_voltage_diss_config();
      break; 
    default:
      break;      
  }
}

void do_calibrate_main(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_MAIN) {
    calibrate_type = CALIBRATE_VO;
    calibrate_index = 0;
    calibrate_init_main();
    calibrate_fill_main(); 
    return;
  }
  
  if(ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    calibrate_type = (calibrate_type + 1) % CALIBRATE_CNT;
    calibrate_fill_main(); 
  } else if(ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    calibrate_type = (calibrate_type - 1) % CALIBRATE_CNT;
    calibrate_fill_main(); 
  } else if(ev == EV_KEY_SET_PRESS) {
    if(calibrate_type == CALIBRATE_T) {
      task_set(EV_TIMEO);
    } else {
      task_set(EV_EVKEY);
    }
  }
}

void do_calibrate_val(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_VAL) {
    if(sm_cur_function == SM_CALIBRATE && sm_cur_state == SM_CALIBRATE_MAIN) {
      // load init val and bits
      switch(calibrate_type){
        case CALIBRATE_VO:
          xmeter_read_rom_adc_voltage_out_g(cal_grid_adc, XMETER_GRID_SIZE);
          xmeter_read_rom_dac_voltage_g(cal_grid_dac, XMETER_GRID_SIZE); 
          break;
        case CALIBRATE_C:
          xmeter_read_rom_adc_current_g(cal_grid_adc, XMETER_GRID_SIZE);
          xmeter_read_rom_dac_current_g(cal_grid_dac, XMETER_GRID_SIZE); 
          break;
        case CALIBRATE_VD:
          xmeter_read_rom_adc_voltage_diss_g(cal_grid_adc, XMETER_GRID_SIZE);
          break; 
        default:
          break;        
      }
    }
    calibrate_init_val_bits(0);
    calibrate_fill_val_bits(0); 
    return;
  }
  
  if(ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    calibrate_inc_val( ev == EV_KEY_MOD_C );
    calibrate_fill_val_bits(0);     
  } else if(ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    calibrate_dec_val( ev == EV_KEY_MOD_CC );
    calibrate_fill_val_bits(0); 
  } else if(ev == EV_KEY_SET_PRESS) {
    if(calibrate_type == CALIBRATE_VD) {
      calibrate_load_adc_bits();
      task_set(EV_TIMEO);
    } else {
      task_set(EV_EVKEY);
    }
  }
}

void do_calibrate_bits(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_BITS) {
    calibrate_init_val_bits(1);
    calibrate_fill_val_bits(1);  
    return;
  }
  
  if(ev == EV_250MS) {
    calibrate_load_adc_bits();
    calibrate_fill_val_bits(1); 
  } else if(ev == EV_KEY_MOD_C || ev == EV_KEY_SET_C) {
    calibrate_inc_dac_bits( ev == EV_KEY_MOD_C );
    calibrate_fill_val_bits(1);     
  } else if(ev == EV_KEY_MOD_CC || ev == EV_KEY_SET_CC) {
    calibrate_dec_dac_bits( ev == EV_KEY_MOD_CC );
    calibrate_fill_val_bits(1); 
  } else if(ev == EV_KEY_SET_PRESS) {
    if(calibrate_index == XMETER_GRID_SIZE - 1) {
      task_set(EV_TIMEO);
    } else {
      calibrate_index ++;
      task_set(EV_EVKEY);
    }
  }
}

void do_calibrate_temp(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_TEMP) {
    calibrate_init_temp();
    xmeter_read_adc_temp();
    calibrate_fill_temp(); 
    return;
  }
  
  xmeter_read_adc_temp();
  calibrate_fill_temp(); 
}

void do_calibrate_save(uint8_t to_func, uint8_t to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_SAVE) {
    calibrate_init_save();
    return;
  }
  
  if(ev == EV_KEY_SET_PRESS) {
    calibrate_init_save();
    CDBG("do_calibrate_save: save value\n");
    calibrate_save_rom();
    calibrate_fill_save(); 
  }
}

static const struct sm_trans_slot code  sm_trans_calibrate_main[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main}, 
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main},
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main}, 
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main},
  {EV_TIMEO, SM_CALIBRATE, SM_CALIBRATE_TEMP, do_calibrate_temp}, // temp
  {EV_EVKEY, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val}, // other 
  {EV_KEY_MOD_PRESS, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_calibrate_val[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val}, 
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val},
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val},
  {EV_TIMEO, SM_CALIBRATE, SM_CALIBRATE_SAVE, do_calibrate_save}, // done!
  {EV_EVKEY, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits}, // set bits  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_calibrate_bits[] = {
  {EV_250MS, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits}, 
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits}, 
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits},
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_BITS, do_calibrate_bits},
  {EV_EVKEY, SM_CALIBRATE, SM_CALIBRATE_VAL, do_calibrate_val},   // next value 
  {EV_TIMEO, SM_CALIBRATE, SM_CALIBRATE_SAVE, do_calibrate_save}, // done!
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_calibrate_temp[] = {
  {EV_250MS, SM_CALIBRATE, SM_CALIBRATE_TEMP, do_calibrate_temp}, 
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code  sm_trans_calibrate_save[] = {
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_SAVE, do_calibrate_save},
  {EV_KEY_MOD_PRESS, SM_CALIBRATE, SM_CALIBRATE_MAIN, do_calibrate_main},  
  {NULL, NULL, NULL, NULL}
};


const struct sm_state_slot code sm_function_calibrate[] = {
  {"SM_CALIBRATE_MAIN", sm_trans_calibrate_main},
  {"SM_CALIBRATE_VAL", sm_trans_calibrate_val}, 
  {"SM_CALIBRATE_BITS", sm_trans_calibrate_bits}, 
  {"SM_CALIBRATE_TEMP", sm_trans_calibrate_temp},  
  {"SM_CALIBRATE_SAVE", sm_trans_calibrate_save}, 
};