#include "protocol.h"
#include "debug.h"

void control_fill_msg_crc(control_msg_t * msg)
{
  uint8_t * p = (uint8_t *)msg;
  uint8_t index, crc = 0;
  uint8_t len;
  msg->msg_header.msg_crc = 0;
  len = msg->msg_header.msg_body_len + sizeof(control_msg_header_t);
  if(len > sizeof(control_msg_t)) {
    len = sizeof(control_msg_t);
  }
  for(index = 0 ; index < len; index ++) {
    crc += p[index];
  }
  msg->msg_header.msg_crc = (~crc) + 1;
}

bool control_verify_msg_crc(const control_msg_t * msg)
{
  uint8_t * p = (uint8_t *)msg;
  uint8_t index, crc = 0;
  uint8_t len;
  len = msg->msg_header.msg_body_len + sizeof(control_msg_header_t);
  if(len > sizeof(control_msg_t)) {
    len = sizeof(control_msg_t);
  }
  for(index = 0 ; index < len; index ++) {
    crc += p[index];
  }
  return crc == 0;
}

bool control_verify_msg(const control_msg_t * msg, uint8_t len)
{
  if(len < sizeof(control_msg_header_t)) {
    CDBG("invalid msg: too short");
    return false;
  }
  
  if(!control_verify_msg_crc(msg)) {
    CDBG("invalid msg: crc error");
    return false;
  }
  
  if(len < sizeof(control_msg_header_t) + msg->msg_header.msg_body_len) {
    CDBG("invalid msg: too short");
    return false;
  }  
  
  if(msg->msg_header.msg_magic != CONTROL_MSG_MAGIC_CODE) {
    CDBG("invalid msg: magic mis-match");
    return false;
  }
  
  if((msg->msg_header.msg_code & 0x7F) >= CONTROL_MSG_CODE_CNT 
    || (msg->msg_header.msg_code & 0x7F) == 0) {
    CDBG("invalid msg: invalid msg_code");
    return false;
  }
    
  if(msg->msg_header.msg_channel >= CONTROL_MSG_MAX_CHANNEL) {
    CDBG("invalid msg: invalid channel");
    return false;
  }  
  
  return true;
}

void xmeter_float2val(double val, xmeter_value_t * out, uint8_t res)
{
  out->neg = val < 0 ? 1 : 0;
  
  out->res = res;
  
  if(out->neg) {
    val = 0.0 - val;
  }
  out->integer = (uint16_t) val;
  
  if(out->res == 0) {
    out->decimal = 0;
  } else if(out->res == 1) {
    out->decimal = (val - out->integer) * 10.0;
    out->decimal = out->decimal * 100;
  } else if(out->res == 2) {
    out->decimal = (val - out->integer) * 100.0;
    out->decimal = out->decimal * 10;
  } else {
    out->decimal = (val - out->integer) * 1000.0;
  }
  
  if(out->integer > 999)
    out->integer = 999;
  
  if(out->decimal > 999)
    out->decimal = 999;
  
  if(out->integer == 0 && out->decimal == 0) {
    out->neg = 0;
  }
}

double xmeter_val2float(xmeter_value_t * val)
{
  double ret;
  
  ret = val-> integer + val->decimal / 1000.0;
  
  if(val->neg) {
    ret = 0.0 - ret;
  }
  
  return ret;
}
