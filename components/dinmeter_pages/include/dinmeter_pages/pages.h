/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * The page list: which Signal K values the display shows, one page per
 * encoder step. Stored as one JSON document in NVS and replaced as a whole
 * from the web UI. Shape (README.md, "Pages"):
 *
 *   {"pages":[{"id","label","kind":"single","path","type","unit","decimals",
 *              "conversion","updatePeriodMs","staleTimeoutMs"},
 *             {"id","label","kind":"dual","values":[{...},{...}], ...}]}
 */
#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DINMETER_PAGES_MAX 32

/* Load the stored document, or an empty one. NVS must be up (espos_init). */
esp_err_t dinmeter_pages_init(void);

/* Malloc'd copy of the current document. NULL only when out of memory. */
char *dinmeter_pages_get_json(void);

/* Validate and store `json`. ESP_ERR_INVALID_ARG with *err_path and *err_msg set
 * (static strings) when the document is wrong, ESP_FAIL when NVS refuses it. */
esp_err_t dinmeter_pages_set_json(const char *json, size_t len, const char **err_path, const char **err_msg);

/* GET/PUT /api/v1/pages and GET /api/v1/pages/conversions. After
 * espos_start(), which starts the HTTP server. */
esp_err_t dinmeter_pages_register_api(void);

/* Called after every stored change, on the writer's task (httpd). One
 * consumer; a second call replaces the first. */
typedef void (*dinmeter_pages_change_cb_t)(void *arg);
void dinmeter_pages_on_change(dinmeter_pages_change_cb_t cb, void *arg);

#ifdef __cplusplus
}
#endif
