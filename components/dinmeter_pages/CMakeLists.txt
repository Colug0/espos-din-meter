# SPDX-FileCopyrightText: 2026 Thomas Christoffel
# SPDX-License-Identifier: Apache-2.0
idf_component_register(
    SRCS src/store.c src/api.c src/conversions.c
    INCLUDE_DIRS include
    PRIV_REQUIRES espos_httpd nvs_flash espressif__cjson)
