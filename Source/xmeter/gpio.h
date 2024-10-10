#ifndef __XMETER_GPIO_H__
#define __XMETER_GPIO_H__

#include <STC89C5xRC.H>
/* com */
// P3.0 as RxD
// P3.1 as TxD

/* clock */
// P3.4 as T0

/* button */
/*
KEY_MOD_A alse connect to IT0(P3.2) as trigger
*/
sbit KEY_MOD_A = P2 ^ 0;
sbit KEY_MOD_B = P2 ^ 1;
sbit KEY_MOD_C = P2 ^ 2;
/*
KEY_SET_A also connect to IT1(P3.3) as trigger
*/
sbit KEY_SET_A = P2 ^ 3;
sbit KEY_SET_B = P2 ^ 4;
sbit KEY_SET_C = P2 ^ 5;


/* i2c */
sbit I2C_SCL = P2 ^ 6;  
sbit I2C_SDA = P2 ^ 7; 

/* rom */
sbit ROM_RESET = P2 ^ 2; // 和Mod共用一个，开机时长按Mod相当于工厂reset

/* beeper */
sbit BEEPER_OUT = P4 ^ 0;

/* lcd */
#define LCD_DATA P0

sbit LCD_RS = P1 ^ 0;
sbit LCD_RW = P1 ^ 1;
sbit LCD_E  = P1 ^ 2;

/* xmeter */
sbit XMETER_OUTPUT_SWITCH       =  P1 ^ 3;
sbit XMETER_FAN_SWITCH          =  P1 ^ 4;
sbit XMETER_CC                  =  P1 ^ 5;
sbit XMETER_CONV_RDY            =  P1 ^ 6;

void gpio_initialize (void);

#endif
