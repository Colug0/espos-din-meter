# espos-din-meter

Signal K values on an [M5Stack DIN Meter](https://docs.m5stack.com/en/core/M5DinMeter):
a small display for a DIN rail that shows one or two values per page. Turn the
knob to go to the next page.

Each page reads Signal K paths from your server, for example battery state of
charge and current, tank level or wind speed. Values arrive at most once a
second. A value that stops arriving turns grey; a value of the wrong type
shows `XX` in red instead of a wrong number.

Built on [espOS](https://github.com/signalk-espOS/espOS).

## Status

In development, not released yet. Runs on the author's boat.

## First setup

1. Power the DIN Meter. On first start it opens a WiFi network `espOS-xxxx`.
   Join it and enter your boat's WiFi.
2. The device finds the Signal K server and asks for access. Approve it in
   Signal K under **Security → Access Requests**.
3. Open the device's web page (its address is shown in your router, or
   `http://din-meter-xxxx.local`) and add pages on the **Pages** tab.

## Pages

Per page:

| Field | Meaning |
|---|---|
| Label | Title shown on the display |
| Single value / two values | One big value, or two side by side with a caption each |
| Path | Signal K path under `vessels.self`, e.g. `electrical.batteries.house.voltage` |
| Type | `number`, `string` or `boolean`, as Signal K sends it |
| Conversion | Turns Signal K's SI units into what you want to read: 0..1 → %, m/s → kn, K → °C, m³ → L, … |
| Unit, decimals | For numbers without a conversion |
| Update | How often Signal K may send the value (at least 100 ms) |
| Stale after | When a value that stopped arriving turns grey |

**Export** saves the page list as a JSON file, **Import** loads one.

## Building

See [AGENTS.md](AGENTS.md), "Build".

## Licence

Apache-2.0.
