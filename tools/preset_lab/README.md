# Pico Preset Lab

Local web app for designing `kSoundPresets` with immediate audio feedback on laptop.

## What It Does

- Parses the C++ `kSoundPresets` initializer block.
- Mirrors the firmware synth engine (LFO, repeat, retrigger, arp jump, noise filtering, phaser).
- Lets you tweak parameters and audition instantly.
- Exports canonical C++ initializer text you can paste into `pico2w_lcd_soundboard/SoundPresets.h`.

## Run

```bash
cd tools/preset_lab
python3 -m http.server 8080
```

Open `http://localhost:8080`.

## Workflow

1. Paste the preset block (or load the demo block).
2. Click `Parse`.
3. Select a preset and tweak values in the inspector.
4. Use `Play` or enable `Auto-preview on change`.
5. Click `Copy Block` and paste into `SoundPresets.h`.

## Notes

- Parser scope is the preset array block only (or snippet containing `kSoundPresets`).
- No firmware build/flash automation is included.
- The app is local-only and uses no external dependencies.

## Sync Script

Use the helper to keep firmware and lab demo block aligned:

```bash
# firmware SoundPresets.h -> lab DEMO_BLOCK
python3 tools/preset_lab/sync_presets.py fw-to-lab

# lab DEMO_BLOCK -> firmware SoundPresets.h (also updates PRESET_COUNT)
python3 tools/preset_lab/sync_presets.py lab-to-fw
```
