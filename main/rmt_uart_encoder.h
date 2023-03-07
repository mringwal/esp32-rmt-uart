/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of ir uart encoder configuration
 */
typedef struct {
    uint32_t resolution; /*!< Encoder resolution, in Hz */
    uint32_t baudrate;
    uint8_t  level_idle;

} rmt_uart_encoder_config_t;

/**
 * @brief Create RMT encoder for encoding UART data into RMT symbols
 *
 * @param[in] config Encoder configuration
 * @param[out] ret_encoder Returned encoder handle
 * @return
 *      - ESP_ERR_INVALID_ARG for any invalid arguments
 *      - ESP_ERR_NO_MEM out of memory when creating ir uart encoder
 *      - ESP_OK if creating encoder successfully
 */
esp_err_t rmt_new_uart_encoder(const rmt_uart_encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);

#ifdef __cplusplus
}
#endif
