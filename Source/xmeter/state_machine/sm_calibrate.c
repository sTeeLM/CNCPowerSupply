#include "sm_calibrate.h"
#include "sm_xmeter.h"
#include "task.h"
#include "sm.h"
#include "xmeter.h"
#include "lcd.h"
#include "rom.h"
#include "delay.h"
#include "debug.h"
/*
phase 0: physical calibrate
voltage:
Vi XXXX Vo XXXX
V DAC Bits XXXX 

current:
C ADC Bits XXXX
C DAC Bits XXXX

*/
static void sm_calibrate_init_phase0(uint8_t state)
{
  lcd_clear();
  if(state == SM_CALIBRATE_PHY_VOLTAGE) {
    lcd_set_string(0, 0, "Vi XXXX Vo XXXX");
    lcd_set_string(1, 0, "V DAC Bits XXXX");
  } else {
    lcd_set_string(0, 0, "C ADC Bits XXXX");
    lcd_set_string(1, 0, "C DAC Bits XXXX");
  }
}


static void sm_calibrate_fill_phase0(uint8_t state)
{
  if(state == SM_CALIBRATE_PHY_VOLTAGE) {
    lcd_set_hex(0, 3, xmeter_get_adc_bits_voltage_in());
    lcd_set_hex(0, 11, xmeter_get_adc_bits_voltage_out());
    lcd_set_hex(1, 11, xmeter_get_dac_bits_v());    
  } else {
    lcd_set_hex(0, 11, xmeter_get_adc_bits_current());
    lcd_set_hex(1, 11, xmeter_get_dac_bits_c());  
  }
}


/* phase 1 : collect params
AAAAAA i +XX.XX
B XXXX o +XX.XX 

AAAAAA = Zero
       = ZeroIn
       = Max
       = MaxIn

B = V for Voltage
  = C for Current
*/
static uint16_t sm_calibrate_dac_c_bits[2];
static uint16_t sm_calibrate_dac_v_bits[2];
static uint16_t sm_calibrate_adc_vi_bits[2];
static uint16_t sm_calibrate_adc_vo_bits[2];
static struct xmeter_value sm_calibrate_adc_vi_val[2];
static uint16_t sm_calibrate_adc_c_bits[2];

static bit sm_calibrate_dac()
{
  double k, b;
  bit ret = 0;
  do {
    if(!xmeter_cal(sm_calibrate_dac_c_bits[0], sm_calibrate_dac_c_bits[1], 
      0.0, 5.0, &k, &b)) {
      break;
    }
    xmeter_write_rom_dac_current_kb(k, b);  

    if(!xmeter_cal(sm_calibrate_dac_v_bits[0], sm_calibrate_dac_v_bits[1], 
      0.0, 30.0, &k, &b)) {
      break;
    }
    xmeter_write_rom_dac_voltage_kb(k, b);  
    ret = 1;
  }while(0);
  return ret;
}

static bit sm_calibrate_adc_c()
{
  double k, b;
  bit ret = 0;
  do {
    if(!xmeter_cal(sm_calibrate_adc_c_bits[0], sm_calibrate_adc_c_bits[1], 
      0.0, 5.0, &k, &b)) {
      break;
    }
    xmeter_write_rom_adc_current_kb(k, b);
    ret = 1;
  }while(0);
  return ret;
}

static bit sm_calibrate_adc_vi()
{
  double k, b, f1, f2;
  bit ret = 0;
  do {
    f1 = xmeter_val2float(&sm_calibrate_adc_vi_val[0]);
    f2 = xmeter_val2float(&sm_calibrate_adc_vi_val[1]);
    if(!xmeter_cal(sm_calibrate_adc_vi_bits[0], sm_calibrate_adc_vi_bits[1], 
      f1, f2, &k, &b)) {
      break;
    }
    xmeter_write_rom_adc_voltage_in_kb(k, b);
    ret = 1;
  }while(0);
  return ret;
}

static bit sm_calibrate_adc_vo()
{
  double k, b;
  bit ret = 0;
  do {
    if(!xmeter_cal(sm_calibrate_adc_vo_bits[0], sm_calibrate_adc_vo_bits[1], 
      0.0, 30.0, &k, &b)) {
      break;
    }
    xmeter_write_rom_adc_voltage_out_kb(k, b);
    ret = 1;
  }while(0);
  return ret;
}

