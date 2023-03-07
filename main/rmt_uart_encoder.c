/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "rmt_uart_encoder.h"

static const char *TAG = "ir_uart";

typedef enum {
    RMT_UART_ENCODER_START_BIT,
    RMT_UART_ENCODER_DATA,
    RMT_UART_ENCODER_STOP_BIT
} rmt_uart_state;

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *bytes_encoder;
    rmt_encoder_t *copy_encoder;
    rmt_uart_state state;
    rmt_symbol_word_t bit_0;
    rmt_symbol_word_t bit_1;
    uint16_t data_offset;
} rmt_rmt_ir_uart_encoder_t;

static size_t rmt_encode_uart(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_rmt_ir_uart_encoder_t *ir_uart = __containerof(encoder, rmt_rmt_ir_uart_encoder_t, base);
    rmt_encoder_handle_t bytes_encoder = ir_uart->bytes_encoder;
    rmt_encoder_handle_t copy_encoder = ir_uart->copy_encoder;
    rmt_encode_state_t session_state = 0;
    rmt_encode_state_t state = 0;
    size_t encoded_symbols = 0;
    const uint8_t * data = (const uint8_t *) primary_data;

    while (ir_uart->data_offset < data_size){

        switch (ir_uart->state) {

            case RMT_UART_ENCODER_START_BIT:
                encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ir_uart->bit_0, sizeof(ir_uart->bit_0), &session_state);
                if (session_state & RMT_ENCODING_COMPLETE) {
                    ir_uart->state = RMT_UART_ENCODER_DATA;
                }
                if (session_state & RMT_ENCODING_MEM_FULL) {
                    state |= RMT_ENCODING_MEM_FULL;
                    goto out; // yield if there's no free space for encoding artifacts
                }
                break;

            case RMT_UART_ENCODER_DATA:
                encoded_symbols += bytes_encoder->encode(bytes_encoder, channel, &data[ir_uart->data_offset], 1, &session_state);
                if (session_state & RMT_ENCODING_COMPLETE) {
                    ir_uart->state = RMT_UART_ENCODER_STOP_BIT;
                }
                if (session_state & RMT_ENCODING_MEM_FULL) {
                    state |= RMT_ENCODING_MEM_FULL;
                    goto out; // yield if there's no free space for encoding artifacts
                }
                break;

            case RMT_UART_ENCODER_STOP_BIT:
                encoded_symbols += copy_encoder->encode(copy_encoder, channel, &ir_uart->bit_1, sizeof(ir_uart->bit_1), &session_state);

                if (session_state & RMT_ENCODING_COMPLETE) {
                    ir_uart->state = RMT_UART_ENCODER_START_BIT;
                    ir_uart->data_offset++;
                }
                if (session_state & RMT_ENCODING_MEM_FULL) {
                    state |= RMT_ENCODING_MEM_FULL;
                    goto out; // yield if there's no free space for encoding artifacts
                }
                break;

            default:
                break;

        }
    }

    // done
    ir_uart->data_offset = 0;
    state |= RMT_ENCODING_COMPLETE;

out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_del_uart_encoder(rmt_encoder_t *encoder)
{
    rmt_rmt_ir_uart_encoder_t *ir_uart = __containerof(encoder, rmt_rmt_ir_uart_encoder_t, base);
    rmt_del_encoder(ir_uart->bytes_encoder);
    rmt_del_encoder(ir_uart->copy_encoder);
    free(ir_uart);
    return ESP_OK;
}

static esp_err_t rmt_uart_encoder_reset(rmt_encoder_t *encoder)
{
    rmt_rmt_ir_uart_encoder_t *ir_uart = __containerof(encoder, rmt_rmt_ir_uart_encoder_t, base);
    rmt_encoder_reset(ir_uart->bytes_encoder);
    rmt_encoder_reset(ir_uart->copy_encoder);
    ir_uart->state = RMT_UART_ENCODER_START_BIT;
    ir_uart->data_offset = 0;
    return ESP_OK;
}

esp_err_t rmt_new_uart_encoder(const rmt_uart_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_rmt_ir_uart_encoder_t *ir_uart = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ir_uart = calloc(1, sizeof(rmt_rmt_ir_uart_encoder_t));
    ESP_GOTO_ON_FALSE(ir_uart, ESP_ERR_NO_MEM, err, TAG, "no mem for ir uart encoder");
    ir_uart->base.encode = rmt_encode_uart;
    ir_uart->base.del = rmt_del_uart_encoder;
    ir_uart->base.reset = rmt_uart_encoder_reset;
    ir_uart->state = RMT_UART_ENCODER_START_BIT; 
    ir_uart->data_offset = 0;
    // half bit time
    uint32_t half_bit_time_ticks = config->resolution / (2 * config->baudrate);
    uint8_t level_active = 1 - config->level_idle;
    ir_uart->bit_0 = (rmt_symbol_word_t) {
        .level0 = level_active,
        .duration0 = half_bit_time_ticks,
        .level1 = level_active,
        .duration1 = half_bit_time_ticks,
    };
    ir_uart->bit_1 = (rmt_symbol_word_t) {
        .level0 = config->level_idle,
        .duration0 = half_bit_time_ticks,
        .level1 = config->level_idle,
        .duration1 = half_bit_time_ticks,
    };
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = ir_uart->bit_0,
        .bit1 = ir_uart->bit_1,
        .flags.msb_first = 0
    };
    ESP_GOTO_ON_ERROR(rmt_new_bytes_encoder(&bytes_encoder_config, &ir_uart->bytes_encoder), err, TAG, "create bytes encoder failed");
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &ir_uart->copy_encoder), err, TAG, "create copy encoder failed");

    *ret_encoder = &ir_uart->base;
    return ESP_OK;
err:
    if (ir_uart) {
        if (ir_uart->bytes_encoder) {
            rmt_del_encoder(ir_uart->bytes_encoder);
        }
        if (ir_uart->copy_encoder) {
            rmt_del_encoder(ir_uart->copy_encoder);
        }
        free(ir_uart);
    }
    return ret;
}
