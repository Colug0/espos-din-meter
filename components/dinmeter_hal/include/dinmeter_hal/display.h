/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native panel resolution, portrait. The LVGL display swaps the axes. */
#define DINMETER_DISPLAY_NATIVE_W 135
#define DINMETER_DISPLAY_NATIVE_H 240

/* SPI bus and ST7789 panel. The backlight stays off until
 * dinmeter_display_backlight_on(), so a blank panel is never lit. */
esp_err_t dinmeter_display_init(void);
void dinmeter_display_backlight_on(void);

/* For the LVGL port. NULL before init. */
esp_lcd_panel_io_handle_t dinmeter_display_io(void);
esp_lcd_panel_handle_t dinmeter_display_panel(void);

#ifdef __cplusplus
}
#endif
