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

 #ifndef _EI_CLASSIFIER_INFERENCING_ENGINE_ATON_H

#if (EI_CLASSIFIER_INFERENCING_ENGINE == EI_CLASSIFIER_ATON)

/* Include ----------------------------------------------------------------- */
#include "edge-impulse-sdk/tensorflow/lite/kernels/custom/tree_ensemble_classifier.h"
#include "edge-impulse-sdk/classifier/ei_model_types.h"
#include "edge-impulse-sdk/classifier/ei_run_dsp.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

#include "ll_aton_runtime.h"
#include "ll_aton_NN_interface.h"
#include "app_config.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Private variables ------------------------------------------------------- */
static uint8_t *nn_in;
static uint8_t *nn_out;

static const LL_Buffer_InfoTypeDef *nn_in_info;
static const LL_Buffer_InfoTypeDef *nn_out_info;

LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(Default);


EI_IMPULSE_ERROR run_nn_inference_image_quantized(
    const ei_impulse_t *impulse,
    signal_t *signal,
    uint32_t learn_block_index,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    ei_learning_block_config_tflite_graph_t *block_config = (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_aton_graph_t *graph_config = (ei_config_aton_graph_t*)block_config->graph_config;

    // this needs to be changed for multi-model, multi-impulse
    static bool first_run = true;

    uint64_t ctx_start_us = ei_read_timer_us();

    #if DATA_OUT_FORMAT_FLOAT32
    static float32_t *nn_out;
    #else
    static uint8_t *nn_out;
    #endif
    static uint32_t nn_out_len;

    if(first_run == true) {

        nn_in_info = LL_ATON_Input_Buffers_Info_Default();
        nn_out_info = LL_ATON_Output_Buffers_Info_Default();

        nn_in = (uint8_t *) LL_Buffer_addr_start(&nn_in_info[0]);
        uint32_t nn_in_len = LL_Buffer_len(&nn_in_info[0]);

        #if DATA_OUT_FORMAT_FLOAT32
        nn_out = (float32_t *) nn_out_info[0].addr_base.p;
        #else
        nn_out = (uint8_t *) LL_Buffer_addr_start(&nn_out_info[0]);
        #endif
        nn_out_len = LL_Buffer_len(&nn_out_info[0]);

        first_run = false;
    }

    signal->get_data(0, impulse->nn_input_frame_size, (float*) nn_in);
    #ifdef USE_DCACHE
    SCB_CleanInvalidateDCache_by_Addr(nn_in, impulse->nn_input_frame_size);
    #endif

    LL_ATON_RT_Main(&NN_Instance_Default);

    /* Discard all nn_out regions to avoid Dcache evictions during nn inference */
    #ifdef USE_DCACHE
    int i = 0;
    while (nn_out_info[i].name != NULL) {
            SCB_InvalidateDCache_by_Addr((float32_t *) LL_Buffer_addr_start(&nn_out_info[i]), LL_Buffer_len(&nn_out_info[i]));
            i++;
    }
    #endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    size_t output_size = nn_out_len;

    result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, output_size);
    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    switch (graph_config->quant_type) {
        case kTfLiteFloat32: {
            result->_raw_outputs[learn_block_index].matrix = new matrix_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix->buffer, (float *)nn_out, output_size * sizeof(float));
            break;
        }
        case kTfLiteInt8: {
            result->_raw_outputs[learn_block_index].matrix_i8 = new matrix_i8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer, (int8_t *)nn_out, output_size * sizeof(int8_t));
            break;
        }
        case kTfLiteUInt8: {
            result->_raw_outputs[learn_block_index].matrix_u8 = new matrix_u8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_u8->buffer, (uint8_t *)nn_out, output_size * sizeof(uint8_t));
            break;
        }
        default: {
            ei_printf("ERR: Cannot handle output type (%d)\n", graph_config->quant_type);
            return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
        }
    }

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    return EI_IMPULSE_OK;
}


/**
 * @brief      Do neural network inferencing over the processed feature matrix
 *
 * @param      fmatrix  Processed matrix
 * @param      result   Output classifier results
 * @param[in]  debug    Debug output enable
 *
 * @return     The ei impulse error.
 */
