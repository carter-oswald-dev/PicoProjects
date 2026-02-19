# PicoProjects

Hybrid Arduino + Pico SDK monorepo for Raspberry Pi Pico W / Pico 2W projects.

## Repository Layout

- `projects/`
  - `creepy_halloween_sonar/` (Arduino / `arduino-cli`)
  - `pico2w_lcd_soundboard/` (Arduino / `arduino-cli`)
  - `pico_lcd_generative_art/` (Pico SDK / CMake)
  - `pico_am_radio/` (Pico SDK / CMake)
  - `kitchen_wall_clock/` (Arduino / `arduino-cli`)
- `shared/`
  - `pico_display/` shared LCD driver + text rendering code (buildable by both toolchains)
- `scripts/`
  - root build/verify/flash/doctor/smoke helpers

## Build Commands

### Build all projects (default)

```bash
./scripts/build.sh --all
```

### Build one project

```bash
./scripts/build.sh --project creepy_halloween_sonar
./scripts/build.sh --project pico2w_lcd_soundboard
./scripts/build.sh --project pico_lcd_generative_art
./scripts/build.sh --project pico_am_radio
./scripts/build.sh --project kitchen_wall_clock
```

### Build without flashing

```bash
./scripts/build.sh --all --no-flash
```

### Per-project build entrypoints

```bash
./projects/creepy_halloween_sonar/build.sh
./projects/pico2w_lcd_soundboard/build.sh
./projects/pico_lcd_generative_art/build.sh
./projects/pico_am_radio/build.sh
./projects/kitchen_wall_clock/build.sh
```

## Verify / Smoke / Doctor

```bash
./scripts/doctor.sh
./scripts/verify.sh --all
./scripts/smoke.sh --project pico_lcd_generative_art
```

- `verify.sh` runs prerequisite checks, builds, preset sync regression, then smoke checks.
- `smoke.sh` reports `PASS` or explicit `SKIPPED` when hardware is unavailable.

## Root CMake Superbuild

```bash
cmake -S . -B .build/root
cmake --build .build/root --target build_all
cmake --build .build/root --target verify_all
```

Available CMake targets:
- `build_creepy_halloween_sonar`
- `build_pico2w_lcd_soundboard`
- `build_pico_lcd_generative_art`
- `build_pico_am_radio`
- `build_kitchen_wall_clock`
- `build_all`
- `verify_all`

## Flashing Behavior

When build scripts run without `--no-flash`, they attempt UF2 copy to:
- macOS: `/Volumes/RPI-RP2` or `/Volumes/RP2350`
- Linux: `/media/$USER/RPI-RP2`, `/media/$USER/RP2350`, `/run/media/$USER/RPI-RP2`, or `/run/media/$USER/RP2350`

If no mount is present, flashing is skipped and reported.

## Shared Change Policy

If files under `shared/` change, rebuild all dependent projects (prefer `./scripts/build.sh --all` or `./scripts/verify.sh --all`).
