/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "timer_config.h"
#include "stm32n6xx_hal_tim.h"

#define TIM_CNT_FREQ_US    (1000000U) /* Timer frequency counter : 1 MHz => 1 us */

static TIM_HandleTypeDef        TimHandle;

static uint32_t fact;
static uint32_t time_overflow_timer;
static uint64_t overflow_time;

void TIM2_IRQHandler(void);

/**
 * 
 */
HAL_StatusTypeDef timer_config_init(void)
{
    RCC_ClkInitTypeDef    clkconfig;
    uint32_t              uwTimclock;
    uint32_t              uwAPB1Prescaler;
    HAL_StatusTypeDef     Status;

    time_overflow_timer = 0;

    /* Enable TIM2 clock */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Get clock configuration */
    HAL_RCC_GetClockConfig(&clkconfig);

    RCC_PeriphCLKInitTypeDef  PeriphClkInit;
    HAL_RCCEx_GetPeriphCLKConfig(&PeriphClkInit);

    /* Get APB1 prescaler */
    uwAPB1Prescaler = clkconfig.APB1CLKDivider;

    /* Compute TIM2 clock */
    if (uwAPB1Prescaler == RCC_APB1_DIV1) {
        uwTimclock = HAL_RCC_GetSysClockFreq();
    }
    else {
        uwTimclock = 2UL * HAL_RCC_GetSysClockFreq();
    }

    if (RCC_TIMPRES_DIV2 == PeriphClkInit.TIMPresSelection) {
        uwTimclock /= 2;
    }
    else if (RCC_TIMPRES_DIV4 == PeriphClkInit.TIMPresSelection) {
        uwTimclock /= 4;
    }
    else if (RCC_TIMPRES_DIV8 == PeriphClkInit.TIMPresSelection) {
        uwTimclock /= 8;
    }

    fact = __HAL_TIM_CALC_PSC(uwTimclock, TIM_CNT_FREQ_US);
    overflow_time = ((uint64_t)1 << 32) / fact;
    
    /* Initialize TIM2 */
    TimHandle.Instance = TIM2;

    TimHandle.Init.Period = 0xFFFFFFFF; // Max 
    TimHandle.Init.Prescaler = fact;
    TimHandle.Init.ClockDivision = 0;
    TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
    Status = HAL_TIM_Base_Init(&TimHandle);
    if (Status == HAL_OK) {
        /* Start the TIM time Base generation in interrupt mode */
        Status = HAL_TIM_Base_Start_IT(&TimHandle);
        if (Status == HAL_OK) {
            /* Enable the TIM2 global Interrupt */
            HAL_NVIC_SetPriority(TIM2_IRQn, ((1UL<<__NVIC_PRIO_BITS) - 1UL), 0);
        }
    }

    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    return Status;
}

/*
 * 
*/
uint64_t timer_config_read_us(void)
{
    return (uint64_t)((time_overflow_timer * overflow_time)
        + (TIM2->CNT));
}

/**
 * 
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    time_overflow_timer++;
}

/**
  * @brief  This function handles TIM interrupt request.
  * @param  None
  * @retval None
  */
void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&TimHandle);
}
