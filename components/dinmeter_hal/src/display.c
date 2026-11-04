/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "dinmeter_hal/display.h"

#include <math.h>
#include <stdatomic.h>

#include "dinmeter_hal/board.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define LCD_SPI_HOST SPI2_HOST
/* M5GFX sets this panel up as offset_x=52, offset_y=40 in native portrait.
 * esp_lcd applies the gap in draw_bitmap() without regard to swap_xy, unlike
 * LovyanGFX's setRotation(), so the landscape values are given here already
 * swapped. Wrong values show a strip of noise along two edges. */
#define LCD_GAP_X 40
#define LCD_GAP_Y 52

/* GPIO9 drives the EN pin of an AW35122 load switch (U6 in the DIN Meter
 * schematic) that powers the backlight LEDs, so brightness is PWM on that
 * switch. It turns on gently: at 5 kHz the panel stayed black; 200 Hz is what
 * M5Stack's own DinMeter demo uses (M5DinMeter-UserDemo, ft_disp_lgfx_cfg.hpp). 1 % maps to a duty of 64/1023, not near zero, so the lowest
 * setting stays readable; the percentage goes through a gamma curve because
 * the eye sees brightness logarithmically and a linear scale left the top
 * half looking the same.
 * Every LEDC timer on the S3 shares one clock source; APB with 10 bits
 * reaches both 200 Hz here and the buzzer's range. */
#define BL_LEDC_TIMER LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_7
#define BL_FREQ_HZ 200
#define BL_DUTY_MIN 64
#define BL_DUTY_MAX 1023

static const char *TAG = "din_display";
static atomic_uint s_brightness = 80;
static atomic_bool s_backlight_on;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;

esp_err_t dinmeter_display_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BL_LEDC_TIMER,
        .freq_hz = BL_FREQ_HZ,
        .clk_cfg = LEDC_USE_APB_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
    ledc_channel_config_t channel = {
        .gpio_num = DINMETER_PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_LEDC_CHANNEL,
        .timer_sel = BL_LEDC_TIMER,
        .duty = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "backlight channel");

    spi_bus_config_t buscfg = {
        .mosi_io_num = DINMETER_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = DINMETER_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DINMETER_DISPLAY_NATIVE_W * DINMETER_DISPLAY_NATIVE_H * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus");

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DINMETER_PIN_LCD_DC,
        .cs_gpio_num = DINMETER_PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &s_io), TAG,
                        "panel io");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = DINMETER_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_config, &s_panel), TAG, "panel new");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, LCD_GAP_X, LCD_GAP_Y), TAG, "gap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on");
    ESP_LOGI(TAG, "panel ready (%dx%d)", DINMETER_DISPLAY_NATIVE_W, DINMETER_DISPLAY_NATIVE_H);
    return ESP_OK;
}

static void apply_brightness(void)
{
    uint32_t duty = 0;
    if (atomic_load(&s_backlight_on)) {
        float x = (atomic_load(&s_brightness) - 1) / 99.0f;
        duty = BL_DUTY_MIN + (uint32_t)lroundf((BL_DUTY_MAX - BL_DUTY_MIN) * powf(x, 2.2f));
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

void dinmeter_display_backlight_on(void)
{
    atomic_store(&s_backlight_on, true);
    apply_brightness();
}

void dinmeter_display_backlight_off(void)
{
    atomic_store(&s_backlight_on, false);
    apply_brightness();
}

void dinmeter_display_set_brightness(uint8_t percent)
{
    if (percent < 1) {
        percent = 1;
    } else if (percent > 100) {
        percent = 100;
    }
    atomic_store(&s_brightness, percent);
    apply_brightness();
}

esp_lcd_panel_io_handle_t dinmeter_display_io(void)
{
    return s_io;
}

esp_lcd_panel_handle_t dinmeter_display_panel(void)
{
    return s_panel;
}
