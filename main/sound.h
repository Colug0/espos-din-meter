/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The sound task: plays the start-up tune, a click when the knob is
 * pressed, and keeps the tune table in step with the sound settings. After
 * espos_start() and dinmeter_input_init(). */
esp_err_t sound_start(void);

#ifdef __cplusplus
}
#endif
