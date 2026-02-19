# Pico AM Radio (RP2350 / Pico 2W)

Single-pin AM/OOK melody transmitter for Raspberry Pi Pico 2W.

The firmware keeps a continuous 1 MHz carrier running in a PIO state machine and applies OOK gating on the same GPIO to play "Shave and a Haircut." Pitch is encoded by toggling carrier visibility at each note frequency derived from MIDI values.

## Antenna Pin

- RF output/antenna pin: `GPIO2`
- For quick tests, connect a short wire to `GPIO2` as a simple antenna.

## Build

From repo root:

```bash
./projects/pico_am_radio/build.sh
```

Build without flashing:

```bash
./projects/pico_am_radio/build.sh --no-flash
```

UF2 output:
- `.build/pico_am_radio/PicoAM.uf2`

## Requirements

- CMake
- `arm-none-eabi-gcc`
- `picotool`
- Pico SDK path (auto-detected by build script or via `PICO_SDK_PATH`)
