#ifndef __CONTROL_PROTOCOL_H__
#define __CONTROL_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
  
#ifndef __C51__
#define PROTO_PACKED  __attribute__((packed))
#else
#define PROTO_PACKED
#endif
  
/* max: + 999.999
   min: - 999.999
*/
typedef struct xmeter_value {
  uint8_t  neg;             /* is negative ? 0 is positive! */
  uint8_t  res;             /* 精度， x精确到小数后x位，0 <= x <= 3*/
  uint16_t integer;
  uint16_t decimal;
} PROTO_PACKED xmeter_value_t ; 

/* 
max 127
CMD: MSB is zero
RESPONSE: MSB is 1
*/
typedef enum _control_msg_code_t{
  CONTROL_MSG_CODE_NONE     = 0,
  CONTROL_MSG_CODE_HEATBEAT,
  CONTROL_MSG_CODE_REMOTE_OVERRIDE,
  CONTROL_MSG_CODE_START_STOP_FAN, 
  CONTROL_MSG_CODE_START_STOP_SWITCH, 
  CONTROL_MSG_CODE_GET_XMETER_STATUS,  
  CONTROL_MSG_CODE_GET_ADC_CURRENT,
  CONTROL_MSG_CODE_GET_ADC_VOLTAGE_IN,
  CONTROL_MSG_CODE_GET_ADC_VOLTAGE_OUT,
  CONTROL_MSG_CODE_GET_ADC_TEMP,
  CONTROL_MSG_CODE_GET_POWER_OUT,
  CONTROL_MSG_CODE_GET_POWER_DISS,
  CONTROL_MSG_CODE_GET_DAC_VOLTAGE,  
  CONTROL_MSG_CODE_SET_DAC_VOLTAGE,
  CONTROL_MSG_CODE_GET_DAC_CURRENT,
  CONTROL_MSG_CODE_SET_DAC_CURRENT,
  CONTROL_MSG_CODE_GET_STEPS_VOLTAGE,
  CONTROL_MSG_CODE_GET_STEPS_CURRENT,
  CONTROL_MSG_CODE_GET_STEPS_TEMP,
  CONTROL_MSG_CODE_GET_STEPS_POWER,  
  CONTROL_MSG_CODE_GET_PARAM_TEMP_HI,
  CONTROL_MSG_CODE_SET_PARAM_TEMP_HI,
  CONTROL_MSG_CODE_GET_PARAM_TEMP_LO,
  CONTROL_MSG_CODE_SET_PARAM_TEMP_LO,
  CONTROL_MSG_CODE_GET_PARAM_OVER_HEAT,
  CONTROL_MSG_CODE_SET_PARAM_OVER_HEAT,
  CONTROL_MSG_CODE_GET_PARAM_MAX_POWER_DISS,
  CONTROL_MSG_CODE_SET_PARAM_MAX_POWER_DISS,
  CONTROL_MSG_CODE_GET_PARAM_BEEP,
  CONTROL_MSG_CODE_SET_PARAM_BEEP,
  CONTROL_MSG_CODE_GET_PRESET_CURRENT,
  CONTROL_MSG_CODE_SET_PRESET_CURRENT,
  CONTROL_MSG_CODE_GET_PRESET_VOLTAGE,
  CONTROL_MSG_CODE_SET_PRESET_VOLTAGE, 
  CONTROL_MSG_CODE_CNT
} control_msg_code_t;

/* 
  CONTROL_MSG_CODE_REMOTE_OVERRIDE
  CONTROL_MSG_CODE_START_STOP_FAN 
  CONTROL_MSG_CODE_START_STOP_SWITCH
*/
typedef struct _control_msg_body_enable_t
{
  uint8_t enable;
} PROTO_PACKED control_msg_body_enable_t;

/*
  CONTROL_MSG_CODE_GET_XMETER_STATUS
*/
typedef struct _control_msg_body_status_t
{
  uint8_t override_on;
  uint8_t fan_on;
  uint8_t switch_on;
  uint8_t cc_on; 
} PROTO_PACKED control_msg_body_status_t;
/* 
  CONTROL_MSG_GET_ADC_XX 
  CONTROL_MSG_SET_DAC_XX
  CONTROL_MSG_GET_POWER_XX
*/
typedef struct _control_msg_body_xmeter_t
{
  xmeter_value_t xmeter_val;
} PROTO_PACKED control_msg_body_xmeter_t;

/*
  CONTROL_MSG_CODE_GET_STEPS_XX
*/
typedef struct _control_msg_body_steps_t
{
  xmeter_value_t coarse;
  xmeter_value_t fine;  
} PROTO_PACKED control_msg_body_steps_t;

/* 
  CONTROL_MSG_GET_PARAM_XX
  CONTROL_MSG_SET_PARAM_XX
*/
typedef struct _control_msg_body_param_t
{
  uint8_t  uint8_val;
  xmeter_value_t xmeter_val;
} PROTO_PACKED control_msg_body_param_t;

/* 
  CONTROL_MSG_GET_PRESET_XX
  CONTROL_MSG_SET_PRESET_XX
*/
typedef struct _control_msg_body_preset_t
{
  
  uint8_t  preset_index;
  xmeter_value_t xmeter_val;
} PROTO_PACKED control_msg_body_preset_t;

typedef union _control_msg_body
{
  control_msg_body_enable_t    enable;
  control_msg_body_status_t    status;
  control_msg_body_xmeter_t    xmeter;
  control_msg_body_steps_t     steps;
  control_msg_body_param_t     param;
  control_msg_body_preset_t    preset;
} PROTO_PACKED control_msg_body_t;


typedef enum _control_msg_status_t
{
  CONTROL_MSG_STATUS_SUCCESS = 0,
  CONTROL_MSG_STATUS_ERROR   = 1, 
  CONTROL_MSG_STATUS_TIMEOUT = 2,
  CONTROL_MSG_STATUS_BUSY    = 3,
  CONTROL_MSG_STATUS_PROTO   = 4,
  CONTROL_MSG_STATUS_LOCKED  = 5,
}PROTO_PACKED control_msg_status_t;

typedef struct _control_msg_header_t
{
  uint16_t msg_magic;
  uint8_t  msg_code;   // control_msg_code_t
  uint8_t  msg_status; // control_msg_status_t
  uint8_t  msg_channel;
  uint8_t  msg_crc;
  uint8_t  msg_body_len;
}PROTO_PACKED control_msg_header_t;

typedef struct _control_msg_t
{
  control_msg_header_t  msg_header;
  control_msg_body_t    msg_body;
} PROTO_PACKED control_msg_t;

#if defined ( __C51__ ) || (defined ( __X86__ ))
#define CONTROL_MSG_MAGIC_CODE 0x1234 /* for C51 or X86, big-endian*/
#else
#define CONTROL_MSG_MAGIC_CODE 0x3412 /* for ARM, little-endian*/
#endif

#define CONTROL_MSG_MAX_CHANNEL 4

void control_fill_msg_crc(control_msg_t * msg);
bool control_verify_msg_crc(const control_msg_t * msg);
bool control_verify_msg(const control_msg_t * msg, uint8_t len);

double xmeter_val2float(xmeter_value_t * val);
void xmeter_float2val(double val, xmeter_value_t * out, uint8_t res);

#ifdef __cplusplus
}
#endif


#endif