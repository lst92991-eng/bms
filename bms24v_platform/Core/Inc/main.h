/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SC8815_INT_Pin GPIO_PIN_5
#define SC8815_INT_GPIO_Port GPIOA
#define SC8815_SW_I2C_SDA_Pin GPIO_PIN_6
#define SC8815_SW_I2C_SDA_GPIO_Port GPIOA
#define SC8815_SW_I2C_SCL_Pin GPIO_PIN_7
#define SC8815_SW_I2C_SCL_GPIO_Port GPIOA
#define SC8815_PSTOP_Pin GPIO_PIN_0
#define SC8815_PSTOP_GPIO_Port GPIOB
#define SC8815_CE_N_Pin GPIO_PIN_1
#define SC8815_CE_N_GPIO_Port GPIOB
#define LED_RED_Pin GPIO_PIN_13
#define LED_RED_GPIO_Port GPIOB
#define LED_GREEN_Pin GPIO_PIN_14
#define LED_GREEN_GPIO_Port GPIOB
#define BMS_MUX_BTN_Pin GPIO_PIN_3
#define BMS_MUX_BTN_GPIO_Port GPIOD
#define BMS_WAKE_DRV_Pin GPIO_PIN_3
#define BMS_WAKE_DRV_GPIO_Port GPIOB
#define BQ_INT_Pin GPIO_PIN_4
#define BQ_INT_GPIO_Port GPIOB
#define BQ_INT_EXTI_IRQn EXTI4_15_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
