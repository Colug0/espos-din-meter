/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Dark mode for night watches: a long press turns it on (if the owner
 * allowed it under display.dark_enabled), and the screen then goes dark
 * after display.dark_timeout_s without use. The runtime task does the
 * sleeping and waking; this module holds the state, which survives a
 * restart, and ends it when Signal K's environment.mode turns to day.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load the stored state. NVS and config must be up (before_network). */
esp_err_t darkmode_init(void);

bool darkmode_enabled(void);
bool darkmode_active(void);
uint32_t darkmode_timeout_ms(void);

/* Switch it on or off, store it and apply the brightness. Any task. */
void darkmode_set(bool on);

#ifdef __cplusplus
}
#endif
