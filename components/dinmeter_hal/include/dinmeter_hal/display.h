/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native panel resolution, portrait. The LVGL display swaps the axes. */
#define DINMETER_DISPLAY_NATIVE_W 135
#define DINMETER_DISPLAY_NATIVE_H 240

/* SPI bus, ST7789 panel and backlight PWM. The backlight stays off until
 * dinmeter_display_backlight_on(), so a blank panel is never lit. */
esp_err_t dinmeter_display_init(void);
void dinmeter_display_backlight_on(void);

/* Backlight brightness, 1..100 %. Takes effect at once if the backlight is
 * on, else when it is switched on. Any task. */
void dinmeter_display_set_brightness(uint8_t percent);

/* For the LVGL port. NULL before init. */
esp_lcd_panel_io_handle_t dinmeter_display_io(void);
esp_lcd_panel_handle_t dinmeter_display_panel(void);

#ifdef __cplusplus
}
#endif
