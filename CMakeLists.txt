# SPDX-FileCopyrightText: 2026 Thomas Christoffel
# SPDX-License-Identifier: Apache-2.0
#
# espos-din-meter: a pure ESP-IDF 6 project on espOS (espos/ submodule). WiFi,
# config, the web UI, Signal K, mDNS, the health watchdog and signed OTA come
# from espOS; this project adds the M5Stack DIN Meter HAL and the page display.
#
# Build (WSL2, the path must not contain spaces):
#   . ~/esp/esp-idf-v6.0.3/export.sh
#   scripts/build.sh -B build build
cmake_minimum_required(VERSION 3.22)

# The DIN Meter carries 8 MB and no PSRAM. espOS's 8mb.csv: two 3 MB app
# slots and 1728 KB for the web UI. A device keeps the table it was first
# flashed with (OTA replaces app slots, never the table).
include("${CMAKE_CURRENT_LIST_DIR}/espos/cmake/espos_project.cmake")
espos_project_prologue(NAME "din-meter"
                       PARTITIONS "${ESPOS_PARTITIONS_DIR}/8mb.csv")

# The registry "app" and the OTA project check. Never rename it: every device
# in the field would refuse the renamed firmware's updates.
project(din_meter)

# espOS's web UI plus the Pages tab, built by `npm run build` in ui/ and
# committed as ui-dist/.
espos_project_ui_partition(NAME "din-meter" DIR "${CMAKE_CURRENT_LIST_DIR}/ui-dist")
