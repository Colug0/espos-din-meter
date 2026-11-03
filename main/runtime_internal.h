/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DINPAGE_STR_MAX 48
#define DINPAGE_PATH_MAX 128
#define DINPAGE_VALUE_MAX 64
#define DINVALUE_LABEL_MAX 24
#define DINPAGE_MAX_VALUES 2

typedef enum {
    DP_UNKNOWN = 0,
    DP_NUMBER,
    DP_STRING,
    DP_BOOLEAN,
} dinpage_type_t;

typedef enum {
    SLOT_NO_DATA = 0,
    SLOT_OK,
    SLOT_STALE,
    SLOT_TYPE_ERROR,
} slot_state_t;

/* One Signal K value. A single page has one (the page label names it); a
 * dual page has two, each with a short caption above it. */
typedef struct {
    char label[DINVALUE_LABEL_MAX]; /* only shown for dual pages */
    char path[DINPAGE_PATH_MAX];
    dinpage_type_t type;
    char unit[16];
    int decimals;
    /* Number values: shown = raw * conv_factor + conv_offset, (1, 0) without
     * a conversion. `unit` is the conversion's unit when one is set. */
    double conv_factor;
    double conv_offset;
} dinvalue_t;

typedef struct {
    char id[DINPAGE_STR_MAX];
    char label[DINPAGE_STR_MAX]; /* page title, shown for both kinds */
    int value_count;             /* 1 (single) or 2 (dual) */
    dinvalue_t values[DINPAGE_MAX_VALUES];
    uint32_t update_period_ms;
    uint32_t stale_timeout_ms;
} dinpage_t;

typedef struct {
    char text[DINPAGE_VALUE_MAX]; /* pre-formatted display text */
    slot_state_t state;
} dinvalue_slot_t;

/* LVGL on the panel, landscape. Lights the backlight after the first frame. */
esp_err_t renderer_init(void);

/* Single-value page: page label at top, one big value + unit centered. */
void renderer_show_page(const dinpage_t *page, const dinvalue_slot_t *slot);

/* Two-value page: two captioned values side by side, no title row. */
void renderer_show_dual_page(const dinpage_t *page, const dinvalue_slot_t slots[2]);

/* Shown when no pages are configured at all. */
void renderer_show_no_pages(void);

/* The runtime task: subscriptions from the page list, value cache, encoder
 * paging, rendering. After espos_start() and dinmeter_pages_init(). */
esp_err_t dinmeter_runtime_start(void);

#ifdef __cplusplus
}
#endif
