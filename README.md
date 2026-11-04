# espos-din-meter

Signal K values on an [M5Stack DIN Meter](https://docs.m5stack.com/en/core/DinMeter):
a small display for a DIN rail in the switch panel. It shows one or two values
per page, for example battery charge and current, tank levels, depth or wind.
Turn the knob for the next page.

The display reads the values from your Signal K server over WiFi, at most once
a second. A value that stops arriving turns grey; a value of the wrong type
shows `XX` in red, never a wrong number. Nothing is written to Signal K, and
no plugin is needed on the server.

Built on [espOS](https://github.com/signalk-espOS/espOS).

## Hardware

| Board | Status | Flash |
|---|---|---|
| M5Stack DIN Meter (ESP32-S3, 135×240 display, knob, buzzer) | in use on a boat | 8 MB |

The DIN Meter has no PSRAM. That is why it shows a few values well rather
than a full dashboard.

## Wiring

Power only: 6 to 36 V DC on the power terminal, or USB-C. Nothing else needs
connecting; the Grove ports are not used.

## Install

Use the Chrome or Edge browser and a USB-C cable:

* **[espOS web flasher](https://signalk-espos.github.io/signalk-espos-manager/flash/)**,
  or the Flash page of the signalk-espos-manager plugin on your Signal K
  server. Pick the DIN Meter and follow the steps.
* By hand: download `din-meter-esp32s3-vX.Y.Z-merged.bin` from the
  [latest release](https://github.com/Colug0/espos-din-meter/releases/latest)
  and write it to offset `0x0`, e.g.
  `esptool --chip esp32s3 write-flash 0x0 din-meter-esp32s3-vX.Y.Z-merged.bin`.

Writing the merged image erases the device's settings. For an update, use the
OTA route below instead.

## First setup

1. Power the DIN Meter. It shows **WiFi setup: join espOS-xxxx**. Join that
   WiFi network with your phone, open `http://192.168.4.1` and enter your
   boat's WiFi.
2. The device finds the Signal K server and asks for access. The display shows
   **Approve access**: approve the request in Signal K under
   **Security → Access Requests**.
3. The display shows **No pages yet** and the device's address,
   `espos-xxxx.local`. Open it in a browser and add pages on the **Pages**
   tab.

## Pages

Per page:

| Field | Meaning |
|---|---|
| Label | Title shown on the display |
| Single value / two values | One big value, or two side by side with a caption each |
| Path | Signal K path under `vessels.self`, e.g. `electrical.batteries.house.voltage` |
| Type | `number`, `string` or `boolean`, as Signal K sends it |
| Conversion | From Signal K's SI units to what you want to read: 0..1 → %, m/s → kn, km/h or mph, K → °C or °F, rad → deg, Pa → hPa or bar, m → ft, m³ → L |
| Unit, decimals | For numbers without a conversion |
| Update | How often Signal K may send the value (at least 100 ms) |
| Stale after | When a value that stopped arriving turns grey |

**Export** saves the page list as a JSON file, **Import** loads one, for a
second display or after a reset.

## On the display

* A **red dot** in the corner: no WiFi. An **amber dot**: WiFi, but no Signal K.
* **Brightness**: ▲/▼ on the Pages tab, under Display. A night brightness
  applies while Signal K reports `environment.mode` night; the
  signalk-espos-manager plugin publishes it from the sun at the boat's
  position.
* **Dark mode** for night watches: allow it under Display, then hold the knob
  for a second. The screen goes dark when unused and wakes on a turn or press;
  the first turn only wakes it. Hold the knob again to end it. It also ends when
  Signal K reports day.
* **Sound**: a short tune at start-up and a click on a knob press. Tunes are
  RTTTL and can be changed under Config → Sound; an empty name is silent.

## Updates

Over the air, with the
[signalk-espos-manager](https://github.com/signalk-espOS/signalk-espos-manager)
plugin, or on the device's OTA tab: paste the link to the release's
`din-meter-esp32s3-vX.Y.Z-ota.bin` under **From a URL**. Settings and pages
are kept.

* Releases are signed. A DIN Meter that ran other firmware, including a build
  of your own, needs one USB flash of a release before it takes OTA updates.
* An OTA update replaces the firmware, not the web pages. When the release
  notes say the web pages changed, flash over USB to get them.

## When it misbehaves

* The device's web page: the Status, WiFi and Signal K tabs say what is
  connected and what is not.
* `http://espos-xxxx.local/api/v1/logs` holds the last few minutes of the log.
* The display stays on "Approve access": the request in Signal K was not
  approved yet, or was denied. After a denial, request access again on the
  device's Signal K tab.
* Report problems in the
  [issues](https://github.com/Colug0/espos-din-meter/issues).

## Building

See [AGENTS.md](AGENTS.md), "Build".

## Licence

Apache-2.0.
