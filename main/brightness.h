/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Backlight from display.brightness, or display.night_bright while Signal K
 * reports environment.mode "night". Applies the day value at once. Before
 * espos_sk is started is fine: the subscription waits for the stream. */
esp_err_t brightness_start(void);

/* Re-read the settings and set the backlight. Any task. */
void brightness_apply(void);

/* Once a minute: after 30 minutes without Signal K, forget the last mode
 * and go back to the day brightness. */
void brightness_tick(void);

#ifdef __cplusplus
}
#endif
