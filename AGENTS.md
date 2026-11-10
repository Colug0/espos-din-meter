# espos-din-meter

ESP-IDF 6 firmware for the **M5Stack DIN Meter** (ESP32-S3FN8, 8 MB flash,
**no PSRAM**, 135×240 ST7789, rotary encoder with push button, buzzer). It
shows Signal K values on configurable pages, one page per encoder step. Pages
are configured in the device's web UI only, never on the device.

Built on **espOS** (`espos/` submodule, pinned to a release tag): WiFi,
provisioning portal, config store and web UI, Signal K discovery, token,
stream in and out, PUT, REST, mDNS, health watchdog, logs, core dump and
signed OTA are espOS. This repo is the display on top.

## Layout

| Path | What |
|---|---|
| `components/dinmeter_hal` | Pins, ST7789 panel over SPI, encoder (PCNT) and button |
| `components/dinmeter_pages` | Page document in NVS, its validation, `GET/PUT /api/v1/pages`, the unit conversion table and `GET /api/v1/pages/conversions` |
| `main/` | `app_main.c` (espOS start, router workaround), `runtime.c` (subscriptions, value cache, paging), `renderer.c` (LVGL) |
| `ui/` | Vite project: espOS's web UI from the submodule plus the Pages tab. `npm run build` writes `ui-dist/` |
| `ui-dist/` | The gzipped bundle, **committed**, packed into the LittleFS image |

## Invariants

1. **LVGL only under `lvgl_port_lock()`.** The runtime task holds it for the
   few label updates of a redraw, nothing else touches LVGL.
2. **Signal K callbacks copy and return.** They run on the stream task.
3. **A wrong value never looks right.** A type that does not match the page
   shows `XX` in red, an old value turns grey after `staleTimeoutMs`, no
   value yet shows `...`. Labels have a fixed width and clip.
4. **Conversion ids are stored in owners' page documents.** Add new ones to
   `components/dinmeter_pages/src/conversions.c`; never rename or remove one.
5. **No animations.** Redraw only on a page switch or a changed value.

## Board facts that bite

* **No PSRAM.** Internal RAM is the only heap, and it runs out first. LVGL's
  pool is 16 KB (`CONFIG_LV_MEM_SIZE`); at the 64 KB default free heap fell to
  ~18 KB and WiFi, HTTP and Signal K failed in ways that looked like network
  bugs. `app_main` logs `heap ... min` once a minute; watch that number.
* **Bluetooth does not fit** next to WiFi, HTTP, Signal K and LVGL:
  `espos_ble` failed with `ESP_ERR_NO_MEM`.
* The panel gap is (40, 52) for landscape, with `swap_xy` and `mirror_y` in
  the LVGL port. Other values show noise along two edges.
* docs.m5stack.com swaps the button (GPIO42) and buzzer (GPIO3) pins. The
  M5Unified source is right.
