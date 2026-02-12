# Pico 2W LCD Soundboard

Bluetooth soundboard firmware for Pico 2W + Waveshare Pico-LCD-1.3.

The project renders a button-driven UI on the LCD, plays synthesized presets over Bluetooth A2DP, and controls remote speaker volume via AVRCP absolute volume.

## Features

- 4-slot on-screen preset paging and playback (`A/B/X/Y`)
- Directional key controls for paging and volume
- Preset-driven synth engine with LFO/repeat/retrigger/arp/phaser parameters
- Shared LCD driver and text renderer from `shared/pico_display`

## Build

From repo root:

```bash
./projects/pico2w_lcd_soundboard/build.sh
```

Build without flashing:

```bash
./projects/pico2w_lcd_soundboard/build.sh --no-flash
```

UF2 output:
- `.build/pico2w_lcd_soundboard/pico2w_lcd_soundboard.ino.uf2`

## Preset Lab

Local preset editor lives under:
- `projects/pico2w_lcd_soundboard/tools/preset_lab/`

Run it:

```bash
cd projects/pico2w_lcd_soundboard/tools/preset_lab
python3 -m http.server 8080
```

Sync presets between firmware and lab:

```bash
python3 projects/pico2w_lcd_soundboard/tools/preset_lab/sync_presets.py fw-to-lab
python3 projects/pico2w_lcd_soundboard/tools/preset_lab/sync_presets.py lab-to-fw
```

## Notes

- Toolchain: Arduino (`arduino-cli`) with `rp2040:rp2040` core.
- Build script injects shared libraries via `--libraries <repo>/shared`.
