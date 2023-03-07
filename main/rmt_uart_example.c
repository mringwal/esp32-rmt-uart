/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "rmt_uart_encoder.h"


#define RMT_UART_RESOLUTION_HZ 1000000 // 1MHz resolution, 1 tick = 1 us
#define RMT_UART_GPIO_NUM      18

static const char *TAG = "example";

static const uint8_t test_message[] = { 1, 3, 7, 15, 31 };

// #define USE_IR_CARRIER

void app_main(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t rmt_uart_channel = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_UART_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_UART_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &rmt_uart_channel));

#ifdef USE_IR_CARRIER
    ESP_LOGI(TAG, "Setup 38 kHz IR Carrir");
    rmt_carrier_config_t tx_carrier_cfg = {
        .duty_cycle = 0.33,                 // duty cycle 33%
        .frequency_hz = 38000,              // 38KHz
        .flags.polarity_active_low = false, // carrier should modulated to high level
    };
    // modulate carrier to TX channel
    ESP_ERROR_CHECK(rmt_apply_carrier(ir_uart_channel, &tx_carrier_cfg));
    uint8_t level_idle = 0;
#else
    uint8_t level_idle = 1;
#endif

    ESP_LOGI(TAG, "Install IR UART encoder");
    rmt_encoder_handle_t rmt_uart = NULL;
    rmt_uart_encoder_config_t encoder_config = {
        .resolution = RMT_UART_RESOLUTION_HZ,
        .baudrate = 2000,
        .level_idle = level_idle,
    };
    ESP_ERROR_CHECK(rmt_new_uart_encoder(&encoder_config, &rmt_uart));


    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(rmt_uart_channel));

    ESP_LOGI(TAG, "Start Test Message");
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
        .flags.eot_level = level_idle,
    };
    while (1) {
        ESP_ERROR_CHECK(rmt_transmit(rmt_uart_channel, rmt_uart, test_message, sizeof(test_message), &tx_config));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