* GPIO9 drives the EN pin of an AW35122 load switch that powers the
  backlight, so dimming is PWM on that switch. At 5 kHz the panel stayed
  black; 200 Hz (M5Stack's own demo) works. Every LEDC timer on the S3 shares
  one clock source, so the backlight and buzzer timers both use APB.
* The ESP32-S3 restarts without deauthenticating. The boat's MikroTik then
  keeps the old station state and delivers nothing to the device for minutes.
  espOS's `wifi.fresh_assoc` drops the first association of every boot;
  `app_main.c` turns it on unless the owner has set it. Check the router
  log for "reconnecting" before blaming firmware for a lost Signal K link.
* `espos_start_opts_t.board` is `"M5Stack DIN Meter"` and the CMake project
  name is `din_meter`. Never change either: OTA and the registry match on them.

## Build

ESP-IDF v6.0.3 in WSL2 (Ubuntu), the version in `.idf-version`. Always build
through the wrapper, never bare `idf.py build`:

```bash
. ~/esp/esp-idf-v6.0.3/export.sh
git submodule update --init --recursive
scripts/build.sh -B build build
```

ESP-IDF refuses a project path that contains spaces, and building on `/mnt/c`
is slow: mirror a Windows checkout into WSL with `rsync -a --delete`
(excluding `build*/`, `managed_components/`, `sdkconfig*`,
`dependencies.lock`, `secure_boot_signing_key.pem`, `ui/node_modules/`) and
build there.

**Windows checkouts:** espOS has no `.gitattributes`, so with
`core.autocrlf=true` its shell scripts arrive with CRLF and fail in WSL
("bash\r"). Set `git -C espos config core.autocrlf false` and check the
submodule out again.

The web UI: `cd ui && npm ci && npm run build`, then commit `ui-dist/`. Build
the UI first and the firmware after; a firmware build running at the same
time packs the old bundle.

Flash from Windows with esptool, always with `@flash_args` (it also resets
`otadata`):

```powershell
cd \\wsl$\Ubuntu\home\thomas\dev\espos-din-meter\build
py -m esptool --chip esp32s3 -p COMx -b 460800 --before default-reset --after hard-reset write-flash "@flash_args"
```

Zero warnings is the gate; CI fails on compiler, linker and CMake warnings.
sdkconfig: espOS's defaults first, this repo's `sdkconfig.defaults` holds only
what differs, a git-ignored `sdkconfig.local` takes bench overrides. Defaults
reach a fresh `sdkconfig` only, so delete it after changing any of them.

An OTA update replaces the app only. The web UI lives in the `storage`
partition (`espos_project_ui_partition(DIR ...)`), which only a USB flash
writes, so a release that changes `ui/` needs a USB flash to bring the new
page to a device.

Verify on the device: `curl http://<device>/api/v1/logs?limit=200`,
`/api/v1/system/info`, `/api/v1/sk/status`, `/api/v1/pages`.

## The test server is the boat

The only Signal K server is the owner's boat server. **Every contact with it
needs the owner's approval first**: connecting, sending a request, installing
or configuring a plugin, and above all any PUT or delta that changes state
there. Say what will be sent and what it changes, then wait. Never sign in on
the owner's behalf.

## Never commit

WiFi SSIDs or passwords, server IPs or hostnames, API keys, tokens, `*.pem`,
page documents from the boat, or any other boat configuration: not in source,
`sdkconfig.defaults` or Kconfig defaults. Site config lives in NVS.

## Licensing

Apache-2.0. Every file carries the two SPDX lines. Runtime and bundled
dependencies stay permissive.

## Commits and pull requests

* Conventional Commits, `<type>(<scope>): <subject>`, imperative, lower case,
  no trailing period, subject at most 50 characters. PR titles become the
  release notes.
* Types: `feat`, `fix`, `perf`, `refactor`, `revert`, `docs`, `test`, `build`,
  `ci`, `chore`. Scopes: `hal`, `display`, `pages`, `runtime`, `ui`, `ci`,
  `deps`.
* Commits end with `Co-Authored-By: Claude …`, PR descriptions with
  "🤖 Generated with Claude Code".
* Every change goes through a pull request from its own branch; nothing is
  pushed to `main` directly. One logical change per PR. Squash-merge every
  PR with the PR title as the subject:
  `gh pr merge N --squash --subject "<PR title> (#N)" --match-head-commit <sha>`.
* CodeRabbit reviews each PR (`.coderabbit.yaml`, profile "assertive"). Check
  every comment against the code: fix what holds, often re-testing on the
  device, and answer what does not with the reason. A reply on a review
  thread ends with `_🤖 Addressed by [Claude Code](https://claude.com/claude-code)_`;
  resolve the thread after answering. If no review starts, comment
  `@coderabbitai review`.
* No version bumps in feature or fix PRs; release-please owns `version.txt`.
  Its release PR gets no checks (opened with the workflow token): close and
  reopen it once by hand.
* Never commit, push or merge unless the owner asks.

## Pull requests to other repos

Read the target repo's own `AGENTS.md`, `CONTRIBUTING` and pull request
template first; their rules win. AI attribution stays (`Co-Authored-By`,
"Generated with Claude Code") unless the target repo itself asks for none:
espos-p4-cockpit's `AGENTS.md`, for example, says "No AI attribution
anywhere". The registry asks nothing of the kind. Conventional Commits with a
subject of at most 50 characters, squash-merge and no version bumps hold for
all of the espOS repos. Land espOS changes first, then bump the submodule here
in a separate PR.

## Comments and docs

Comments say why, not what. No development narrative in comments or docs;
that is git history. Docs follow espOS's style: lead with what the reader does
or gets, short sentences, a number or file name rather than an adjective.
Write **Signal K** with a space and **espOS** with a small e.
