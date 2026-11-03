/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Encoder (quadrature through PCNT) and its push button. */
esp_err_t dinmeter_input_init(void);

/* Free-running encoder count, two counts per detent. */
int dinmeter_input_get_count(void);

/* True while the button is held (active low, not debounced). */
bool dinmeter_input_button_pressed(void);

#ifdef __cplusplus
}
#endif