static void sm_calibrate_init_phase1(uint8_t state)
{
  lcd_clear();
  lcd_set_char(0, 7, 'i');
  lcd_set_char(1, 7, 'o');  
  switch(state) {
    case SM_CALIBRATE_VOLTAGE_ZERO:
      lcd_set_string(0, 0, "Zero  ");
      lcd_set_char(1, 0, 'V');
      break;
    case SM_CALIBRATE_VOLTAGE_ZERO_IN:
      lcd_set_string(0, 0, "ZeroIn");
      lcd_set_char(1, 0, 'V');
      break;
    case SM_CALIBRATE_VOLTAGE_MAX:
      lcd_set_string(0, 0, "Max   ");
      lcd_set_char(1, 0, 'V');
      break;
    case SM_CALIBRATE_VOLTAGE_MAX_IN:
      lcd_set_string(0, 0, "Max In");
      lcd_set_char(1, 0, 'V');
      break;      
    case SM_CALIBRATE_CURRENT_ZERO:  
      lcd_set_string(0, 0, "Zero  ");
      lcd_set_char(1, 0, 'C');
      break;
    case SM_CALIBRATE_CURRENT_MAX:
      lcd_set_string(0, 0, "Max   ");
      lcd_set_char(1, 0, 'C');
      break;      
  }
}

static void sm_calibrate_fill_phase1(uint8_t state)
{
  uint16_t bits;
  switch(state) {
    case SM_CALIBRATE_VOLTAGE_ZERO:
    case SM_CALIBRATE_VOLTAGE_MAX:
      bits = xmeter_get_dac_bits_v();
      lcd_set_hex(1, 2, bits);
      lcd_set_char(0, 9, xmeter_adc_voltage_in.neg ? '-' : ' ');
      lcd_set_digit(0, 10, xmeter_adc_voltage_in.integer, xmeter_adc_voltage_in.decimal);
      lcd_set_char(1, 9, xmeter_adc_voltage_out.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);  
    break;    
    case SM_CALIBRATE_VOLTAGE_ZERO_IN:
      lcd_set_hex(1, 2, sm_calibrate_dac_v_bits[0]);
      lcd_set_char(0, 9, sm_calibrate_adc_vi_val[0].neg ? '-' : ' ');
      lcd_set_digit(0, 10, sm_calibrate_adc_vi_val[0].integer, sm_calibrate_adc_vi_val[0].decimal);
      lcd_set_char(1, 9, xmeter_adc_voltage_out.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);  
    break;
    case SM_CALIBRATE_VOLTAGE_MAX_IN:
      lcd_set_hex(1, 2, sm_calibrate_dac_v_bits[1]);
      lcd_set_char(0, 9, sm_calibrate_adc_vi_val[1].neg ? '-' : ' ');
      lcd_set_digit(0, 10, sm_calibrate_adc_vi_val[1].integer, sm_calibrate_adc_vi_val[1].decimal);
      lcd_set_char(1, 9, xmeter_adc_voltage_out.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);  
    break;
    case SM_CALIBRATE_CURRENT_ZERO:  
    case SM_CALIBRATE_CURRENT_MAX:
      bits = xmeter_get_dac_bits_c();
      lcd_set_hex(1, 2, bits);
      lcd_set_char(0, 9, xmeter_adc_current.neg ? '-' : ' ');
      lcd_set_digit(0, 10, xmeter_adc_current.integer, xmeter_adc_current.decimal);
      lcd_set_char(1, 9, xmeter_adc_current.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_adc_current.integer, xmeter_adc_current.decimal);
    break;
      default:
      break;
  }
}

/* phase 2 : calculate b\k from params, and save to rom

*/
static uint8_t sm_calibrate_step;

#define SM_CALIBRATE_STEP_DAC 1
#define SM_CALIBRATE_STEP_ADC_CURRENT 2
#define SM_CALIBRATE_STEP_ADC_VOLTAGE_IN 3
#define SM_CALIBRATE_STEP_ADC_VOLTAGE_OUT 4
#define SM_CALIBRATE_STEP_DONE 5
#define SM_CALIBRATE_STEP_FAIL 6

