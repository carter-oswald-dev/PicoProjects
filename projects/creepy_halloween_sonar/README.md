# Creepy Halloween Sonar (Pico W)

Proximity-triggered Bluetooth ghost sound effect for Raspberry Pi Pico W.

The firmware reads distance from an HC-SR04 ultrasonic sensor and streams synthesized audio to a Bluetooth speaker using A2DP. As objects get closer, pitch and loudness increase with smoothing to reduce audible artifacts.

## Blog Post

This project is described here:
- https://www.pesfandiar.com/blog/2025/12/02/creepy-sonar-ghost-microcontroller

## Hardware

- Board: Raspberry Pi Pico W
- Sensor: HC-SR04 ultrasonic rangefinder
- Speaker: Bluetooth A2DP sink

### Wiring Notes

- `TRIG` -> `GP2`
- `ECHO` -> `GP3` through voltage divider (10k / 15k) to reduce 5V output to ~3V
- Sensor power: `VBUS` + `GND`

## Build

From repo root:

```bash
./projects/creepy_halloween_sonar/build.sh
```

Build without flashing:

```bash
./projects/creepy_halloween_sonar/build.sh --no-flash
```

UF2 output:
- `.build/creepy_halloween_sonar/creepy_halloween_sonar.ino.uf2`

## Notes

- Toolchain: Arduino (`arduino-cli`) with `rp2040:rp2040` core.
- Speaker MAC is currently hardcoded in `creepy_halloween_sonar.ino`.
