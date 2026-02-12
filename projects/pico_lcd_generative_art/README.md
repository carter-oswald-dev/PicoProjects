# Pico LCD Generative Art (RP2350 / Pico 2W)

Real-time plasma-style visual synthesizer for Pico 2W + Waveshare 1.3" 240x240 LCD.

The app renders plasma frames and writes them to the LCD using a shared ST7789 driver. It uses both cores: one for rendering and one for display writeout.

## Blog Post

Project write-up:
- https://www.pesfandiar.com/blog/2026/01/13/realtime-visual-synth-microcontroller-lcd

## Controls

- `A/B/X/Y`: switch plasma modes
- `UP/DOWN`: increase/decrease animation time step
- `LEFT/RIGHT`: hue shift
- `CTRL`: reset speed and hue shift

## Build

From repo root:

```bash
./projects/pico_lcd_generative_art/build.sh
```

Build without flashing:

```bash
./projects/pico_lcd_generative_art/build.sh --no-flash
```

UF2 output:
- `.build/pico_lcd_generative_art/GenerativeArt.uf2`

## Requirements

- CMake
- `arm-none-eabi-gcc`
- `picotool`
- Pico SDK path (auto-detected by build script or via `PICO_SDK_PATH`)

## Upstream Snapshot

This project was vendored from:
- Repository: https://github.com/pesfandiar/PicoLcdGenerativeArt
- Commit: `ab06bacbae09e8585f2520cafa02c6a9be896b67`

Imported files:
- `GenerativeArt.c`
- `CMakeLists.txt` (adapted locally to link shared LCD driver)
- `pico_sdk_import.cmake`
- `raspberrypi-swd.cfg`

`LcdDriver.c` and `LcdDriver.h` are intentionally not copied in this project; it links the canonical shared implementation from `shared/pico_display/src`.
