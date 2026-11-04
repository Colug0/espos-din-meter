# Changelog

## 0.1.0 (2026-11-04)

First release: Signal K values on an M5Stack DIN Meter, built on espOS 0.26.0.

### Added

- **Pages** set up in the device's web UI (Pages tab): one or two Signal K values per page; turn the knob for the next page. Unit conversions (0..1 → %, m/s → kn, km/h or mph, K → °C or °F, rad → deg, Pa → hPa or bar, m → ft, m³ → L). A value that stops arriving turns grey; a value of the wrong type shows `XX`. Export and import of the page list.
- **Status on the display**: the WiFi setup network to join, the access request to approve in Signal K, and a corner dot when WiFi (red) or Signal K (amber) is lost.
- **Brightness** in even steps, and a night brightness while Signal K reports `environment.mode` night.
- **Dark mode** for night watches: a long press turns it on; the screen goes dark when unused and wakes on a turn or press. It ends when `environment.mode` turns to day.
- **Buzzer tunes** (RTTTL) at start-up and on a knob press, editable under Config → Sound.

### Notes

- Releases are signed with key `94af317c231f06ab`. A DIN Meter running another build needs one USB flash with this release before it takes OTA updates.
- An OTA update replaces the firmware but not the web UI. A release that changes the web UI needs a USB flash.

**Full Changelog**: https://github.com/Colug0/espos-din-meter/compare/v0.0.0...v0.1.0
