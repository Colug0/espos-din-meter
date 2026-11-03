/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"
#include "dinmeter_pages/conversions.h"
#include "dinmeter_pages/pages.h"
#include "espos_httpd.h"

static esp_err_t pages_get(httpd_req_t *req)
{
    char *json = dinmeter_pages_get_json();
    if (!json) {
        return espos_httpd_send_error(req, "500 Internal Server Error", "no_mem", "out of memory");
    }
    esp_err_t err = espos_httpd_send_json(req, NULL, json);
    free(json);
    return err;
}

static esp_err_t pages_put(httpd_req_t *req)
{
    if (!espos_httpd_require_json(req)) {
        return ESP_OK;
    }
    char *body = NULL;
    size_t len = 0;
    if (espos_httpd_read_body(req, &body, &len) != ESP_OK) {
        return ESP_FAIL;
    }
    const char *err_path = "";
    const char *err_msg = "";
    esp_err_t err = dinmeter_pages_set_json(body, len, &err_path, &err_msg);
    free(body);

    if (err == ESP_ERR_INVALID_ARG) {
        /* err_path and err_msg are fixed strings without quotes. */
        char resp[192];
        snprintf(resp, sizeof(resp), "{\"error\":\"validation\",\"path\":\"%s\",\"message\":\"%s\"}", err_path,
                 err_msg);
        return espos_httpd_send_json(req, "400 Bad Request", resp);
    }
    if (err != ESP_OK) {
        return espos_httpd_send_error(req, "500 Internal Server Error", "write_failed", esp_err_to_name(err));
    }
    return espos_httpd_send_json(req, NULL, "{\"changed\":true}");
}

static esp_err_t conversions_get(httpd_req_t *req)
{
    int n = 0;
    const dinmeter_conversion_t *table = dinmeter_conversions(&n);
    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(root, "conversions");
    for (int i = 0; list && i < n; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            break;
        }
        cJSON_AddStringToObject(item, "id", table[i].id);
        cJSON_AddStringToObject(item, "label", table[i].label);
        cJSON_AddStringToObject(item, "unit", table[i].unit);
        cJSON_AddItemToArray(list, item);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return espos_httpd_send_error(req, "500 Internal Server Error", "no_mem", "out of memory");
    }
    esp_err_t err = espos_httpd_send_json(req, NULL, json);
    free(json);
    return err;
}

esp_err_t dinmeter_pages_register_api(void)
{
    static const httpd_uri_t uris[] = {
        {.uri = "/api/v1/pages", .method = HTTP_GET, .handler = pages_get},
        {.uri = "/api/v1/pages", .method = HTTP_PUT, .handler = pages_put},
        {.uri = "/api/v1/pages/conversions", .method = HTTP_GET, .handler = conversions_get},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        esp_err_t err = espos_httpd_register(&uris[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}
