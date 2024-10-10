#ifndef __DELAY_H__
#define __DELAY_H__

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif
	
uint8_t delay_init(void);

__STATIC_INLINE void delay_us(volatile uint32_t us)
{
  uint32_t clk_cycle_start = DWT->CYCCNT;

  /* Go to number of cycles for system */
  us *= (HAL_RCC_GetHCLKFreq() / 1000000);

  /* Delay till end */
  while ((DWT->CYCCNT - clk_cycle_start) < us);
} 

__STATIC_INLINE  void delay_ms(uint32_t ms)
{
  uint32_t i;
  for(i = 0 ; i < ms ; i ++) {
    delay_us(1000);
  }
}

#ifdef __cplusplus
}
#endif

#endif