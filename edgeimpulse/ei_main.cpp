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

extern UART_HandleTypeDef hlpuart1;
static int8_t received_image_buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// Gets data for the classifier from the received image buffer
static int received_feature_get_data(size_t offset, size_t length, float *out_ptr)
{
  if (offset + length > EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
  {
    return EIDSP_OUT_OF_BOUNDS;
  }

  int8_t *out_ptr_i8 = reinterpret_cast<int8_t *>(out_ptr);
  memcpy(out_ptr_i8, received_image_buffer + offset, length * sizeof(int8_t));
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

extern "C" void ei_classify_callback(uint8_t *image_data, uint32_t size)
{
  if (image_data == nullptr || size != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
  {
    ei_printf("ERROR: Invalid image size (expected %d, got %lu)\n",
              EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, size);
    const char *errorMsg = "Classification failed\r\n";
    printf("%s", errorMsg);
    return;
  }

  memcpy(received_image_buffer, image_data, size);

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
  signal.get_data = &received_feature_get_data;

  ei_impulse_result_t result = {nullptr};

  ei_printf("Running classifier...\n");
  uint32_t start_cycles = DWT->CYCCNT;
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
  uint32_t end_cycles = DWT->CYCCNT;
  uint32_t cycles = end_cycles - start_cycles;
  float latency_ms = (float)cycles / (SystemCoreClock / 1000.0);

  if (res != 0)
  {
    ei_printf("ERROR: Classifier failed with code %d\n", res);
    const char *errorMsg = "Classification failed\r\n";
    printf("%s", errorMsg);
    return;
  }

  int predicted_digit = 0;
  for (size_t ix = 1; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    if (result.classification[ix].value > result.classification[predicted_digit].value)
      predicted_digit = ix;

  ei_printf("Prediction: Digit %d (confidence: %.3f)\n", predicted_digit, result.classification[predicted_digit].value);

  ei_printf("All probabilities:\n");
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++)
    ei_printf("  %d: %.3f\n", ix, result.classification[ix].value);

  char response[64];
  sprintf(response, "Latency: %.3f ms\r\nPredicted: %d\r\n", latency_ms, predicted_digit);
  printf("%s", response);
}
