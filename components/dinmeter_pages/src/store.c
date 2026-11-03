/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "dinmeter_pages/conversions.h"
#include "dinmeter_pages/pages.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define NVS_NAMESPACE "dinmeter"
#define NVS_KEY "pages"
#define DEFAULT_UPDATE_PERIOD_MS 1000.0
#define MIN_UPDATE_PERIOD_MS 100.0
#define MAX_UPDATE_PERIOD_MS 3600000.0
#define DEFAULT_DOC "{\"pages\":[]}"

static const char *TAG = "din_pages";
static SemaphoreHandle_t s_lock;
static char *s_cached_json; /* valid and normalized once init() has run */
static dinmeter_pages_change_cb_t s_change_cb;
static void *s_change_cb_arg;

static bool is_nonempty_string(const cJSON *item)
{
    return cJSON_IsString(item) && item->valuestring[0] != '\0';
}

static bool is_valid_type(const cJSON *item)
{
    return cJSON_IsString(item) &&
           (strcmp(item->valuestring, "number") == 0 || strcmp(item->valuestring, "string") == 0 ||
            strcmp(item->valuestring, "boolean") == 0);
}

/* The value fields of a single page, or of one entry of a dual page's
 * "values". `where` becomes *err_path on failure. */
static const char *validate_value_fields(const cJSON *obj, const char *where, const char **err_path)
{
    *err_path = where;
    if (!is_nonempty_string(cJSON_GetObjectItem(obj, "path"))) {
        return "missing/empty \"path\"";
    }
    if (!is_valid_type(cJSON_GetObjectItem(obj, "type"))) {
        return "\"type\" must be \"number\", \"string\" or \"boolean\"";
    }
    const cJSON *decimals = cJSON_GetObjectItem(obj, "decimals");
    if (decimals && (!cJSON_IsNumber(decimals) || decimals->valuedouble < 0 || decimals->valuedouble > 10)) {
        return "\"decimals\" must be a number 0..10";
    }
    const cJSON *unit = cJSON_GetObjectItem(obj, "unit");
    if (unit && !cJSON_IsString(unit)) {
        return "\"unit\" must be a string";
    }
    const cJSON *conv = cJSON_GetObjectItem(obj, "conversion");
    if (conv && (!cJSON_IsString(conv) || !dinmeter_conversion_valid(conv->valuestring))) {
        return "\"conversion\" is not a known conversion id";
    }
    return NULL;
}

/* Validates `root` and fills in defaults (kind, updatePeriodMs,
 * staleTimeoutMs). NULL on success, else the first problem. */
