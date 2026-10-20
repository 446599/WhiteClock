/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 CW.
  * All rights reserved.</center></h2>
  *
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "cw32l010.h"
#include "cw32l010_gpio.h"
#include "cw32l010_spi.h"
#include "cw32l010_rtc.h"
#include "cw32l010_sysctrl.h"
#include "cw32l010_systick.h"
#include "interrupts_cw32l010.h"
#include "system_cw32l010.h"

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
/* USER CODE BEGIN EFP */
/* USER CODE END EFP */


/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */
#define EPD_BUSY_PIN                 GPIO_PIN_1  /* PA1 */
#define EPD_RST_PIN                  GPIO_PIN_2  /* PA2 */
#define EPD_CS_PIN                   GPIO_PIN_3  /* PA3 */
#define EPD_DC_PIN                   GPIO_PIN_4  /* PA4 */
#define EPD_SPI_SCK_PIN              GPIO_PIN_5  /* PA5, AF3 */
#define EPD_SPI_MOSI_PIN             GPIO_PIN_6  /* PA6, AF3 */

#define EPD_WIDTH                    152U
#define EPD_HEIGHT                   296U
#define EPD_ROW_BYTES                ((EPD_WIDTH + 7U) / 8U)
/* USER CODE END Private defines */


#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT CW *****END OF FILE****/
