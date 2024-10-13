/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "usart.h"
#include "delay.h"
/* USER CODE BEGIN 0 */

#define MX_USART_TRANSMIT_TIMEOUT 200 //ms
#define MX_USART_RECEIVE_TIMEOUT  200 //ms
#define MX_USART_BUSY_TRY_CNT     3
#define MX_USART_BUSY_WAIT_DELAT  10 //ms
/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
static uint8_t USART2_Receive_Cplt;
/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_MspInit 1 */
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE END USART2_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);
    /* USART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspDeInit 1 */
    
  /* USER CODE END USART2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

static HAL_StatusTypeDef MX_USART_Transmit(UART_HandleTypeDef * huart, uint8_t *pData, uint16_t Size)
{
  uint8_t try_cnt = MX_USART_BUSY_TRY_CNT;
  HAL_StatusTypeDef status;
  do {
    status = HAL_UART_Transmit(huart, pData, Size, MX_USART_TRANSMIT_TIMEOUT);
    delay_ms(MX_USART_BUSY_WAIT_DELAT);
  }while(status == HAL_BUSY && try_cnt--);
  return  status;
}

static HAL_StatusTypeDef MX_USART_Receive(UART_HandleTypeDef * huart, uint8_t *pData, uint16_t Size)
{
  uint8_t try_cnt = MX_USART_BUSY_TRY_CNT;
  HAL_StatusTypeDef status;
  do {
    status = HAL_UART_Receive(huart, pData, Size, MX_USART_RECEIVE_TIMEOUT);
    delay_ms(MX_USART_BUSY_WAIT_DELAT);
  }while(status == HAL_BUSY && try_cnt--);
  return status; 
}

static HAL_StatusTypeDef MX_USART_Receive_IT(UART_HandleTypeDef * huart, uint8_t *pData, uint16_t Size)
{
  uint8_t try_cnt = MX_USART_BUSY_TRY_CNT;
  HAL_StatusTypeDef status;
  do {
    status = HAL_UART_Receive_IT(huart, pData, Size);
    delay_ms(MX_USART_BUSY_WAIT_DELAT);
  }while(status == HAL_BUSY && try_cnt--);
  return status; 
}

HAL_StatusTypeDef USART1_Transmit(uint8_t *pData, uint16_t Size)
{
  return MX_USART_Transmit(&huart1, pData, Size);
}

HAL_StatusTypeDef USART2_Transmit(uint8_t *pData, uint16_t Size)
{
  return MX_USART_Transmit(&huart2, pData, Size);
}

HAL_StatusTypeDef USART2_Receive_IT(uint8_t *pData, uint16_t Size)
{
  USART2_Receive_Cplt = 0;
  return MX_USART_Receive_IT(&huart2, pData, Size);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART2) {
    USART2_Receive_Cplt = 1;
  }
}

HAL_StatusTypeDef USART2_Receive_Wait(uint32_t TimeOut)
{
  uint32_t tickstart = 0U;
  
  tickstart = HAL_GetTick();
  
  while(1) {
    if(USART2_Receive_Cplt) {
      break;
    } else if((HAL_GetTick() - tickstart) > TimeOut || TimeOut == 0) {
      break;
    }
  }
  
  if(!USART2_Receive_Cplt) {
    HAL_UART_AbortReceive_IT(&huart2);
  }
  
  return USART2_Receive_Cplt ? HAL_OK : HAL_TIMEOUT;
  
}
/* USER CODE END 1 */
