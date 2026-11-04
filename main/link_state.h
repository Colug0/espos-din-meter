/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_SETUP,       /* setup portal open, no network: owner must enter WiFi */
    LINK_NO_NETWORK,  /* WiFi configured but not connected */
    LINK_APPROVE,     /* access request waiting in Signal K */
    LINK_NO_SK,       /* network up, Signal K stream not (yet) open */
    LINK_OK,
} link_state_t;

typedef struct {
    link_state_t state;
    char portal_ssid[33]; /* LINK_SETUP */
    char portal_ip[16];   /* LINK_SETUP */
    char hostname[33];
} link_info_t;

/* Subscribe to espOS's Signal K stream events. Before espos_start(), so the
 * first connect is not missed. */
esp_err_t link_state_init(void);

/* Current state. Polls espOS; cheap except in LINK_NO_SK, where the token
 * state is read at most every two seconds. Runtime task only. */
void link_state_get(link_info_t *out);

#ifdef __cplusplus
}
#endif