static const char *validate_and_normalize(cJSON *root, const char **err_path)
{
    static char path_buf[24];
    static char sub_path_buf[40];

    cJSON *pages = cJSON_GetObjectItem(root, "pages");
    *err_path = "";
    if (!cJSON_IsArray(pages)) {
        return "expected {\"pages\": [...]}";
    }
    int n = cJSON_GetArraySize(pages);
    if (n > DINMETER_PAGES_MAX) {
        *err_path = "pages";
        return "too many pages";
    }

    for (int i = 0; i < n; i++) {
        cJSON *page = cJSON_GetArrayItem(pages, i);
        snprintf(path_buf, sizeof(path_buf), "pages[%d]", i);
        *err_path = path_buf;
        if (!cJSON_IsObject(page)) {
            return "expected an object";
        }

        const cJSON *id = cJSON_GetObjectItem(page, "id");
        if (!is_nonempty_string(id)) {
            return "missing/empty \"id\"";
        }
        if (!is_nonempty_string(cJSON_GetObjectItem(page, "label"))) {
            return "missing/empty \"label\"";
        }
        for (int j = 0; j < i; j++) {
            const cJSON *other_id = cJSON_GetObjectItem(cJSON_GetArrayItem(pages, j), "id");
            if (strcmp(other_id->valuestring, id->valuestring) == 0) {
                return "duplicate \"id\"";
            }
        }

        const cJSON *kind = cJSON_GetObjectItem(page, "kind");
        bool is_dual = false;
        if (!kind) {
            cJSON_AddStringToObject(page, "kind", "single");
        } else if (cJSON_IsString(kind) && strcmp(kind->valuestring, "dual") == 0) {
            is_dual = true;
        } else if (!cJSON_IsString(kind) || strcmp(kind->valuestring, "single") != 0) {
            return "\"kind\" must be \"single\" or \"dual\"";
        }

        if (is_dual) {
            const cJSON *values = cJSON_GetObjectItem(page, "values");
            if (!cJSON_IsArray(values) || cJSON_GetArraySize(values) != 2) {
                return "\"kind\":\"dual\" needs exactly 2 entries in \"values\"";
            }
            for (int v = 0; v < 2; v++) {
                const cJSON *val = cJSON_GetArrayItem(values, v);
                snprintf(sub_path_buf, sizeof(sub_path_buf), "pages[%d].values[%d]", i, v);
                *err_path = sub_path_buf;
                if (!cJSON_IsObject(val)) {
                    return "expected an object";
                }
                if (!is_nonempty_string(cJSON_GetObjectItem(val, "label"))) {
                    return "missing/empty \"label\"";
                }
                const char *msg = validate_value_fields(val, sub_path_buf, err_path);
                if (msg) {
                    return msg;
                }
            }
            *err_path = path_buf;
        } else {
            const char *msg = validate_value_fields(page, path_buf, err_path);
            if (msg) {
                return msg;
            }
        }

        double period_ms = DEFAULT_UPDATE_PERIOD_MS;
        const cJSON *period = cJSON_GetObjectItem(page, "updatePeriodMs");
        if (period) {
            if (!cJSON_IsNumber(period) || period->valuedouble < MIN_UPDATE_PERIOD_MS ||
                period->valuedouble > MAX_UPDATE_PERIOD_MS) {
                return "\"updatePeriodMs\" out of range";
            }
            period_ms = period->valuedouble;
        } else {
            cJSON_AddNumberToObject(page, "updatePeriodMs", period_ms);
        }

        const cJSON *stale = cJSON_GetObjectItem(page, "staleTimeoutMs");
        if (stale) {
            if (!cJSON_IsNumber(stale) || stale->valuedouble < period_ms) {
                return "\"staleTimeoutMs\" must be a number >= updatePeriodMs";
            }
        } else {
            cJSON_AddNumberToObject(page, "staleTimeoutMs", period_ms * 5.0);
        }
    }
    return NULL;
}

static esp_err_t nvs_load(char **out)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    size_t len = 0;
    err = nvs_get_str(h, NVS_KEY, NULL, &len);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    char *buf = malloc(len);
    if (!buf) {
        nvs_close(h);
        return ESP_ERR_NO_MEM;
    }
    err = nvs_get_str(h, NVS_KEY, buf, &len);
    nvs_close(h);
    if (err != ESP_OK) {
        free(buf);
        return err;
    }
    *out = buf;
    return ESP_OK;
}

static esp_err_t nvs_save(const char *json)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, NVS_KEY, json);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t dinmeter_pages_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    char *loaded = NULL;
    if (nvs_load(&loaded) == ESP_OK) {
        s_cached_json = loaded;
        ESP_LOGI(TAG, "loaded pages document (%u bytes)", (unsigned)strlen(loaded));
        return ESP_OK;
    }
    s_cached_json = strdup(DEFAULT_DOC);
    if (!s_cached_json) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "no stored pages document, starting empty");
    return ESP_OK;
}

char *dinmeter_pages_get_json(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char *copy = strdup(s_cached_json);
    xSemaphoreGive(s_lock);
    return copy;
}

esp_err_t dinmeter_pages_set_json(const char *json, size_t len, const char **err_path, const char **err_msg)
{
    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        *err_path = "";
        *err_msg = "malformed JSON (expected an object)";
        return ESP_ERR_INVALID_ARG;
    }
    const char *msg = validate_and_normalize(root, err_path);
    if (msg) {
        cJSON_Delete(root);
        *err_msg = msg;
        return ESP_ERR_INVALID_ARG;
    }
    char *serialized = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!serialized) {
        return ESP_ERR_NO_MEM;
    }
    if (nvs_save(serialized) != ESP_OK) {
        free(serialized);
        return ESP_FAIL;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    free(s_cached_json);
    s_cached_json = serialized;
    xSemaphoreGive(s_lock);

    if (s_change_cb) {
        s_change_cb(s_change_cb_arg);
    }
    return ESP_OK;
}

void dinmeter_pages_on_change(dinmeter_pages_change_cb_t cb, void *arg)
{
    s_change_cb_arg = arg;
    s_change_cb = cb;
}
