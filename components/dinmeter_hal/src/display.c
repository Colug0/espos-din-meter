/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "dinmeter_hal/display.h"

#include "dinmeter_hal/board.h"
#include "driver/gpio.h"
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

static const char *TAG = "din_display";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;

esp_err_t dinmeter_display_init(void)
{
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << DINMETER_PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl_cfg), TAG, "backlight gpio");
    gpio_set_level(DINMETER_PIN_LCD_BL, 0);

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

void dinmeter_display_backlight_on(void)
{
    gpio_set_level(DINMETER_PIN_LCD_BL, 1);
}

esp_lcd_panel_io_handle_t dinmeter_display_io(void)
{
    return s_io;
}

esp_lcd_panel_handle_t dinmeter_display_panel(void)
{
    return s_panel;
}
