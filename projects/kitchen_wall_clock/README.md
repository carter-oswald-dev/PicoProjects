# Kitchen Wall Clock (Pico 2W, Arduino)

Minimal wall display firmware for Pico 2W + Waveshare Pico-LCD-1.3 using `arduino-cli`.

The app shows:
- local Coquitlam time (PST/PDT)
- current Coquitlam weather from Open-Meteo

V1 intentionally keeps UI simple:
- one combined screen
- no button interaction

## Wi-Fi Credentials (Local Only)

Project defaults are non-secret placeholders in:
- `projects/kitchen_wall_clock/src/config.h`

To use your real credentials without committing them:
1. Copy local template:
   - `cp projects/kitchen_wall_clock/src/config_local.example.h projects/kitchen_wall_clock/src/config_local.h`
2. Edit `projects/kitchen_wall_clock/src/config_local.h` and set:
   - `WIFI_SSID`
   - `WIFI_PASSWORD`

`config_local.h` is git-ignored and should remain local.

## Build

From repo root:

```bash
./projects/kitchen_wall_clock/build.sh
```

Toolchain:
- Arduino core `rp2040:rp2040` (Earle Philhower)
- Board: `rp2040:rp2040:rpipico2w`

Build without flashing:

```bash
./projects/kitchen_wall_clock/build.sh --no-flash
```

UF2 output:
- `.build/kitchen_wall_clock/kitchen_wall_clock.ino.uf2`
