#include "control.h"
#include "debug.h"
#include "usart.h"
#include "usbd_cdc_if.h"
#include "gpio.h"

#include <string.h>


void control_dump_msg(const char * fmt, const control_msg_t * msg)
{
  uint8_t * p = (uint8_t *)(&(msg->msg_body));
  uint8_t body_len, print_len, i;
	CDBG("control_dump_msg %s", fmt);
  CDBG("  msg_header.msg_magic:    %04x", msg->msg_header.msg_magic);  
  CDBG("  msg_header.msg_code:     %d", msg->msg_header.msg_code);
  CDBG("  msg_header.msg_status:   %d", msg->msg_header.msg_status);
  CDBG("  msg_header.msg_channel:  %d", msg->msg_header.msg_channel);
  CDBG("  msg_header.msg_crc:      %d", msg->msg_header.msg_crc);
  CDBG("  msg_header.msg_body_len: %d", msg->msg_header.msg_body_len);
  if(msg->msg_header.msg_body_len > 0) {
    CDBG("  msg_body:"); 
    body_len = msg->msg_header.msg_body_len;
    do {
      print_len = body_len > 16 ? 16 : body_len;
      for(i = 0; i < print_len ; i ++) {
        CDBG_R("%02x ", p[i]);
      }
      CDBG_R("\r\n");
      body_len -= print_len;
      p += print_len;
    }while(body_len);
  }
}

bool control_parse_cmd(control_msg_t * cmd, const uint8_t * buffer, uint32_t len)
{
  control_msg_t * pcmd = (control_msg_t *)buffer;
  
  if(!control_verify_msg(pcmd, len)) {
    return false;
  }
  
  memcpy(cmd, pcmd, pcmd->msg_header.msg_body_len + sizeof(control_msg_header_t));
  
  control_dump_msg("receive cmd", cmd);
  
  return true;
}

static void control_select_channel(uint8_t channel)
{
  switch(channel)
  {
    case 0:
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL0_Pin|CHANEL_SEL1_Pin, GPIO_PIN_RESET);
      break;
    case 1:
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL0_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL1_Pin, GPIO_PIN_SET);
      break; 
    case 2:
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL0_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL1_Pin, GPIO_PIN_RESET);
      break; 
    case 3:
      HAL_GPIO_WritePin(GPIOA, CHANEL_SEL0_Pin|CHANEL_SEL1_Pin, GPIO_PIN_SET);
      break;
  }
}

static uint8_t control_hal_error(HAL_StatusTypeDef status)
{
  uint8_t ret;
  
  switch(status){
    case HAL_OK: 
      ret =  CONTROL_MSG_STATUS_SUCCESS;
      break;
    case HAL_ERROR: 
      return CONTROL_MSG_STATUS_ERROR;
      break;
    case HAL_BUSY:
      return CONTROL_MSG_STATUS_BUSY;
      break;
    case HAL_TIMEOUT: 
      return CONTROL_MSG_STATUS_TIMEOUT;
      break; 
    default:
      ret = CONTROL_MSG_STATUS_ERROR;
  }
  return ret;
}

/*
  send msg to channel and get response
*/
void control_relay_cmd(const control_msg_t * cmd, control_msg_t * res)
{
  HAL_StatusTypeDef status;
  uint8_t error_code;
  control_select_channel(cmd->msg_header.msg_channel);
  
  status = USART2_Transmit((uint8_t *)cmd, sizeof(control_msg_t));
  
  if(HAL_OK != status) {
    error_code = control_hal_error(status);
    CERR("control_relay_cmd: USART2_Transmit failed %d", status);
    goto err;
  }
  
  status = USART2_Receive((uint8_t *)res, sizeof(control_msg_t));
  
  if(HAL_OK != status) {
    error_code = control_hal_error(status);
    CERR("control_relay_cmd: USART2_Receive failed %d", status);
    goto err;
  }
  
  if(!control_verify_msg(res, sizeof(control_msg_t)) || (res->msg_header.msg_code & 0x80) == 0) {
    error_code = CONTROL_MSG_STATUS_PROTO;
    CERR("control_relay_cmd: invalid response");
    goto err;
  }
  
  if((cmd->msg_header.msg_code & 0x7F) != (res->msg_header.msg_code & 0x7F)) {
    error_code = CONTROL_MSG_STATUS_PROTO;
    CERR("control_relay_cmd: cmd & response mis-match");
    goto err;
  }
  
  return;
  
err:  
  memcpy(res, cmd, sizeof(control_msg_t));
  res->msg_header.msg_code |= 0x80;
  res->msg_header.msg_status = error_code;
  res->msg_header.msg_body_len = 0;
  control_fill_msg_crc(res);
}

/*
  send response to computer
*/

bool control_send_response(const control_msg_t * res)
{
  uint8_t status = CDC_Transmit_FS((uint8_t *)res, sizeof(control_msg_t));
	
  if(status != USBD_OK) {
    CERR("CDC_Transmit_FS failed: %d", status);
  }
  
  return status == USBD_OK;
}

void control_send_heatbeat(control_msg_t * cmd, control_msg_t * res, uint8_t index)
{
  CDBG("control_send_heatbeat to channel %d", index);
  memset(cmd, 0, sizeof(control_msg_t));
  memset(res, 0, sizeof(control_msg_t));
  
  cmd->msg_header.msg_body_len = 0;
  cmd->msg_header.msg_channel  = 0;  
  cmd->msg_header.msg_code     = CONTROL_MSG_CODE_HEATBEAT;  
  cmd->msg_header.msg_crc      = 0; 
  cmd->msg_header.msg_magic    = CONTROL_MSG_MAGIC_CODE;  
  cmd->msg_header.msg_status   = CONTROL_MSG_STATUS_SUCCESS;
  
  cmd->msg_header.msg_channel  = index; 
  control_fill_msg_crc(cmd);
  control_relay_cmd(cmd, res);
}




