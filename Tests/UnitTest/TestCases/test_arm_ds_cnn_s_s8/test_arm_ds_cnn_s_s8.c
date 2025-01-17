/*
 * SPDX-FileCopyrightText: Copyright 2022 Arm Limited and/or its affiliates <open-source-office@arm.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arm_nnfunctions.h"
#include "unity.h"

#include "../TestData/ds_cnn_s/test_data.h"
#include "../Utils/validate.h"

#define MAX_DIM_SIZE_BYTE_0 (CONV_0_OUTPUT_W * CONV_0_OUTPUT_H * CONV_0_OUT_CH)
#define MAX_DIM_SIZE_BYTE_1 (DW_CONV_1_OUTPUT_H * DW_CONV_1_OUTPUT_W * DW_CONV_1_OUT_CH)

#define MAX_SIZE_BYTES (MAX_DIM_SIZE_BYTE_0 > MAX_DIM_SIZE_BYTE_1 ? MAX_DIM_SIZE_BYTE_0 : MAX_DIM_SIZE_BYTE_1)

// Word aligned start addresses to prevent unalinged access.
#define MAX_NUM_WORDS_IN_OUT ((MAX_DIM_SIZE_BYTE_0 + MAX_DIM_SIZE_BYTE_1 + 3) / 4)
#define IN_OUT_BUFER_0_BYTE_OFFSET (0)
#define IN_OUT_BUFER_1_BYTE_OFFSET (MAX_SIZE_BYTES + MAX_SIZE_BYTES % 4)

static int32_t in_out_buf_main[MAX_NUM_WORDS_IN_OUT];

/* Get size of additional buffers required by library/framework */
int ds_cnn_s_s8_get_buffer_size(void)
{
    /* Custom function based on knowledge that only select layers of DS_CNN_S network require additional buffers. */
    int max_buffer = 0;
    cmsis_nn_conv_params conv_params;
    cmsis_nn_dims input_dims;
    cmsis_nn_dims filter_dims;
    cmsis_nn_dims output_dims;

    // Layer 0 - Conv
    conv_params.padding.h = CONV_0_PAD_H;
    conv_params.padding.w = CONV_0_PAD_W;
    conv_params.stride.h = CONV_0_STRIDE_H;
    conv_params.stride.w = CONV_0_STRIDE_W;
    conv_params.dilation.h = CONV_0_DILATION_H;
    conv_params.dilation.w = CONV_0_DILATION_W;

    input_dims.n = CONV_0_INPUT_BATCHES;
    input_dims.h = CONV_0_INPUT_H;
    input_dims.w = CONV_0_INPUT_W;
    input_dims.c = CONV_0_IN_CH;

    filter_dims.h = CONV_0_FILTER_H;
    filter_dims.w = CONV_0_FILTER_W;

    output_dims.n = input_dims.n;
    output_dims.h = CONV_0_OUTPUT_H;
    output_dims.w = CONV_0_OUTPUT_W;
    output_dims.c = CONV_0_OUT_CH;

    int32_t size = arm_convolve_wrapper_s8_get_buffer_size(&conv_params, &input_dims, &filter_dims, &output_dims);

    max_buffer = size > max_buffer ? size : max_buffer;

    // Layer 0 - DW Conv
    cmsis_nn_dw_conv_params dw_conv_params;
    dw_conv_params.activation.min = DW_CONV_1_OUT_ACTIVATION_MIN;
    dw_conv_params.activation.max = DW_CONV_1_OUT_ACTIVATION_MAX;
    dw_conv_params.ch_mult = 1;
    dw_conv_params.dilation.h = DW_CONV_1_DILATION_H;
    dw_conv_params.dilation.w = DW_CONV_1_DILATION_W;
    dw_conv_params.input_offset = DW_CONV_1_INPUT_OFFSET;
    dw_conv_params.output_offset = DW_CONV_1_OUTPUT_OFFSET;
    dw_conv_params.padding.h = DW_CONV_1_PAD_H;
    dw_conv_params.padding.w = DW_CONV_1_PAD_W;
    dw_conv_params.stride.h = DW_CONV_1_STRIDE_H;
    dw_conv_params.stride.w = DW_CONV_1_STRIDE_W;

    filter_dims.h = DW_CONV_1_FILTER_H;
    filter_dims.w = DW_CONV_1_FILTER_W;

    input_dims.n = 1;
    input_dims.h = DW_CONV_1_INPUT_H;
    input_dims.w = DW_CONV_1_INPUT_W;
    input_dims.c = DW_CONV_1_OUT_CH;

    output_dims.h = DW_CONV_1_OUTPUT_H;
    output_dims.w = DW_CONV_1_OUTPUT_W;
    output_dims.c = DW_CONV_1_OUT_CH;

    size = arm_depthwise_conv_wrapper_s8_get_buffer_size(&dw_conv_params, &input_dims, &filter_dims, &output_dims);

    max_buffer = size > max_buffer ? size : max_buffer;

    return max_buffer;
}

