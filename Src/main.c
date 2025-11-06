/**
******************************************************************************
* @file    main.c
* @author  GPM Application Team
*
******************************************************************************
* @attention
*
* Copyright (c) 2023 STMicroelectronics.
* All rights reserved.
*
* This software is licensed under terms that can be found in the LICENSE file
* in the root directory of this software component.
* If no LICENSE file comes with this software, it is provided AS-IS.
*
******************************************************************************
*/

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "app_fuseprogramming.h"
#include "main.h"
#include "misc_toolbox.h"
#include "stm32n6xx_nucleo.h"
#include "system_clock_config.h"
#ifndef TEST_CONNECTION_MODE
#include "npu_cache.h"
#endif
#include <stdio.h>
#if defined(USE_NS_TIMER) && (USE_NS_TIMER == 1)
#include "timer_config.h"
#endif

static void init_external_memories(void);
extern int ei_main(void);
extern void ei_classify_callback(uint8_t *image_data, uint32_t size);

#define IMAGE_BUFFER_SIZE 1024U
#define LED_BLINK_INTERVAL_MS 50U

extern uint8_t image_buffer[];

void LED_BLUE_Blink(void)
{
  static uint32_t last_toggle_tick = 0U;
  uint32_t current_tick = HAL_GetTick();

  if (current_tick - last_toggle_tick >= LED_BLINK_INTERVAL_MS)
  {
    last_toggle_tick = current_tick;
    BSP_LED_Toggle(LED_BLUE);
  }
}

extern volatile uint8_t rx_complete;

void ProcessUartReception(void)
{
  if (rx_complete)
  {
    rx_complete = 0;
    ei_classify_callback(image_buffer, IMAGE_BUFFER_SIZE);
    HAL_UART_Receive_IT(&hlpuart1, image_buffer, IMAGE_BUFFER_SIZE);
  }
}

int main(void)
{
  set_vector_table_addr();

  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_ICACTIVE_Msk;
  /* Set back system and CPU clock source to HSI */
  __HAL_RCC_CPUCLK_CONFIG(RCC_CPUCLKSOURCE_HSI);
  __HAL_RCC_SYSCLK_CONFIG(RCC_SYSCLKSOURCE_HSI);
  HAL_Init();
  system_init_post();

  SCB_EnableICache();
#if defined(USE_DCACHE)
  /* Power on DCACHE */
  MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk;
  SCB_EnableDCache();
#endif

/* Configure the system clock */
#if (NUCLEO_N6_CONFIG == 1)
  SystemClock_Config_Nucleo();
#elif VDDCORE_OVERDRIVE == 1
  upscale_vddcore_level();
  SystemClock_Config_HSI_overdrive();
#else
  SystemClock_Config_HSI_no_overdrive();
#endif

#ifndef TEST_CONNECTION_MODE
  NPU_Config();
#endif

#if defined(USE_NS_TIMER) && (USE_NS_TIMER == 1)
  timer_config_init();
#endif

  Fuse_Programming();

  init_external_memories();

  RISAF_Config();
  IAC_Config();

  set_clk_sleep_mode();

  /* Initialize LEDs */
  BSP_LED_Init(LED_BLUE);
  BSP_LED_Init(LED2);

  UART_Config();
  UART_Interrupt_Config();
  SystemIsolation_Config();

  /* Start interrupt-based reception */
  if (HAL_UART_Receive_IT(&hlpuart1, image_buffer, IMAGE_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }

#ifndef TEST_CONNECTION_MODE
  printf("Starting Edge Impulse...\n");
  ei_main();
#endif

  return 0;
}

void IAC_IRQHandler(void)
{
  while (1)
  {
  }
}

/* Allow to debug with cache enable */
__attribute__((section(".keep_me"))) void app_clean_invalidate_dbg()
{
  SCB_CleanInvalidateDCache();
}

static void init_external_memories(void)
{
#if defined(USE_EXTERNAL_MEMORY_DEVICES) && USE_EXTERNAL_MEMORY_DEVICES == 1
  BSP_XSPI_NOR_Init_t Flash;

#if (NUCLEO_N6_CONFIG == 0)
  BSP_XSPI_RAM_Init(0);
  BSP_XSPI_RAM_EnableMemoryMappedMode(0);
#endif

  Flash.InterfaceMode = BSP_XSPI_NOR_OPI_MODE;
  Flash.TransferRate = BSP_XSPI_NOR_DTR_TRANSFER;

  if (BSP_XSPI_NOR_Init(0, &Flash) != BSP_ERROR_NONE)
  {
    __BKPT(0);
  }
  if (BSP_XSPI_NOR_EnableMemoryMappedMode(0) != BSP_ERROR_NONE)
  {
    __BKPT(0);
  }
#endif
}

#ifdef USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  UNUSED(file);
  UNUSED(line);
  __BKPT(0);
  while (1)
  {
  }
}
#endif
