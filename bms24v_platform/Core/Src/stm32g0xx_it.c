/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "main.h"
#include "stm32g0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "App_DebugCli.h"
#include "App_BatMan.h"
#include "App_Safety.h"
#include "Int_Fault.h"
#include "Int_Log.h"
#include "Int_SC8815.h"
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

#if defined(__GNUC__)
void HardFault_Handler(void) __attribute__((naked));
#endif

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#if defined(__CC_ARM)
__asm void HardFault_Handler(void)
{
  IMPORT Int_Fault_HandleHardFault
  MOV r1, lr
  MOVS r2, #4
  TST r1, r2
  BEQ hard_fault_use_msp
  MRS r0, PSP
  B hard_fault_dispatch
hard_fault_use_msp
  MRS r0, MSP
hard_fault_dispatch
  LDR r3, =Int_Fault_HandleHardFault
  BX r3
  ALIGN
}

/* CubeMX 生成的 C 壳仅保留作未引用后备，向量表使用上面的裸汇编强符号。 */
#define HardFault_Handler HardFault_Handler_GeneratedUnused
#endif

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim14;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  Int_Fault_Panic(INT_FAULT_NMI, __get_MSP());
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
#if defined(__GNUC__)
  __asm volatile (
      "mov r1, lr\n"
      "movs r2, #4\n"
      "tst r1, r2\n"
      "beq 1f\n"
      "mrs r0, psp\n"
      "b 2f\n"
      "1:\n"
      "mrs r0, msp\n"
      "2:\n"
      "ldr r3, =Int_Fault_HandleHardFault\n"
      "bx r3\n");
#elif defined(__CC_ARM)
  Int_Fault_Panic(INT_FAULT_HARDFAULT, __get_MSP());
#else
  Int_Fault_HandleHardFault((const uint32_t *)__get_MSP(), 0u);
#endif
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/******************************************************************************/
/* STM32G0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line 4 to 15 interrupts.
  */
void EXTI4_15_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI4_15_IRQn 0 */

  /* USER CODE END EXTI4_15_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(SC8815_INT_Pin);
  HAL_GPIO_EXTI_IRQHandler(BQ_INT_Pin);
  /* USER CODE BEGIN EXTI4_15_IRQn 1 */

  /* USER CODE END EXTI4_15_IRQn 1 */
}

/**
  * @brief This function handles TIM14 global interrupt.
  */
void TIM14_IRQHandler(void)
{
  /* USER CODE BEGIN TIM14_IRQn 0 */

  /* USER CODE END TIM14_IRQn 0 */
  HAL_TIM_IRQHandler(&htim14);
  /* USER CODE BEGIN TIM14_IRQn 1 */

  /* USER CODE END TIM14_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/* USER CODE BEGIN 1 */

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == SC8815_INT_Pin)
  {
    /* INT 原因未判定前先撤销功率，再把寄存器判因留给 SC 任务。 */
    Int_SC8815_ForceStandby();
    App_Safety_OnScInterruptFromISR();
    Int_SC8815_NotifyInterruptFromISR();
  }
  else if (GPIO_Pin == BQ_INT_Pin)
  {
    /* ALERT 先撤销功率与令牌，只锁存事件；完整 Safety/PF/Alarm 帧在任务中判因。 */
    Int_SC8815_ForceStandby();
    App_Safety_OnBqAlertFromISR();
    App_BatMan_NotifyAlertFromISR();
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  /* 唯一 UART error 汇聚点：先释放 TX 环形缓冲状态，再恢复工程 RX。 */
  Int_Log_OnUartError(huart);
  if ((huart != NULL) && (huart->Instance == USART1))
  {
    App_DebugCli_OnUartError();
  }
}

/* USER CODE END 1 */
