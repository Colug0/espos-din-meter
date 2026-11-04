/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The passive buzzer on GPIO3, driven by LEDC. Silent after init. */
esp_err_t dinmeter_buzzer_init(void);

/* Sound `hz` (square wave), or silence for 0. One task only. */
void dinmeter_buzzer_tone(uint16_t hz);

#ifdef __cplusplus
}
#endif