void ds_cnn_s_s8_inference(void)
{
    /* Test for a complete int8 DS_CNN_S keyword spotting network from https://github.com/ARM-software/ML-zoo &
     * Tag: 22.02 */
    cmsis_nn_context ctx;
    const arm_cmsis_nn_status expected = ARM_CMSIS_NN_SUCCESS;

    ctx.size = ds_cnn_s_s8_get_buffer_size();
    ctx.buf = malloc(ctx.size);

    int8_t *in_out_buf_0 = (int8_t *)&in_out_buf_main[IN_OUT_BUFER_0_BYTE_OFFSET >> 2];
    int8_t *in_out_buf_1 = (int8_t *)&in_out_buf_main[IN_OUT_BUFER_1_BYTE_OFFSET >> 2];

    // Layer 0 - Implicit reshape
    // 1x490 is interpreted as 49x10

    // Layer 1 - Conv
    cmsis_nn_conv_params conv_params;
    cmsis_nn_per_channel_quant_params quant_params;
    cmsis_nn_dims in_out_dim_0;
    cmsis_nn_dims conv_filter_dims;
    cmsis_nn_dims dw_conv_filter_dims;
    cmsis_nn_dims in_out_dim_1;
    cmsis_nn_dims bias_dims;

    conv_params.padding.h = CONV_0_PAD_H;
    conv_params.padding.w = CONV_0_PAD_W;
    conv_params.stride.h = CONV_0_STRIDE_H;
    conv_params.stride.w = CONV_0_STRIDE_W;
    conv_params.dilation.h = CONV_0_DILATION_H;
    conv_params.dilation.w = CONV_0_DILATION_W;
    conv_params.input_offset = CONV_0_INPUT_OFFSET;
    conv_params.output_offset = CONV_0_OUTPUT_OFFSET;
    // Not repeated subsequently as it is the same for all in this specific case.
    conv_params.activation.min = -128;
    conv_params.activation.max = 127;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_1_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_1_conv2d_output_shift;

    in_out_dim_0.n = CONV_0_INPUT_BATCHES;
    in_out_dim_0.h = CONV_0_INPUT_H;
    in_out_dim_0.w = CONV_0_INPUT_W;
    in_out_dim_0.c = CONV_0_IN_CH;

    conv_filter_dims.h = CONV_0_FILTER_H;
    conv_filter_dims.w = CONV_0_FILTER_W;

    in_out_dim_1.n = in_out_dim_0.n;
    in_out_dim_1.h = CONV_0_OUTPUT_H;
    in_out_dim_1.w = CONV_0_OUTPUT_W;
    in_out_dim_1.c = CONV_0_OUT_CH;
    bias_dims.c = CONV_0_OUT_CH;

    arm_cmsis_nn_status status = arm_convolve_wrapper_s8(&ctx,
                                                         &conv_params,
                                                         &quant_params,
                                                         &in_out_dim_0,
                                                         ds_cnn_s_input,
                                                         &conv_filter_dims,
                                                         ds_cnn_s_layer_1_conv2d_weights,
                                                         &bias_dims,
                                                         ds_cnn_s_layer_1_conv2d_bias,
                                                         &in_out_dim_1,
                                                         in_out_buf_0);

    /***************************** Depthwise Separable Block 1 *************** */
    // Layer 1 - DW Conv
    // Common params for DW conv in subsequent layers
    cmsis_nn_dw_conv_params dw_conv_params;
    dw_conv_params.activation.min = DW_CONV_1_OUT_ACTIVATION_MIN;
    dw_conv_params.activation.max = DW_CONV_1_OUT_ACTIVATION_MAX;
    dw_conv_params.ch_mult = 1;
    dw_conv_params.dilation.h = DW_CONV_1_DILATION_H;
    dw_conv_params.dilation.w = DW_CONV_1_DILATION_W;
    dw_conv_params.padding.h = DW_CONV_1_PAD_H;
    dw_conv_params.padding.w = DW_CONV_1_PAD_W;
    dw_conv_params.stride.h = DW_CONV_1_STRIDE_H;
    dw_conv_params.stride.w = DW_CONV_1_STRIDE_W;

    // Layer specific params
    dw_conv_params.input_offset = DW_CONV_1_INPUT_OFFSET;
    dw_conv_params.output_offset = DW_CONV_1_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_2_dw_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_2_dw_conv2d_output_shift;

    dw_conv_filter_dims.h = DW_CONV_1_FILTER_H;
    dw_conv_filter_dims.w = DW_CONV_1_FILTER_W;

    in_out_dim_0.h = DW_CONV_1_OUTPUT_H;
    in_out_dim_0.w = DW_CONV_1_OUTPUT_W;
    in_out_dim_0.c = DW_CONV_1_OUT_CH;

    // Same for all layers in DS block
    bias_dims.c = in_out_dim_0.c;

    status |= arm_depthwise_conv_wrapper_s8(&ctx,
                                            &dw_conv_params,
                                            &quant_params,
                                            &in_out_dim_1,
                                            in_out_buf_0,
                                            &dw_conv_filter_dims,
                                            ds_cnn_s_layer_2_dw_conv2d_weights,
                                            &bias_dims,
                                            ds_cnn_s_layer_2_dw_conv2d_bias,
                                            &in_out_dim_0,
                                            in_out_buf_1);

    // Layer 2 - Conv

    // Common params for Conv in rest of DS blocks
    in_out_dim_1.h = in_out_dim_0.h;
    in_out_dim_1.w = in_out_dim_0.w;
    in_out_dim_1.c = in_out_dim_0.c;
    conv_filter_dims.h = CONV_2_FILTER_H;
    conv_filter_dims.w = CONV_2_FILTER_W;

    conv_params.padding.h = CONV_2_PAD_H;
    conv_params.padding.w = CONV_2_PAD_W;
    conv_params.stride.h = CONV_2_STRIDE_H;
    conv_params.stride.w = CONV_2_STRIDE_W;

    // Layer specific params
    conv_params.input_offset = CONV_2_INPUT_OFFSET;
    conv_params.output_offset = CONV_2_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_3_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_3_conv2d_output_shift;

    status |= arm_convolve_wrapper_s8(&ctx,
                                      &conv_params,
                                      &quant_params,
                                      &in_out_dim_0,
                                      in_out_buf_1,
                                      &conv_filter_dims,
                                      ds_cnn_s_layer_3_conv2d_weights,
                                      &bias_dims,
                                      ds_cnn_s_layer_3_conv2d_bias,
                                      &in_out_dim_1,
                                      in_out_buf_0);

    /***************************** Depthwise Separable Block 2 *************** */
    // Layer specific
    dw_conv_params.input_offset = DW_CONV_3_INPUT_OFFSET;
    dw_conv_params.output_offset = DW_CONV_3_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_4_dw_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_4_dw_conv2d_output_shift;

    status |= arm_depthwise_conv_wrapper_s8(&ctx,
                                            &dw_conv_params,
                                            &quant_params,
                                            &in_out_dim_1,
                                            in_out_buf_0,
                                            &dw_conv_filter_dims,
                                            ds_cnn_s_layer_4_dw_conv2d_weights,
                                            &bias_dims,
                                            ds_cnn_s_layer_4_dw_conv2d_bias,
                                            &in_out_dim_0,
                                            in_out_buf_1);

    // Layer specific params
    conv_params.input_offset = CONV_4_INPUT_OFFSET;
    conv_params.output_offset = CONV_4_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_5_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_5_conv2d_output_shift;

    status |= arm_convolve_wrapper_s8(&ctx,
                                      &conv_params,
                                      &quant_params,
                                      &in_out_dim_0,
                                      in_out_buf_1,
                                      &conv_filter_dims,
                                      ds_cnn_s_layer_5_conv2d_weights,
                                      &bias_dims,
                                      ds_cnn_s_layer_5_conv2d_bias,
                                      &in_out_dim_1,
                                      in_out_buf_0);

    /***************************** Depthwise Separable Block 3 *************** */
    // Layer specific
    dw_conv_params.input_offset = DW_CONV_5_INPUT_OFFSET;
    dw_conv_params.output_offset = DW_CONV_5_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_6_dw_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_6_dw_conv2d_output_shift;

    status |= arm_depthwise_conv_wrapper_s8(&ctx,
                                            &dw_conv_params,
                                            &quant_params,
                                            &in_out_dim_1,
                                            in_out_buf_0,
                                            &dw_conv_filter_dims,
                                            ds_cnn_s_layer_6_dw_conv2d_weights,
                                            &bias_dims,
                                            ds_cnn_s_layer_6_dw_conv2d_bias,
                                            &in_out_dim_0,
                                            in_out_buf_1);

    // Layer specific params
    conv_params.input_offset = CONV_6_INPUT_OFFSET;
    conv_params.output_offset = CONV_6_OUTPUT_OFFSET;
    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_7_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_7_conv2d_output_shift;

    status |= arm_convolve_wrapper_s8(&ctx,
                                      &conv_params,
                                      &quant_params,
                                      &in_out_dim_0,
                                      in_out_buf_1,
                                      &conv_filter_dims,
                                      ds_cnn_s_layer_7_conv2d_weights,
                                      &bias_dims,
                                      ds_cnn_s_layer_7_conv2d_bias,
                                      &in_out_dim_1,
                                      in_out_buf_0);

    /***************************** Depthwise Separable Block 4 *************** */
    // Layer specific
    dw_conv_params.input_offset = DW_CONV_7_INPUT_OFFSET;
    dw_conv_params.output_offset = DW_CONV_7_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_8_dw_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_8_dw_conv2d_output_shift;

    status |= arm_depthwise_conv_wrapper_s8(&ctx,
                                            &dw_conv_params,
                                            &quant_params,
                                            &in_out_dim_1,
                                            in_out_buf_0,
                                            &dw_conv_filter_dims,
                                            ds_cnn_s_layer_8_dw_conv2d_weights,
                                            &bias_dims,
                                            ds_cnn_s_layer_8_dw_conv2d_bias,
                                            &in_out_dim_0,
                                            in_out_buf_1);

    conv_params.input_offset = CONV_8_INPUT_OFFSET;
    conv_params.output_offset = CONV_8_OUTPUT_OFFSET;

    quant_params.multiplier = (int32_t *)ds_cnn_s_layer_9_conv2d_output_mult;
    quant_params.shift = (int32_t *)ds_cnn_s_layer_9_conv2d_output_shift;

    status |= arm_convolve_wrapper_s8(&ctx,
                                      &conv_params,
                                      &quant_params,
                                      &in_out_dim_0,
                                      in_out_buf_1,
                                      &conv_filter_dims,
                                      ds_cnn_s_layer_9_conv2d_weights,
                                      &bias_dims,
                                      ds_cnn_s_layer_9_conv2d_bias,
                                      &in_out_dim_1,
                                      in_out_buf_0);

    /***************************** Average Pool *************** */

    cmsis_nn_pool_params pool_params;
    pool_params.activation.max = AVERAGE_POOL_9_OUT_ACTIVATION_MAX;
    pool_params.activation.min = AVERAGE_POOL_9_OUT_ACTIVATION_MIN;
    pool_params.padding.h = AVERAGE_POOL_9_PAD_H;
    pool_params.padding.w = AVERAGE_POOL_9_PAD_W;
    pool_params.stride.h = AVERAGE_POOL_9_STRIDE_H;
    pool_params.stride.w = AVERAGE_POOL_9_STRIDE_W;

    conv_filter_dims.h = AVERAGE_POOL_9_FILTER_H;
    conv_filter_dims.w = AVERAGE_POOL_9_FILTER_W;

    in_out_dim_0.n = 1;
    in_out_dim_0.h = AVERAGE_POOL_9_OUTPUT_H;
    in_out_dim_0.w = AVERAGE_POOL_9_OUTPUT_W;
    in_out_dim_0.c = in_out_dim_1.c;

    status |=
        arm_avgpool_s8(&ctx, &pool_params, &in_out_dim_1, in_out_buf_0, &conv_filter_dims, &in_out_dim_0, in_out_buf_1);

    /***************************** Fully Connected ****************/
    cmsis_nn_fc_params fc_params;
    fc_params.activation.max = FULLY_CONNECTED_11_OUT_ACTIVATION_MAX;
    fc_params.activation.min = FULLY_CONNECTED_11_OUT_ACTIVATION_MIN;
    fc_params.filter_offset = 0;
    fc_params.input_offset = FULLY_CONNECTED_11_INPUT_OFFSET;
    fc_params.output_offset = FULLY_CONNECTED_11_OUTPUT_OFFSET;

    cmsis_nn_per_tensor_quant_params per_tensor_quant_params;
    per_tensor_quant_params.multiplier = FULLY_CONNECTED_11_OUTPUT_MULTIPLIER;
    per_tensor_quant_params.shift = FULLY_CONNECTED_11_OUTPUT_SHIFT;

    in_out_dim_0.c = in_out_dim_0.c * in_out_dim_0.h * in_out_dim_0.w;
    in_out_dim_0.h = 1;
    in_out_dim_0.w = 1;

    conv_filter_dims.n = in_out_dim_0.c;
    conv_filter_dims.h = 1;
    conv_filter_dims.w = 1;
    conv_filter_dims.c = FULLY_CONNECTED_11_OUTPUT_W;

    in_out_dim_1.n = 1;
    in_out_dim_1.h = 1;
    in_out_dim_1.w = 1;
    in_out_dim_1.c = FULLY_CONNECTED_11_OUTPUT_W;

    bias_dims.c = in_out_dim_1.c;

    status |= arm_fully_connected_s8(&ctx,
                                     &fc_params,
                                     &per_tensor_quant_params,
                                     &in_out_dim_0,
                                     in_out_buf_1,
                                     &conv_filter_dims,
                                     ds_cnn_s_layer_12_fc_weights,
                                     &bias_dims,
                                     ds_cnn_s_layer_12_fc_bias,
                                     &in_out_dim_1,
                                     in_out_buf_0);

    /***************************** Softmax *************** */

    arm_softmax_s8(in_out_buf_0,
                   SOFTMAX_12_NUM_ROWS,
                   SOFTMAX_12_ROW_SIZE,
                   SOFTMAX_12_MULT,
                   SOFTMAX_12_SHIFT,
                   SOFTMAX_12_DIFF_MIN,
                   in_out_buf_0);
    free(ctx.buf);

    TEST_ASSERT_EQUAL(expected, status);
    TEST_ASSERT_TRUE(validate(in_out_buf_0, ds_cnn_s_output_ref, sizeof(ds_cnn_s_output_ref)));
}
