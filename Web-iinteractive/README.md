# Pico AM Radio — MicroPython Web Edition

Play MIDI files as OOK AM transmissions using a **Raspberry Pi Pico 2 W** — no PC needed after setup. Plug into any USB power bank, connect your phone or laptop to the Pico's own WiFi network, and control playback from a browser.

Tune a nearby AM radio to your chosen frequency (default **1000 kHz / 1 MHz**) and hold the antenna wire close to it.

---

## Why GP0 (Pin 1)?

This build uses **GP0 (physical Pin 1)** — the top-left corner pin — as the antenna output.

On the Pico, Pin 1 sits at the very edge of the board. If you accidentally brush the antenna wire against an adjacent pin, you'll only touch **Pin 2 (GP1)**, which is a plain GPIO and safe. A corner pin is the safest choice when dangling a bare wire off the board.

```
┌─────────────────────┐
│ [Pin 1 / GP0] ←──── antenna wire goes here
│ [Pin 2 / GP1]
│ [Pin 3 / GND]
│  ...
```

Pin 1 is also the easiest to identify at a glance — no counting needed.

---

## Hardware

| You need | Notes |
|---|---|
| Raspberry Pi **Pico 2 W** | Must be the W variant for WiFi |
| ~20 cm wire | Solid core or stranded, bare end |
| AM radio | Portable battery radio works best |
| USB power bank or charger | Any 5 V USB source |

Solder or clip the wire to **Pin 1 (GP0)**. That's it.

---

## Installation

> [!WARNING]
> **Monophonic only — one note at a time.**
> This transmitter works by rapidly toggling GP0 on and off at each note's audio frequency. Because there is only one pin, it can only produce one pitch at any given moment. MIDI files with chords, harmonies, or overlapping notes (polyphony) will not play correctly — simultaneous notes will be collapsed and only one voice will be heard. Use a **single-melody, monophonic MIDI file** with no overlapping notes. If your MIDI sounds garbled or skips notes, open it in a free tool such as [MuseScore](https://musescore.org) or [LMMS](https://lmms.io) and export only the melody track as a new type-0 MIDI file.

1. Flash your Pico 2 W with the latest **MicroPython wireless firmware** from [micropython.org/download/RPI_PICO2_W](https://micropython.org/download/RPI_PICO2_W/). The standard (non-W) build does not include the WiFi driver.
2. Open **Thonny** (or use `mpremote`) with the Pico connected via USB.
3. Copy all three files to the **root** of the Pico's filesystem:
   - `main.py`
   - `radio.py`
   - your `.mid` file(s)
4. Disconnect from Thonny and plug the Pico into any USB power source. It boots and serves the web UI automatically — no PC needed.

---

## Connecting to the Web UI

1. On any device (phone, tablet, laptop), open WiFi settings and join the network **`PicoAMRadio`** — it has no password.
2. Open a browser and navigate to **`http://192.168.4.1`**
3. The control page loads immediately.

> **Note:** While connected to PicoAMRadio your device has no internet access — the Pico is just a local hotspot. On some phones you may get a "No internet connection" warning; tap "Stay connected" or "Use this network anyway".

---

## Using the Web UI

### Playing a file

1. Select a `.mid` file from the dropdown.
2. Press **▶ Start**. The status badge changes to **TRANSMITTING**.

### Stopping

Press **■ Stop** at any time. Playback ends within a fraction of a second.

### Changing the carrier frequency

> [!WARNING]
> **You must stop the transmission before changing frequency.** The Set button is disabled while transmitting. Stop first, change the frequency, then press Start again to transmit on the new frequency.

1. Stop any current transmission.
2. Enter a frequency in **kHz** in the frequency box (500–30000).
3. Press **Set**.
4. Start playback again.

The frequency range is limited to the **AM medium wave broadcast band: 530 kHz – 1700 kHz**. This is the range where the Pico's GPIO drive strength works best and where standard AM radios tune.

---

## Changing the Tune

### Use a different MIDI file

Copy any additional `.mid` files to the Pico's filesystem using Thonny while connected via USB. They appear in the web UI dropdown automatically on the next page load.

Good free sources:

- [Kunstderfuge.com](https://www.kunstderfuge.com/) — classical
- [VGMusic.com](https://www.vgmusic.com/) — video game music
- [FreeMidi.org](https://freemidi.org/)

> **Tip:** Keep files small (under ~50 KB). Large orchestral MIDI with many tracks parses slowly in MicroPython. Single-track or melody-only exports work best.

### Export a melody-only MIDI from MuseScore

1. Open your score in MuseScore.
2. Right-click the melody staff → **Select → All Similar Elements in Same Staff**.
3. File → **Export** → choose **MIDI**, enable "Export only selection".
4. Save and copy the resulting `.mid` to the Pico.

---

## Troubleshooting

**Nothing on the radio**
- Make sure your radio is on AM, not FM.
- Tune slowly across 900–1100 kHz (or wherever you set the carrier).
- Hold the radio's antenna right next to the Pico wire.
- Confirm `TONE_GPIO = 0` is set in `radio.py`.

**Can't find the PicoAMRadio network**
- The Pico takes ~3 seconds to boot and start the AP. Wait a moment and refresh your WiFi scan.
- Make sure you flashed the **wireless** MicroPython build (RPI_PICO2_W), not the standard Pico build.

**Browser shows "Site can't be reached" at 192.168.4.1**
- Confirm you're connected to PicoAMRadio, not your normal WiFi.
- Some Android phones switch back to mobile data automatically — disable mobile data temporarily.

**Garbled or wrong notes**
- Use a simple melody-only MIDI. Drum tracks (channel 10) and dense chords confuse the single-voice parser.
- Re-export from a DAW as a type-0, single-track MIDI.

**`MidiParseError: SMPTE timecode not supported`**
- Re-export from MuseScore, GarageBand, or LMMS using **PPQ / ticks-per-beat** timing — this is the default in every common tool.

**Buzzing or jitter on high notes**
- Normal above ~C6 due to MicroPython interrupt overhead. Notes in the C4–C5 range are cleanest.

---

## MIDI Note Reference

```
Octave  C    C#   D    D#   E    F    F#   G    G#   A    A#   B
  3     48   49   50   51   52   53   54   55   56   57   58   59
  4     60   61   62   63   64   65   66   67   68   69   70   71
  5     72   73   74   75   76   77   78   79   80   81   82   83
  6     84   85   86   87   88   89   90   91   92   93   94   95
```

---

## Licence

MIT — do whatever you like with it.
