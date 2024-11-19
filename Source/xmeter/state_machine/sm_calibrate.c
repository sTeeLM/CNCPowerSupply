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

*/
static void sm_calibrate_init_phase0(uint8_t state)
{
  lcd_clear();
  lcd_set_string(0, 0, "Vd XXXX Vo XXXX");
  lcd_set_string(1, 0, "V DAC Bits XXXX");
}


static void sm_calibrate_fill_phase0(uint8_t state)
{
  lcd_set_hex(0, 3, xmeter_get_adc_bits_voltage_diss());
  lcd_set_hex(0, 11, xmeter_get_adc_bits_voltage_out());
  lcd_set_hex(1, 11, xmeter_get_dac_bits_v());    
}



static uint16_t sm_calibrate_dac_c_bits[2];
static uint16_t sm_calibrate_dac_v_bits[2];
static uint16_t sm_calibrate_adc_vd_bits[2];
static uint16_t sm_calibrate_adc_vo_bits[2];
static struct xmeter_value sm_calibrate_adc_vd_val[2];
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

static bit sm_calibrate_adc_vd()
{
  double k, b, f1, f2;
  bit ret = 0;
  do {
    f1 = xmeter_val2float(&sm_calibrate_adc_vd_val[0]);
    f2 = xmeter_val2float(&sm_calibrate_adc_vd_val[1]);
    if(!xmeter_cal(sm_calibrate_adc_vd_bits[0], sm_calibrate_adc_vd_bits[1], 
      f1, f2, &k, &b)) {
      break;
    }
    xmeter_write_rom_adc_voltage_diss_kb(k, b);
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

/* phase 1 : collect params
AAAAAA A +XX.XX
B XXXX D +XX.XX 

AAAAAA = Zero
       = Max
       = Diss

B = V for Voltage
  = C for Current
*/

static void sm_calibrate_init_phase1(uint8_t state)
{
  lcd_clear();
  switch(state) {
    case SM_CALIBRATE_VOLTAGE_ZERO:
      lcd_set_string(0, 0, "Zero   A");
      lcd_set_string(1, 0, "V      D");
      break;
    case SM_CALIBRATE_VOLTAGE_MAX:
      lcd_set_string(0, 0, "Max    A");
      lcd_set_string(1, 0, "V      D");
      break;     
    case SM_CALIBRATE_CURRENT_ZERO:  
      lcd_set_string(0, 0, "Zero   A");
      lcd_set_string(1, 0, "C      D");
      break;
    case SM_CALIBRATE_CURRENT_MAX:
      lcd_set_string(0, 0, "Zero   A");
      lcd_set_string(1, 0, "C      D");
      break; 
    case SM_CALIBRATE_VOLTAGE_DISS:
      lcd_set_string(0, 0, "Diss   0");
      lcd_set_string(1, 0, "V      M");
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
      lcd_set_char(0, 9, xmeter_adc_voltage_out.neg ? '-' : ' ');
      lcd_set_digit(0, 10, xmeter_adc_voltage_out.integer, xmeter_adc_voltage_out.decimal);
      lcd_set_char(1, 9, xmeter_dac_voltage.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_dac_voltage.integer, xmeter_dac_voltage.decimal);  
    break;
    case SM_CALIBRATE_CURRENT_ZERO:  
    case SM_CALIBRATE_CURRENT_MAX:
      bits = xmeter_get_dac_bits_c();
      lcd_set_hex(1, 2, bits);
      lcd_set_char(0, 9, xmeter_adc_current.neg ? '-' : ' ');
      lcd_set_digit(0, 10, xmeter_adc_current.integer, xmeter_adc_current.decimal);
      lcd_set_char(1, 9, xmeter_dac_current.neg ? '-' : ' ');
      lcd_set_digit(1, 10, xmeter_dac_current.integer, xmeter_dac_current.decimal);
    break;
    case SM_CALIBRATE_VOLTAGE_DISS:
      lcd_set_char(0, 9, sm_calibrate_adc_vd_val[0].neg ? '-' : ' ');
      lcd_set_digit(0, 10, sm_calibrate_adc_vd_val[0].integer, sm_calibrate_adc_vd_val[0].decimal);
      lcd_set_char(1, 9, sm_calibrate_adc_vd_val[1].neg ? '-' : ' ');
      lcd_set_digit(1, 10, sm_calibrate_adc_vd_val[1].integer, sm_calibrate_adc_vd_val[1].decimal);
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
#define SM_CALIBRATE_STEP_ADC_VOLTAGE_DISS 3
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
    case SM_CALIBRATE_STEP_ADC_VOLTAGE_DISS:
      lcd_clear();
      lcd_set_string(0, 0, "Cal ADC Vd......");
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

static void do_calibrate_voltage_zero(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_ZERO) {
    xmeter_set_dac_bits_v(0x0); // output voltage maybe negtive
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

void do_calibrate_voltage_max(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_MAX) {
    
    // fill sm_calibrate_dac_v_bits[0]
    sm_calibrate_dac_v_bits[0]  = xmeter_get_dac_bits_c();
    // fill sm_calibrate_adc_vo_bits[0]
    sm_calibrate_adc_vo_bits[0] = xmeter_get_adc_bits_voltage_out();
    // fill sm_calibrate_adc_vd_bits[1]
    sm_calibrate_adc_vd_bits[1] = xmeter_get_adc_bits_voltage_diss();
    // fill sm_calibrate_adc_vd_val[1] at last step..
    
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

void do_calibrate_current_zero(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_CURRENT_ZERO) {
    
    // fill sm_calibrate_dac_v_bits[1]
    sm_calibrate_dac_v_bits[1]  = xmeter_get_dac_bits_c();
    // fill sm_calibrate_adc_vo_bits[1]
    sm_calibrate_adc_vo_bits[1] = xmeter_get_adc_bits_voltage_out();
    // fill sm_calibrate_adc_vd_bits[1]
    sm_calibrate_adc_vd_bits[0] = 0x0;
    // fill sm_calibrate_adc_vd_val[0];
    xmeter_assign_value(&xmeter_zero3, &sm_calibrate_adc_vd_val[0]);
    
    // set voltage dac to 5V prox.
    xmeter_set_dac_bits_v(0x2AAA);
    
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

void do_calibrate_voltage_diss(unsigned char to_func, unsigned char to_state, enum task_events ev)
{
  if(sm_cur_state != SM_CALIBRATE_VOLTAGE_DISS) {
    // save params
    sm_calibrate_dac_c_bits[1] = xmeter_get_dac_bits_c();
    sm_calibrate_adc_c_bits[1] = xmeter_get_adc_bits_current();
    
    xmeter_assign_value(&xmeter_adc_voltage_diss, &sm_calibrate_adc_vd_val[1]);
    
    sm_calibrate_init_phase1(SM_CALIBRATE_VOLTAGE_DISS);
    sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_DISS);
    return;
  }
  
  if (ev == EV_KEY_MOD_C) {
    xmeter_inc_voltage_value(&sm_calibrate_adc_vd_val[1], 1);
  } else if (ev == EV_KEY_MOD_CC) {
    xmeter_dec_voltage_value(&sm_calibrate_adc_vd_val[1], 1);
  } else if (ev == EV_KEY_SET_C) {
    xmeter_inc_voltage_value(&sm_calibrate_adc_vd_val[1], 0);
  } else if (ev == EV_KEY_SET_CC) {
    xmeter_dec_voltage_value(&sm_calibrate_adc_vd_val[1], 0);
  }
  xmeter_read_adc();
  sm_calibrate_fill_phase1(SM_CALIBRATE_VOLTAGE_DISS);    
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
    CDBG("sm_calibrate_adc_vd_bits: [%04x][%04x]\n", 
      sm_calibrate_adc_vd_bits[0],
      sm_calibrate_adc_vd_bits[1]);  
    xmeter_dump_value("sm_calibrate_adc_vd_val", sm_calibrate_adc_vd_val, 2);  
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
          sm_calibrate_step = SM_CALIBRATE_STEP_ADC_VOLTAGE_DISS;  
        } else {
          sm_calibrate_step = SM_CALIBRATE_STEP_FAIL;
        }
        break;
      case SM_CALIBRATE_STEP_ADC_VOLTAGE_DISS:
        if(sm_calibrate_adc_vd()) {
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
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},  
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_zero[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_ZERO, do_calibrate_voltage_zero},
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_max[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_MAX, do_calibrate_voltage_max},
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
  {EV_KEY_SET_PRESS, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_DISS, do_calibrate_voltage_diss},      
  {NULL, NULL, NULL, NULL}
};

static const struct sm_trans_slot code sm_trans_calibrate_voltage_diss[] = {
  {EV_KEY_MOD_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_DISS, do_calibrate_voltage_diss}, 
  {EV_KEY_MOD_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_DISS, do_calibrate_voltage_diss},
  {EV_KEY_SET_C, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_DISS, do_calibrate_voltage_diss}, 
  {EV_KEY_SET_CC, SM_CALIBRATE, SM_CALIBRATE_VOLTAGE_DISS, do_calibrate_voltage_diss},
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
  {"SM_CALIBRATE_VOLTAGE_ZERO", sm_trans_calibrate_voltage_zero}, 
  {"SM_CALIBRATE_VOLTAGE_MAX", sm_trans_calibrate_voltage_max},
  {"SM_CALIBRATE_CURRENT_ZERO", sm_trans_calibrate_current_zero}, 
  {"SM_CALIBRATE_CURRENT_MAX", sm_trans_calibrate_current_max},
  {"SM_CALIBRATE_VOLTAGE_DISS", sm_trans_calibrate_voltage_diss},  
  {"SM_CALIBRATE_BK", sm_trans_calibrate_bk}, 
};