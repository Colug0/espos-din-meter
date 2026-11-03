/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdbool.h>
#include <string.h>

#include "dinmeter_pages/conversions.h"

/* Ids are stored in page documents: never rename one, only add. */
static const dinmeter_conversion_t k_conversions[] = {
    {"ratio_percent", "0..1 \xe2\x86\x92 %", 100.0, 0.0, "%"},
    {"ms_to_kn", "m/s \xe2\x86\x92 kn", 1.943844, 0.0, "kn"},
    {"ms_to_kmh", "m/s \xe2\x86\x92 km/h", 3.6, 0.0, "km/h"},
    {"ms_to_mph", "m/s \xe2\x86\x92 mph", 2.236936, 0.0, "mph"},
    {"kelvin_to_celsius", "K \xe2\x86\x92 \xc2\xb0" "C", 1.0, -273.15, "C"},
    {"kelvin_to_fahrenheit", "K \xe2\x86\x92 \xc2\xb0" "F", 1.8, -459.67, "F"},
    {"rad_to_deg", "rad \xe2\x86\x92 deg", 57.29577951308232, 0.0, "deg"},
    {"pa_to_hpa", "Pa \xe2\x86\x92 hPa", 0.01, 0.0, "hPa"},
    {"pa_to_bar", "Pa \xe2\x86\x92 bar", 0.00001, 0.0, "bar"},
    {"m_to_ft", "m \xe2\x86\x92 ft", 3.280840, 0.0, "ft"},
    {"m3_to_l", "m\xc2\xb3 \xe2\x86\x92 L", 1000.0, 0.0, "L"},
};

#define N_CONVERSIONS ((int)(sizeof(k_conversions) / sizeof(k_conversions[0])))

const dinmeter_conversion_t *dinmeter_conversion_find(const char *id)
{
    if (!id) {
        return NULL;
    }
    for (int i = 0; i < N_CONVERSIONS; i++) {
        if (strcmp(id, k_conversions[i].id) == 0) {
            return &k_conversions[i];
        }
    }
    return NULL;
}

bool dinmeter_conversion_valid(const char *id)
{
    return id && (strcmp(id, "none") == 0 || dinmeter_conversion_find(id) != NULL);
}

const dinmeter_conversion_t *dinmeter_conversions(int *count)
{
    *count = N_CONVERSIONS;
    return k_conversions;
}
