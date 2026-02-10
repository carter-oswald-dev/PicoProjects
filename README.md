# PicoProjects

Arduino CLI projects for Raspberry Pi Pico boards.

## Projects

- `creepy_halloween_sonar/` - Bluetooth creepy sonar tone generator (Pico W)
- `pico2w_lcd_soundboard/` - LCD 4-button Bluetooth soundboard (Pico 2W)

## Build

Use the helper script:

```bash
./scripts/build.sh
./pico2w_lcd_soundboard/build.sh
```

Build outputs are written to `.build/<project-name>/`.

## Preset Lab

For laptop-side preset tuning with immediate audio feedback:

```bash
cd tools/preset_lab
python3 -m http.server 8080
```

Then open `http://localhost:8080`.

## Requirements

- `arduino-cli` available in `PATH`, or Arduino IDE installed at:
  `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`
- Arduino-Pico core installed: `rp2040:rp2040`