static void sm_calibrate_init_phase2(uint8_t step)
{
  lcd_clear();
}

static void sm_calibrate_fill_phase2(uint8_t step)
{
  switch(step) {
    case SM_CALIBRATE_STEP_DAC:
      lcd_clear();
      lcd_set_string(0, 0, "Cal DAC.........");
    break;
    case SM_CALIBRATE_STEP_ADC_CURRENT:
      lcd_clear();
      lcd_set_string(0, 0, "Cal ADC C.......");
    break;      
    case SM_CALIBRATE_STEP_ADC_VOLTAGE_IN:
      lcd_clear();
      lcd_set_string(0, 0, "Cal ADC Vi......");
    break;      
    case SM_CALIBRATE_STEP_ADC_VOLTAGE_OUT:
      lcd_clear();
      lcd_set_string(0, 0, "Cal ADC Vo......");
    break;      
    case SM_CALIBRATE_STEP_DONE:
      lcd_set_string(1, 0, "DONE   Press Set");    
    break;          
    case SM_CALIBRATE_STEP_FAIL:
      lcd_set_string(0, 12, "FAIL"); 
      lcd_set_string(1, 0, "       Press Set");      
  }  
}

void do_calibrate_phy_voltage(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_function != SM_CALIBRATE || sm_cur_state != SM_CALIBRATE_PHY_VOLTAGE) {
    sm_calibrate_init_phase0(SM_CALIBRATE_PHY_VOLTAGE);
    sm_calibrate_fill_phase0(SM_CALIBRATE_PHY_VOLTAGE);
    return ;
  }
  if(ev == EV_250MS) {
    xmeter_read_adc();
  } else if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_v(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_v(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_v(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_v(0);
  }  
  sm_calibrate_fill_phase0(SM_CALIBRATE_PHY_VOLTAGE);
}

void do_calibrate_phy_current(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_PHY_CURRENT) {
    sm_calibrate_init_phase0(SM_CALIBRATE_PHY_CURRENT);
    sm_calibrate_fill_phase0(SM_CALIBRATE_PHY_CURRENT);
    return ;
  }
  if(ev == EV_250MS) {
    xmeter_read_adc();
  } else if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_c(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_c(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_c(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_c(0);
  }
  sm_calibrate_fill_phase0(SM_CALIBRATE_PHY_CURRENT);
}

static void do_calibrate_voltage_zero(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_ZERO) {
    xmeter_set_dac_bits_v(0x0);
    sm_calibrate_init_phase1(SM_CALIBRATE_VOLTAGE_ZERO);
    sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_ZERO);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_v(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_v(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_v(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_v(0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_ZERO);
}

void do_calibrate_voltage_zero_in(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_ZERO_IN) {
    // save params
    sm_calibrate_dac_v_bits[0] = xmeter_get_dac_bits_v();
    sm_calibrate_adc_vo_bits[0] = xmeter_get_adc_bits_voltage_out();
    sm_calibrate_adc_vi_bits[0] = xmeter_get_adc_bits_voltage_in();
    xmeter_assign_value(&xmeter_adc_voltage_in, &sm_calibrate_adc_vi_val[0]);
    sm_calibrate_init_phase1(SM_CALIBRATE_VOLTAGE_ZERO_IN);
    sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_ZERO_IN);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_value(&sm_calibrate_adc_vi_val[0], 1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_value(&sm_calibrate_adc_vi_val[0], 1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_value(&sm_calibrate_adc_vi_val[0], 0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_value(&sm_calibrate_adc_vi_val[0], 0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_ZERO_IN);  
}

void do_calibrate_voltage_max(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_MAX) {
    xmeter_set_dac_bits_v(0xffff);
    sm_calibrate_init_phase1(SM_CALIBRATE_VOLTAGE_MAX);
    sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_MAX);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_v(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_v(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_v(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_v(0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_MAX);
}

void do_calibrate_voltage_max_in(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_MAX_IN) {
    // save params
    sm_calibrate_dac_v_bits[1] = xmeter_get_dac_bits_v();
    sm_calibrate_adc_vi_bits[1] = xmeter_get_adc_bits_voltage_in();
    sm_calibrate_adc_vo_bits[1] = xmeter_get_adc_bits_voltage_out();
    xmeter_assign_value(&xmeter_adc_voltage_in, &sm_calibrate_adc_vi_val[1]);
    sm_calibrate_init_phase1(SM_CALIBRATE_VOLTAGE_MAX_IN);
    sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_MAX_IN);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_value(&sm_calibrate_adc_vi_val[1], 1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_value(&sm_calibrate_adc_vi_val[1], 1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_value(&sm_calibrate_adc_vi_val[1], 0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_value(&sm_calibrate_adc_vi_val[1], 0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_MAX_IN);
}

void do_calibrate_current_zero(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_CURRENT_ZERO) {
    xmeter_set_dac_bits_c(0x0);
    sm_calibrate_init_phase1(SM_CALIBRATE_CURRENT_ZERO);
    sm_calibrate_fill_phase1(SM_CALIBRATE_CURRENT_ZERO);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_c(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_c(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_c(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_c(0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_CURRENT_ZERO);
}

void do_calibrate_current_max(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_CURRENT_MAX) {
    
    // save param
    sm_calibrate_dac_c_bits[0] = xmeter_get_dac_bits_c();
    sm_calibrate_adc_c_bits[0] = xmeter_get_adc_bits_current();
    
    xmeter_set_dac_bits_c(0xffff);  
    
    sm_calibrate_init_phase1(SM_CALIBRATE_CURRENT_MAX);
    sm_calibrate_fill_phase1(SM_CALIBRATE_CURRENT_MAX);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_dac_bits_c(1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_dac_bits_c(1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_dac_bits_c(0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_dac_bits_c(0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_CURRENT_MAX);
}

void do_calibrate_bk(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  
  if(sm_cur_state != SM_CALIBRATE_BK) {
    
    // save param
    sm_calibrate_dac_c_bits[1] = xmeter_get_dac_bits_c();
    sm_calibrate_adc_c_bits[1] = xmeter_get_adc_bits_current();
    sm_calibrate_init_phase2(SM_CALIBRATE_STEP_DAC);
    sm_calibrate_fill_phase2(SM_CALIBRATE_STEP_DAC);
    
    sm_calibrate_step = SM_CALIBRATE_STEP_DAC;
    
    CDBG("do_calibrate_bk all params:\n");
    CDBG("sm_calibrate_dac_c_bits: [%04x][%04x]\n", 
      sm_calibrate_dac_c_bits[0],
      sm_calibrate_dac_c_bits[1]);
    CDBG("sm_calibrate_dac_v_bits: [%04x][%04x]\n", 
      sm_calibrate_dac_v_bits[0],
      sm_calibrate_dac_v_bits[1]);    
    CDBG("sm_calibrate_adc_c_bits: [%04x][%04x]\n", 
      sm_calibrate_adc_c_bits[0],
      sm_calibrate_adc_c_bits[1]);
    CDBG("sm_calibrate_adc_vi_bits: [%04x][%04x]\n", 
      sm_calibrate_adc_vi_bits[0],
      sm_calibrate_adc_vi_bits[1]);  
    xmeter_dump_value("sm_calibrate_adc_vi_val", sm_calibrate_adc_vi_val, 2);  
    CDBG("sm_calibrate_adc_vo_bits: [%04x][%04x]\n", 
      sm_calibrate_adc_vo_bits[0],
      sm_calibrate_adc_vo_bits[1]);    
    return;
  }
    
  if (ev == EV_1S) {
    switch (sm_calibrate_step) {
      case SM_CALIBRATE_STEP_DAC:
        if(sm_calibrate_dac()) {
          sm_calibrate_step = SM_CALIBRATE_STEP_ADC_CURRENT;
        } else {
          sm_calibrate_step = SM_CALIBRATE_STEP_FAIL;
        }
        break;
      case SM_CALIBRATE_STEP_ADC_CURRENT:
        if(sm_calibrate_adc_c()) {
          sm_calibrate_step = SM_CALIBRATE_STEP_ADC_VOLTAGE_IN;  
        } else {
          sm_calibrate_step = SM_CALIBRATE_STEP_FAIL;
        }
        break;
      case SM_CALIBRATE_STEP_ADC_VOLTAGE_IN:
        if(sm_calibrate_adc_vi()) {
          sm_calibrate_step = SM_CALIBRATE_STEP_ADC_VOLTAGE_OUT;
        } else {
          sm_calibrate_step = SM_CALIBRATE_STEP_FAIL;
        }
        break;
      case SM_CALIBRATE_STEP_ADC_VOLTAGE_OUT:
        if(sm_calibrate_adc_vo()) {
          sm_calibrate_step = SM_CALIBRATE_STEP_DONE;
        } else {
          sm_calibrate_step = SM_CALIBRATE_STEP_FAIL;
        }
        break;
      case SM_CALIBRATE_STEP_DONE:
      case SM_CALIBRATE_STEP_FAIL:
        break;
    }
    sm_calibrate_fill_phase2(sm_calibrate_step);
  } else if(ev == EV_KEY_SET_PRESS) {
    if(sm_calibrate_step == SM_CALIBRATE_STEP_DONE || 
      sm_calibrate_step == SM_CALIBRATE_STEP_FAIL) {
        task_set(EV_TIMEO);
      }
  }
  
}

static const struct sm_trans_slot code sm_trans_calibrate_phy_voltage[] = {
  {EV_250MS, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage},
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current},  
};

static const struct sm_trans_slot code sm_trans_calibrate_phy_current[] = {
  {EV_250MS, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current},
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_PHY_CURRENT, do_calibrate_phy_current},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_PHY_VOLTAGE, do_calibrate_phy_voltage},
  {EV_KEY_MOD_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},  
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_zero[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO_IN, do_calibrate_voltage_zero_in},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_zero_in[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO_IN, do_calibrate_voltage_zero_in}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO_IN, do_calibrate_voltage_zero_in},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO_IN, do_calibrate_voltage_zero_in}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO_IN, do_calibrate_voltage_zero_in},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_max[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX_IN, do_calibrate_voltage_max_in},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_max_in[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX_IN, do_calibrate_voltage_max_in}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX_IN, do_calibrate_voltage_max_in},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX_IN, do_calibrate_voltage_max_in}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX_IN, do_calibrate_voltage_max_in},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_CURRENT_ZERO, do_calibrate_current_zero},  
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_current_zero[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_CURRENT_ZERO, do_calibrate_current_zero}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_CURRENT_ZERO, do_calibrate_current_zero},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_CURRENT_ZERO, do_calibrate_current_zero}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_CURRENT_ZERO, do_calibrate_current_zero},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_CURRENT_MAX, do_calibrate_current_max},    
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_current_max[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_CURRENT_MAX, do_calibrate_current_max}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_CURRENT_MAX, do_calibrate_current_max},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_CURRENT_MAX, do_calibrate_current_max}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_CURRENT_MAX, do_calibrate_current_max},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_BK, do_calibrate_bk},      
  {NULL, NULL, NULL, NULL}
};
static const struct sm_trans_slot code sm_trans_calibrate_bk[] = {
  {EV_1S, SM_CALIBRATE, SM_CALIBRATE_BK, do_calibrate_bk},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_BK, do_calibrate_bk},  
  {EV_TIMEO, SM_XMETER, SM_XMETER_MAIN, do_xmeter_main},   
  {NULL, NULL, NULL, NULL}
};

const struct sm_state_slot code sm_function_calibrate[] = {
  {"SM_CALIBRATE_PHY_VOLTAGE", sm_trans_calibrate_phy_voltage},
  {"SM_CALIBRATE_PHY_CURRENT", sm_trans_calibrate_phy_current},  
  {"SM_CALIBRATE_VOLTAGE_ZERO", sm_trans_calibrate_voltage_zero},
  {"SM_CALIBRATE_VOLTAGE_ZERO_IN", sm_trans_calibrate_voltage_zero_in},  
  {"SM_CALIBRATE_VOLTAGE_MAX", sm_trans_calibrate_voltage_max},
  {"SM_CALIBRATE_VOLTAGE_MAX_IN", sm_trans_calibrate_voltage_max_in},
  {"SM_CALIBRATE_CURRENT_ZERO", sm_trans_calibrate_current_zero}, 
  {"SM_CALIBRATE_CURRENT_MAX", sm_trans_calibrate_current_max},
  {"SM_CALIBRATE_BK", sm_trans_calibrate_bk}, 
};