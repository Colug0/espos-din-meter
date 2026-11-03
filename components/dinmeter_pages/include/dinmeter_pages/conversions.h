/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Unit conversions for number values: shown = raw * factor + offset. Signal K
 * sends SI units (m/s, K, rad, Pa, m3, ratios); the owner picks what the
 * display shows. This table is the only list: validation, the runtime and the
 * web UI (GET /api/v1/pages/conversions) all read it.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;    /* stored in the page document */
    const char *label; /* shown in the web UI */
    double factor;
    double offset;
    /* ASCII only: the built-in Montserrat subset has no degree sign. */
    const char *unit;
} dinmeter_conversion_t;

/* NULL for "none", an empty id, or an unknown id. */
const dinmeter_conversion_t *dinmeter_conversion_find(const char *id);

/* "none" or a known id. */
bool dinmeter_conversion_valid(const char *id);

/* The table, without "none". */
const dinmeter_conversion_t *dinmeter_conversions(int *count);

#ifdef __cplusplus
}
#endif
