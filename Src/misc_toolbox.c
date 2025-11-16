/**
******************************************************************************
* @file    misc_toolbox.c
* @author  GPM/AIS Application Team
* @brief   Collection of functions to perform main configurations in main.c
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

#include <string.h> // Used for memset

#include "app_config.h"
#include "misc_toolbox.h"
#include "stm32n6xx_ll_usart.h" // Used for configuring UART
#include <stdio.h>

UART_HandleTypeDef hlpuart1;

#define IMAGE_BUFFER_SIZE (32 * 32)
__attribute__((aligned(32)))
uint8_t image_buffer[IMAGE_BUFFER_SIZE];

volatile uint8_t rx_complete = 0;

#ifdef HAL_BSEC_MODULE_ENABLED
static void fuse_hardware_conf(uint32_t bit_to_fuse)
{
  uint32_t fuse_id, data, fuse_mask;
  BSEC_HandleTypeDef sBsecHandler;
  sBsecHandler.Instance = BSEC;
  fuse_mask = (1U << bit_to_fuse);
  /* Read current value of fuse */
  fuse_id = 124U; // HCONF1 OTP (see reference manual)
  if (HAL_BSEC_OTP_Read(&sBsecHandler, fuse_id, &data) == HAL_OK)
  {
    /* Check if bit has already been set */
    if ((data & fuse_mask) != fuse_mask)
    {
      data |= fuse_mask;
      /* Bitwise programming of lower bits */
      if (HAL_BSEC_OTP_Program(&sBsecHandler, fuse_id, data, HAL_BSEC_NORMAL_PROG) == HAL_OK)
      {
        /* Read lower bits to verify the correct programming */
        if (HAL_BSEC_OTP_Read(&sBsecHandler, fuse_id, &data) == HAL_OK)
        {
          if ((data & fuse_mask) != fuse_mask)
          {
            /* Error : Fuse programming not taken in account */
            while (1)
            {
            };
          }
        }
        else
        {
          /* Error : Fuse read unsuccessful */
          while (1)
          {
          };
        }
      }
      else
      {
        /* Error : Fuse programming unsuccessful */
        while (1)
        {
        };
      }
    }
  }
  else
  {
    /* Error  : Fuse read unsuccessful */
    while (1)
    {
    };
  }
}
#endif

void set_clk_sleep_mode(void)
{
  LL_BUS_EnableClockLowPower(~0);
  LL_MEM_EnableClockLowPower(~0);
  LL_AHB1_GRP1_EnableClockLowPower(~0);
  LL_AHB2_GRP1_EnableClockLowPower(~0);
  LL_AHB3_GRP1_EnableClockLowPower(~0);
  LL_AHB4_GRP1_EnableClockLowPower(~0);
  LL_AHB5_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP1_EnableClockLowPower(~0);
  LL_APB1_GRP2_EnableClockLowPower(~0);
  LL_APB2_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP1_EnableClockLowPower(~0);
  LL_APB4_GRP2_EnableClockLowPower(~0);
  LL_APB5_GRP1_EnableClockLowPower(~0);
  LL_MISC_EnableClockLowPower(~0);
}

void upscale_vddcore_level(void)
{
  uint8_t tmp = 0x64;
  BSP_I2C2_Init();
  BSP_I2C2_WriteReg(0x49 << 1, 0x01, &tmp, 1);
  HAL_Delay(1);
}

/* Initialises UART @  USE_UART_BAUDRATE
 * Configures GPIO pins for UART (
 * Enables clocks for UART/GPIO
 */
void UART_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  /* Initialize peripherals clock */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
  PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK4;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Peripheral clock enable */
  __HAL_RCC_LPUART1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* LPUART1 GPIO Configuration: PE5->LPUART1_TX, PE6->LPUART1_RX */
  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF3_LPUART1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* Peripheral config */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 9600;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV8;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
}

void UART_Interrupt_Config(void)
{
  /* Configure LPUART1 interrupt */
  HAL_NVIC_SetPriority(LPUART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);
}

