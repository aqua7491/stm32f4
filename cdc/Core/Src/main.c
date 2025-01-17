
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2020 STMicroelectronics International N.V.
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_hal.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

I2S_HandleTypeDef hi2s3;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
//USB
uint8_t usb_rx[512];
uint8_t usb_tx[512];
volatile uint32_t ir;
volatile uint32_t iw;
volatile int8_t usb_rxne = RESET; //RESET: no data in usb_rx to read
volatile int8_t usb_txe = SET;    //RESET: no space in usb_tx to load data to write
int tx_len = 0;
int ret_tran = 0;

//UART
uint8_t uart_rx[8];
uint8_t uart_rxc[2];
uint8_t uart_tx[8];
int iurx;
int iutx;
int8_t uart_dump = RESET; 

//SPI
uint8_t spi_rx[512];
uint8_t spi_tx[512];
int8_t enable_acc = RESET; 
int8_t enable_bridge_acc = RESET; 

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2S3_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
void LOGD(uint8_t *msg, int len);
void on_CDC_Receive_FS(uint32_t len);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
/**
  * @brief  Rx Transfer completed callbacks.
  * @param  huart  Pointer to a UART_HandleTypeDef structure that contains
  *                the configuration information for the specified UART module.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	//add received data to uart rx buffer
	iurx = iurx%8;
	uart_rx[iurx] = uart_rxc[0];

#if 1 //print uart rx	buffer
	char msg[128];
	int len = 0;	
	sprintf(msg, "UART received:0x%2x at index:%d\n", uart_rxc[0], iurx);
	len = strlen(msg);
	if (len < 0 || len > 128) len = 0;
	//HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100*len);
	LOGD((uint8_t*)msg, len);
#endif

	if (uart_rxc[0] == 'A') {
		enable_acc = SET;
	}
	else if (uart_rxc[0] == 'B') {
		enable_acc = RESET;
		uart_dump = RESET;
	}
	else if (uart_rxc[0] == 'C') {
		uart_dump = SET;
	}
	else if (uart_rxc[0] == 'D') {
		enable_bridge_acc = SET;
	}
	
	iurx += 1;
}

void LOGD(uint8_t *msg, int len) {
	HAL_UART_Transmit(&huart2, (uint8_t*)msg, len, 100*len);
}

void on_CDC_Receive_FS(uint32_t len) {
	ret_tran = 0;	
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	if (usb_rxne != SET) return;
	extern int8_t CDC_is_busy(void);
	if (CDC_is_busy()) return;
	
#if 1 //uart print	
	char msg[64];
	sprintf(msg, "[%s] USB ENDPOINT RECV:%d bytes\n", __func__, len);
	LOGD((uint8_t*)msg, strlen(msg));
#endif
	
	if (enable_bridge_acc == SET) {
		HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
		memcpy(spi_tx, usb_rx, len);
#if 1
		HAL_SPI_Transmit(&hspi1, spi_tx, len, 50*len);
		int i = 0x100;
		while(i--) {__NOP();}
		HAL_SPI_Receive(&hspi1, spi_rx, len, 50*len);
#else		
		HAL_SPI_TransmitReceive(&hspi1, spi_tx, spi_rx, len, 50*len);
#endif		
		HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
		memcpy(usb_tx, spi_rx, len);
		CDC_Transmit_FS(usb_tx, len);
	}
	else {
#if 0 //USB loopback method 1, fast but can not support more than 64 bytes
		memcpy(usb_tx, usb_rx, len);	
		ret_tran = CDC_Transmit_FS(usb_tx, len);
#if 1 //uart print	
		memset(msg, 0, 64);
		sprintf(msg, "[%s] USB ENDPOINT TRAN:%d bytes (ret:%d)\n", __func__, len, ret_tran);
		LOGD((uint8_t*)msg, strlen(msg));
#endif
#elif 1	//USB loopback method 2, transmit later but can support more than 64 bytes	
		if (iw < 512) {
			memcpy(usb_tx+iw, usb_rx, len);
			iw += len;
			tx_len += len;
		}
		else {
			memset(usb_tx, 0, 512);
			iw = 0;
			tx_len = 0;
		}
#else
			__NOP();
#endif		
	}
	
	ir = 0;
	usb_rxne = RESET;
	HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
}

void init_ACC() {
	//acc config
	HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
	spi_tx[0] = 0x20;
	spi_tx[1] = 0x11;
	HAL_SPI_Transmit(&hspi1, spi_tx, 2, 50*2);
	HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
	//acc read for checking config 
	HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
	spi_tx[0] = 0x80|0x20; 
	HAL_SPI_Transmit(&hspi1, spi_tx, 1, 50*1);
	HAL_SPI_Receive(&hspi1, spi_rx, 1, 50*1);
#if 1 //uart print	
	char msg[64];
	sprintf(msg, "CTRL_REG4:0x%x\n", spi_rx[0]);
	LOGD((uint8_t*)msg, strlen(msg));
#endif	
	HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2S3_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	memset(uart_rx, 0xff, 8);
	memset(uart_tx, 0xff, 8);
	char msg[16*8];
	HAL_UART_Receive_DMA(&huart2, uart_rxc, 1);
	
	init_ACC();
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		//spi task cycle
		if (enable_acc == SET) {
			uint16_t x = 0;
#if 1
			//acc read x HB value		
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
			spi_tx[0] = 0x80|0x29; 
			HAL_SPI_Transmit(&hspi1, spi_tx, 1, 50*1);
			HAL_SPI_Receive(&hspi1, spi_rx, 1, 50*1);
			x |= spi_rx[0]<<8;
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
			//acc read x LB value		
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
			spi_tx[0] = 0x80|0x28; 
			HAL_SPI_Transmit(&hspi1, spi_tx, 1, 50*1);
			HAL_SPI_Receive(&hspi1, spi_rx, 1, 50*1);
			x |= spi_rx[0]<<0;
			sprintf(msg, "X:%d(0x%x)\n", (int16_t)x, x);
			LOGD((uint8_t*)msg, strlen(msg));
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
#else
			//acc read x LB value		
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_RESET); //CS ON
			spi_tx[0] = 0x80|0x28;
			spi_tx[1] = 0x0;
			HAL_SPI_TransmitReceive(&hspi1, spi_tx, spi_rx, 2, 50*2);
			x |= spi_rx[1]<<8 | spi_rx[0];
			sprintf(msg, "X:%d(0x%x)\n", (int16_t)x, x);
			LOGD((uint8_t*)msg, strlen(msg));
			HAL_GPIO_WritePin(GPIOE, CS_I2C_SPI_Pin, GPIO_PIN_SET); //CS OFF
#endif			
		}
		//enable_acc = RESET;
		
		//usb task cycle
		
		//uart task cycle
		if (uart_dump == SET) {
			char msg[16*8];
			char c[8];
			int l = 0;
			sprintf(msg, "UART RX:\n");
			for (int i=0; i<8; i++) {
				sprintf(c, "[%2x]", uart_rx[i]);
				strcat(msg, c);
				if ((i+1)%(4) == 0) {
					strcat(msg, "\n");
				}
			}
			strcat(msg, "\n");
			l = strlen(msg);
			if (l < 0 || l > sizeof(msg)) l = sizeof(msg);
			HAL_UART_Transmit(&huart2, (uint8_t*)msg, l, 100*l);			
		}
		
		if (iw != 0) {
			usb_txe = RESET;
			ret_tran = CDC_Transmit_FS(usb_tx, tx_len);
#if 1 //uart print	
			memset(msg, 0, 64);
			sprintf(msg, "[%s] USB ENDPOINT TRAN:%d bytes (ret:%d)\n", __func__, tx_len, ret_tran);
			LOGD((uint8_t*)msg, strlen(msg));
#endif			
			iw = 0;
			tx_len = 0;
			usb_txe = SET;
		}
		else {
			usb_txe = SET;
		}
				
		HAL_Delay(100);
		/* USER CODE END WHILE */
  /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */

}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    /**Configure the main internal regulator output voltage
    */
  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 192;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* I2C1 init function */
static void MX_I2C1_Init(void)
{

  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* I2S3 init function */
static void MX_I2S3_Init(void)
{

  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_96K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  //hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	//hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

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
    _Error_Handler(__FILE__, __LINE__);
  }

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

}

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
     PC3   ------> I2S2_SD
     PB10   ------> I2S2_CK
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, CS_SPI_Pin|CS_I2C_SPI_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : CS_SPI_Pin CS_I2C_SPI_Pin */
  GPIO_InitStruct.Pin = CS_SPI_Pin|CS_I2C_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : EXTI_SPI_Pin */
  GPIO_InitStruct.Pin = EXTI_SPI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(EXTI_SPI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  file: The file name as string.
  * @param  line: The line in file as a number.
  * @retval None
  */
void _Error_Handler(char *file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