EI_IMPULSE_ERROR run_nn_inference(
    const ei_impulse_t *impulse,
    ei_feature_t *fmatrix,
    uint32_t learn_block_index,
    uint32_t* input_block_ids,
    uint32_t input_block_ids_size,
    ei_impulse_result_t *result,
    void *config_ptr,
    bool debug = false)
{
    (void)debug;

    if (!impulse || !fmatrix || !result || !config_ptr) {
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    ei_learning_block_config_tflite_graph_t *block_config =
        (ei_learning_block_config_tflite_graph_t*)config_ptr;
    ei_config_aton_graph_t *graph_config =
        (ei_config_aton_graph_t*)block_config->graph_config;

    if (input_block_ids_size == 0) {
        ei_printf("ERR: ATON: input_block_ids_size == 0\n");
        return EI_IMPULSE_INFERENCE_ERROR;
    }

    /* For now we only support a single DSP input block per learning block.
     * This matches the current autoencoder impulse (one RAW block feeding one NN block).
     */
    uint32_t first_block_id = input_block_ids[0];

    /* Find the index of the DSP block that produces this block_id,
     * so we can pick the right feature matrix from fmatrix[].
     */
    size_t dsp_index = impulse->dsp_blocks_size;
    for (size_t i = 0; i < impulse->dsp_blocks_size; i++) {
        if (impulse->dsp_blocks[i].blockId == first_block_id) {
            dsp_index = i;
            break;
        }
    }

    if (dsp_index >= impulse->dsp_blocks_size) {
        ei_printf("ERR: ATON: could not find DSP block for id %lu\n",
                  (unsigned long)first_block_id);
        return EI_IMPULSE_DSP_ERROR;
    }

    if (fmatrix[dsp_index].matrix == nullptr ||
        fmatrix[dsp_index].matrix->buffer == nullptr) {
        ei_printf("ERR: ATON: feature matrix for DSP index %lu is NULL\n",
                  (unsigned long)dsp_index);
        return EI_IMPULSE_DSP_ERROR;
    }

    ei::matrix_t *input_matrix = fmatrix[dsp_index].matrix;
    size_t input_len = impulse->nn_input_frame_size;
    if (input_matrix->rows * input_matrix->cols < input_len) {
        ei_printf("ERR: ATON: feature matrix too small (%lu < %lu)\n",
                  (unsigned long)(input_matrix->rows * input_matrix->cols),
                  (unsigned long)input_len);
        return EI_IMPULSE_DSP_ERROR;
    }

    /* Lazy one-time init of NPU input/output buffer descriptors */
    static bool first_run = true;
    static uint32_t nn_in_len = 0;
    static uint32_t nn_out_len = 0;

    if (first_run) {
        nn_in_info = LL_ATON_Input_Buffers_Info_Default();
        nn_out_info = LL_ATON_Output_Buffers_Info_Default();

        if (nn_in_info == NULL || nn_out_info == NULL ||
            nn_in_info[0].name == NULL || nn_out_info[0].name == NULL) {
            ei_printf("ERR: ATON: invalid buffer info\n");
            return EI_IMPULSE_INFERENCE_ERROR;
        }

        nn_in = (uint8_t*)LL_Buffer_addr_start(&nn_in_info[0]);
        nn_out = (uint8_t*)LL_Buffer_addr_start(&nn_out_info[0]);

        nn_in_len = LL_Buffer_len(&nn_in_info[0]);
        nn_out_len = LL_Buffer_len(&nn_out_info[0]);

        if (nn_in_len < input_len) {
            ei_printf("ERR: ATON: NPU input buffer too small (%lu < %lu)\n",
                      (unsigned long)nn_in_len,
                      (unsigned long)input_len);
            return EI_IMPULSE_INFERENCE_ERROR;
        }

        first_run = false;
    }

    const LL_Buffer_InfoTypeDef *in_buf = &nn_in_info[0];

    /* Quantize float features -> NPU input buffer when needed */
    if (in_buf->type == DataType_FLOAT) {
        /* Native float input: just copy */
        float *nn_in_f32 = (float*)nn_in;
        for (size_t i = 0; i < input_len; i++) {
            nn_in_f32[i] = input_matrix->buffer[i];
        }
    }
    else {
        /* Assume symmetric quantized input (INT8/UINT8) with scale/offset.
         * Convert: q = round(x / scale + offset), clamped to valid range.
         */
        float scale = 1.0f;
        int offset = 0;
        if (in_buf->scale != NULL) {
            scale = in_buf->scale[0];
        }
        if (in_buf->offset != NULL) {
            offset = in_buf->offset[0];
        }

        int bits = (int)LL_Buffer_bits(in_buf);
        if (bits <= 0 || bits > 16) {
            ei_printf("ERR: ATON: unsupported input bit width %d\n", bits);
            return EI_IMPULSE_INFERENCE_ERROR;
        }

        int is_unsigned = in_buf->Qunsigned != 0;
        int32_t q_min, q_max;
        if (is_unsigned) {
            q_min = 0;
            q_max = (1 << bits) - 1;
        }
        else {
            q_min = -(1 << (bits - 1));
            q_max =  (1 << (bits - 1)) - 1;
        }

        if (scale == 0.0f) {
            ei_printf("ERR: ATON: input scale is zero\n");
            return EI_IMPULSE_INFERENCE_ERROR;
        }

        for (size_t i = 0; i < input_len; i++) {
            float x = input_matrix->buffer[i];
            float q_f = (x / scale) + (float)offset;
            int32_t q = (int32_t)roundf(q_f);

            if (q < q_min) q = q_min;
            if (q > q_max) q = q_max;

            nn_in[i] = (uint8_t)q;
        }
    }

#ifdef USE_DCACHE
    SCB_CleanInvalidateDCache_by_Addr(nn_in, nn_in_len);
#endif

    uint64_t ctx_start_us = ei_read_timer_us();

    LL_ATON_RT_Main(&NN_Instance_Default);

#ifdef USE_DCACHE
    /* Discard all nn_out regions to avoid Dcache evictions during nn inference */
    int i = 0;
    while (nn_out_info[i].name != NULL) {
        SCB_InvalidateDCache_by_Addr(
            (float32_t *)LL_Buffer_addr_start(&nn_out_info[i]),
            LL_Buffer_len(&nn_out_info[i]));
        i++;
    }
#endif

    result->timing.classification_us = ei_read_timer_us() - ctx_start_us;

    size_t output_size = nn_out_len;

    /* Allocate and fill raw output tensor in the format expected by
     * the Edge Impulse post-processing pipeline.
     */
    switch (graph_config->quant_type) {
        case EI_CLASSIFIER_DATATYPE_FLOAT32: {
            result->_raw_outputs[learn_block_index].matrix =
                new matrix_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix->buffer,
                   (float *)nn_out, output_size * sizeof(float));
            break;
        }
        case EI_CLASSIFIER_DATATYPE_INT8: {
            result->_raw_outputs[learn_block_index].matrix_i8 =
                new matrix_i8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_i8->buffer,
                   (int8_t *)nn_out, output_size * sizeof(int8_t));
            break;
        }
        case EI_CLASSIFIER_DATATYPE_UINT8: {
            result->_raw_outputs[learn_block_index].matrix_u8 =
                new matrix_u8_t(1, output_size);
            memcpy(result->_raw_outputs[learn_block_index].matrix_u8->buffer,
                   (uint8_t *)nn_out, output_size * sizeof(uint8_t));
            break;
        }
        default: {
            ei_printf("ERR: ATON: unsupported output quant type %d\n",
                      (int)graph_config->quant_type);
            return EI_IMPULSE_OUTPUT_TENSOR_WAS_NULL;
        }
    }

    result->_raw_outputs[learn_block_index].blockId = block_config->block_id;

    return EI_IMPULSE_OK;
}

#ifdef __cplusplus
}
#endif

#endif // EI_CLASSIFIER_INFERENCING_ENGINE
#endif // _EI_CLASSIFIER_INFERENCING_ENGINE_ATON_H