void SystemIsolation_Config(void)
{
  /* set all required IPs as non-secure and non-privileged */
  __HAL_RCC_RIFSC_CLK_ENABLE();

  /* set up GPIO configuration */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_5, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (huart->Instance == LPUART1)
  {
    /* Initialize peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
    PeriphClkInitStruct.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK4;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* Enable peripheral clock */
    __HAL_RCC_LPUART1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* LPUART1 GPIO Configuration: PE5->LPUART1_TX, PE6->LPUART1_RX */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF3_LPUART1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* LPUART1 interrupt Init */
    HAL_NVIC_SetPriority(LPUART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(LPUART1_IRQn);
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == LPUART1)
  {
    /* Disable peripheral clock */
    __HAL_RCC_LPUART1_CLK_DISABLE();

    /* LPUART1 GPIO Configuration: PE5->LPUART1_TX, PE6->LPUART1_RX */
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_5 | GPIO_PIN_6);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == LPUART1)
  {
    rx_complete = 1;

    const char *ackMsg = "Received 1024 bytes\r\n";
    printf("%s", ackMsg);
  }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart)
{
  // Not used in interrupt mode
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == LPUART1)
  {
    // Clear error flags
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF);

    // Turn on red LED to indicate error
    BSP_LED_On(LED2);

    // Reset reception variables
    rx_complete = 0;

    // Restart interrupt-based reception
    HAL_UART_Receive_IT(huart, image_buffer, IMAGE_BUFFER_SIZE);
  }
}

void RISAF_Config(void)
{
  __HAL_RCC_RIFSC_CLK_ENABLE();
  RIMC_MasterConfig_t RIMC_master = {0};
  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &RIMC_master);
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_NPU, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_SPI5, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
}

void set_vector_table_addr(void)
{
  __disable_irq();
  SCB->VTOR = 0x34000000; // USED WITH no FLEXMEM Extension (standard scenario)
  // SCB->VTOR = 0x10000000; // USED WITH FLEXMEM Extension  (execution from ITCM only)
  /* Set default Vector Table location after system reset or return from Standby */
  // SYSCFG->INITSVTORCR = SCB->VTOR;
  __DSB();
  memset((uint32_t *)NVIC->ICER, 0xFF, sizeof(NVIC->ICER)); // Disable all irq (IRQ 139 is enabled by default)
  memset((uint32_t *)NVIC->ICPR, 0xFF, sizeof(NVIC->ICPR)); // Clear pending IRQs (LPTIM4 has a pending IRQ when exiting bootrom)
  __enable_irq();
}

void system_init_post(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_CRC_CLK_ENABLE();

  /* Enable NPU RAMs (4x448KB) + CACHEAXI */
  RCC->MEMENR |= RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN | RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN;
  RCC->MEMENR |= RCC_MEMENR_CACHEAXIRAMEN; // RCC_MEMENR_NPUCACHERAMEN;

  RAMCFG_SRAM2_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
  RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;

  /* Allow caches to be activated. Default value is 1, but the current boot sets it to 0 */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk | MEMSYSCTL_MSCR_ICACTIVE_Msk;
}

void IAC_Config(void)
{
  /* Configure IAC to trap illegal access events */
  __HAL_RCC_IAC_CLK_ENABLE();
  __HAL_RCC_IAC_FORCE_RESET();
  __HAL_RCC_IAC_RELEASE_RESET();
}

#ifdef HAL_BSEC_MODULE_ENABLED
void fuse_vddio(void)
{
  // Fuse bit for VDDIO2 (HSLV_VDDIO2): used for PSRAM / XSPIM 1
  fuse_hardware_conf(16);
  // Fuse bit for VDDIO3 (HSLV_VDDIO3): used for external Flash / XSPIM2
  fuse_hardware_conf(15);
}
#endif

void Error_Handler(void)
{
  BSP_LED_On(LED2);
  __disable_irq();
  while (1)
  {
  }
}
