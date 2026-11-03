/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "dinmeter_hal/input.h"

#include "dinmeter_hal/board.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "din_input";
static pcnt_unit_handle_t s_pcnt_unit;

esp_err_t dinmeter_input_init(void)
{
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << DINMETER_PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&btn_cfg), TAG, "button gpio");

    pcnt_unit_config_t unit_config = {
        .high_limit = 32767,
        .low_limit = -32768,
        /* Count on across the limits instead of snapping back to 0, which
         * would jump the page index. */
        .flags.accum_count = true,
    };
    ESP_RETURN_ON_ERROR(pcnt_new_unit(&unit_config, &s_pcnt_unit), TAG, "pcnt unit");

    pcnt_glitch_filter_config_t filter_config = {.max_glitch_ns = 1000};
    ESP_RETURN_ON_ERROR(pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config), TAG, "glitch filter");

    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = DINMETER_PIN_ENC_A,
        .level_gpio_num = DINMETER_PIN_ENC_B,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_a_config, &chan_a), TAG, "pcnt chan a");

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = DINMETER_PIN_ENC_B,
        .level_gpio_num = DINMETER_PIN_ENC_A,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_RETURN_ON_ERROR(pcnt_new_channel(s_pcnt_unit, &chan_b_config, &chan_b), TAG, "pcnt chan b");

    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_INCREASE),
                        TAG, "chan a edge");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
                        TAG, "chan a level");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                     PCNT_CHANNEL_EDGE_ACTION_DECREASE),
                        TAG, "chan b edge");
    ESP_RETURN_ON_ERROR(pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                      PCNT_CHANNEL_LEVEL_ACTION_INVERSE),
                        TAG, "chan b level");

    /* accum_count needs watch points at the limits to notice the overflow. */
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, 32767), TAG, "watch high");
    ESP_RETURN_ON_ERROR(pcnt_unit_add_watch_point(s_pcnt_unit, -32768), TAG, "watch low");

    ESP_RETURN_ON_ERROR(pcnt_unit_enable(s_pcnt_unit), TAG, "pcnt enable");
    ESP_RETURN_ON_ERROR(pcnt_unit_clear_count(s_pcnt_unit), TAG, "pcnt clear");
    ESP_RETURN_ON_ERROR(pcnt_unit_start(s_pcnt_unit), TAG, "pcnt start");
    ESP_LOGI(TAG, "encoder and button ready");
    return ESP_OK;
}

int dinmeter_input_get_count(void)
{
    int value = 0;
    if (s_pcnt_unit) {
        pcnt_unit_get_count(s_pcnt_unit, &value);
    }
    return value;
}

bool dinmeter_input_button_pressed(void)
{
    return gpio_get_level(DINMETER_PIN_BUTTON) == 0;
}
