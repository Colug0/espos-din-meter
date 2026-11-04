/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "dinmeter_hal/buzzer.h"

#include "dinmeter_hal/board.h"
#include "driver/ledc.h"
#include "esp_check.h"

/* Its own timer: the backlight's runs at a fixed 200 Hz. The clock source
 * must match the backlight's (APB): the S3 has one for all LEDC timers. At
 * 10 bits APB reaches 76 Hz to 78 kHz. */
#define BZ_LEDC_TIMER LEDC_TIMER_2
#define BZ_LEDC_CHANNEL LEDC_CHANNEL_6
#define BZ_DUTY_ON 512 /* 50 %: the loudest square wave */

static const char *TAG = "din_buzzer";
static uint16_t s_hz;

esp_err_t dinmeter_buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BZ_LEDC_TIMER,
        .freq_hz = 2000,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "buzzer timer");
    ledc_channel_config_t channel = {
        .gpio_num = DINMETER_PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BZ_LEDC_CHANNEL,
        .timer_sel = BZ_LEDC_TIMER,
        .duty = 0,
    };
    return ledc_channel_config(&channel);
}

void dinmeter_buzzer_tone(uint16_t hz)
{
    if (hz == s_hz) {
        return;
    }
    if (hz) {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, BZ_LEDC_TIMER, hz);
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BZ_LEDC_CHANNEL, hz ? BZ_DUTY_ON : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BZ_LEDC_CHANNEL);
    s_hz = hz;
}
