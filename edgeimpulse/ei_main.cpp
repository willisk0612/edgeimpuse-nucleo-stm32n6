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

/* Include ----------------------------------------------------------------- */
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "main.h"
#include "model-parameters/model_variables.h"
#include "stm32n6xx_hal.h"
#include <cstdio>
#include <stdint.h>
#include <string.h>
#if defined(USE_NS_TIMER) && (USE_NS_TIMER == 1)
#include "timer_config.h"
#endif

#ifndef EI_INPUT_SCALE
#define EI_INPUT_SCALE 0.03975987f
#endif

#ifndef EI_INPUT_ZP
#define EI_INPUT_ZP    -4
#endif


extern UART_HandleTypeDef hlpuart1;
static uint8_t received_data_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// Gets data for the classifier from the received data buffer
static int received_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
  if (offset + length > EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
  {
    return EIDSP_OUT_OF_BOUNDS;
  }

  for (size_t i = 0; i < length; i++)
  {
    int8_t q = static_cast<int8_t>(received_data_buffer[offset + i]);
    float x_norm = (static_cast<float>(q) - static_cast<float>(EI_INPUT_ZP)) * EI_INPUT_SCALE;
    out_ptr[i] = x_norm;
  }

  return 0;
}

/**
 * Main entry point for the application
 */
extern "C" int ei_main(void)
{
  ei_printf("Edge Impulse standalone inferencing (NUCLEO-N657X0-Q)\n");

  while (1)
  {
    LED_BLUE_Blink();
    ProcessUartReception();
    ei_sleep(10);
  }
}

extern "C" void ei_classify_callback(uint8_t *data, uint32_t size)
{
  if (data == nullptr || size != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
  {
    ei_printf("ERROR: Invalid data size (expected %d, got %lu)\n",
              EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, size);
    const char *errorMsg = "Classification failed\r\n";
    printf("%s", errorMsg);
    return;
  }

  // Debug: echo first three quantized samples (signed int8) back to sender
  int8_t qx = static_cast<int8_t>(data[0]);
  int8_t qy = static_cast<int8_t>(data[1]);
  int8_t qz = static_cast<int8_t>(data[2]);
  ei_printf("Received q_x: %d, q_y: %d, q_z: %d\n", qx, qy, qz);

  memcpy(received_data_buffer, data, size);

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &received_feature_get_data;

  ei_impulse_result_t result = {nullptr};

  ei_printf("Running classifier...\n");
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  if (res != 0)
  {
    ei_printf("ERROR: Classifier failed with code %d\n", res);
    const char *errorMsg = "Classification failed\r\n";
    printf("%s", errorMsg);
    return;
  }

  const char *label = result.classification[0].label;
  if (label == nullptr)
  {
    label = "value";
  }
  float value = result.classification[0].value;

  ei_printf("Regression output [%s]: %.3f\n", label, value);

  char response[64];
  sprintf(response, "%s: %.3f\r\n", label, value);
  printf("%s", response);
}
