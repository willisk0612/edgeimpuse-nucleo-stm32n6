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
#include "model-parameters/model_metadata.h"
#if defined(USE_NS_TIMER) && (USE_NS_TIMER == 1)
#include "timer_config.h"
#endif

static void init_external_memories(void);
extern int ei_main(void);
extern void ei_classify_callback(uint8_t *image_data, uint32_t size);

#define IMAGE_BUFFER_SIZE EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE
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

void ProcessUartReception(void)
{
  /* Parse framed UART stream: [0xAA][0x55][0x01][seq_lo][seq_hi][len_lo][len_hi][payload...] */
  enum
  {
    UART_STATE_SYNC0 = 0,
    UART_STATE_SYNC1,
    UART_STATE_TYPE,
    UART_STATE_SEQ_LO,
    UART_STATE_SEQ_HI,
    UART_STATE_LEN_LO,
    UART_STATE_LEN_HI,
    UART_STATE_PAYLOAD
  };

  static uint8_t  state      = UART_STATE_SYNC0;
  static uint16_t payload_len = 0U;
  static uint16_t payload_idx = 0U;
  static uint8_t  frame_buf[7U + IMAGE_BUFFER_SIZE];

  while (UART_RingBuffer_Available() > 0U)
  {
    uint8_t byte = 0U;
    (void)UART_RingBuffer_Read(&byte, 1U);

    switch (state)
    {
    case UART_STATE_SYNC0:
      if (byte == 0xAA)
      {
        frame_buf[0] = byte;
        state = UART_STATE_SYNC1;
      }
      break;

    case UART_STATE_SYNC1:
      if (byte == 0x55)
      {
        frame_buf[1] = byte;
        state = UART_STATE_TYPE;
      }
      else
      {
        state = UART_STATE_SYNC0;
      }
      break;

    case UART_STATE_TYPE:
      if (byte == 0x01)
      {
        frame_buf[2] = byte;
        state = UART_STATE_SEQ_LO;
      }
      else
      {
        state = UART_STATE_SYNC0;
      }
      break;

    case UART_STATE_SEQ_LO:
      frame_buf[3] = byte; /* seq_lo */
      state = UART_STATE_SEQ_HI;
      break;

    case UART_STATE_SEQ_HI:
      frame_buf[4] = byte; /* seq_hi */
      state = UART_STATE_LEN_LO;
      break;

    case UART_STATE_LEN_LO:
      frame_buf[5] = byte;
      payload_len = (uint16_t)byte;
      state = UART_STATE_LEN_HI;
      break;

    case UART_STATE_LEN_HI:
      frame_buf[6] = byte;
      payload_len |= ((uint16_t)byte << 8);

      if (payload_len == IMAGE_BUFFER_SIZE && payload_len <= IMAGE_BUFFER_SIZE)
      {
        payload_idx = 0U;
        state = UART_STATE_PAYLOAD;
      }
      else
      {
        /* Invalid length, resync */
        payload_len = 0U;
        state = UART_STATE_SYNC0;
      }
      break;

    case UART_STATE_PAYLOAD:
      if (payload_idx < IMAGE_BUFFER_SIZE)
      {
        image_buffer[payload_idx++] = byte;
      }

      if (payload_idx >= payload_len)
      {
        /* Full payload received, classify */
        ei_classify_callback(image_buffer, payload_len);
        state = UART_STATE_SYNC0;
        payload_len = 0U;
        payload_idx = 0U;
      }
      break;

    default:
      state = UART_STATE_SYNC0;
      payload_len = 0U;
      payload_idx = 0U;
      break;
    }
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

  /* Start continuous stream reception (ReceiveToIdle IT + RX FIFO) */
  UART_StartStreamReception();

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
