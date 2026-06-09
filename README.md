# Pico AM Radio — MicroPython Edition

Play MIDI files as OOK AM transmissions using a Raspberry Pi Pico or Pico 2 W — no extra hardware needed. Tune a nearby AM radio to **~1000 kHz** and hold the antenna wire close to it.

---

## Why GP0 (Pin 1)?

This build uses **GP0 (physical Pin 1)** — the top-left corner pin — instead of GP2.

On the Pico, Pin 1 sits at the very edge of the board. If you accidentally brush the antenna wire against an adjacent pin, you'll only touch **Pin 2 (GP1)**, which is a plain GPIO and safe. By contrast, GP2 (Pin 4) sits next to **Pin 3 (GND)** on one side and **Pin 5 (GP3)** on the other — less risky than power rails, but a corner pin is cleaner when dangling a bare wire off the board.

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
| Raspberry Pi Pico or Pico 2 W | Any variant works |
| ~20 cm wire | Solid core or stranded, bare end |
| AM radio | Portable battery radio works best |

Solder or clip the wire to **Pin 1 (GP0)**. That's it.

---

## Installation

1. Flash your Pico with the latest **MicroPython firmware** from [micropython.org/download](https://micropython.org/download/rp2-pico/).
2. Open **Thonny** (or use `mpremote`).
3. Copy `pico_am_radio.py` to the Pico's filesystem.
4. Copy your `.mid` file(s) to the Pico's filesystem alongside it.

---

## Playing a MIDI File

### Quick start from the REPL

```python
import pico_am_radio
pico_am_radio.play_midi("twinkle.mid")
```

### Loop forever

```python
pico_am_radio.play_midi("twinkle.mid", loop=True)
```

Press **Ctrl-C** to stop.

### Auto-play on boot

Rename `pico_am_radio.py` to `main.py`, then edit the `SONG_FILE` line near the top:

```python
SONG_FILE = "twinkle.mid"   # ← change to your filename
```

The Pico will start playing every time it powers on.

---

## Changing the Tune

### Option A — Use a different MIDI file

The simplest approach. Any **type-0 or type-1 MIDI file** with PPQ timing works. Good free sources:

- [Kunstderfuge.com](https://www.kunstderfuge.com/) — classical
- [VGMusic.com](https://www.vgmusic.com/) — video game music
- [FreeMidi.org](https://freemidi.org/)

Just copy the `.mid` to the Pico and update `SONG_FILE`.

> **Tip:** Keep files small (under ~50 KB). Large orchestral MIDI with hundreds of tracks parses slowly in MicroPython. Single-track or melody-only exports work best.

### Option B — Edit the built-in demo song

Open `pico_am_radio.py` and find the `DEMO` list near the bottom of the file:

```python
DEMO = [
    (72, 2), (67, 1), (67, 1), (68, 2),
    (67, 2), (-1, 2), (71, 2), (72, 2), (-1, 2),
]
```

Each entry is `(midi_note, beats)`:

| Value | Meaning |
|---|---|
| `midi_note` | MIDI note number. Middle C = 60, C5 = 72. Use -1 for a rest. |
| `beats` | Duration in beats. 1 = one beat, 2 = two beats, 0.5 = eighth note, etc. |

**Common MIDI note numbers:**

| Note | C4 | D4 | E4 | F4 | G4 | A4 | B4 | C5 |
|---|---|---|---|---|---|---|---|---|
| Number | 60 | 62 | 64 | 65 | 67 | 69 | 71 | 72 |

To change the tempo, edit `TEMPO_MS` (milliseconds per beat, default 500 = 120 BPM):

```python
TEMPO_MS = 400   # faster  (~150 BPM)
TEMPO_MS = 600   # slower  (~100 BPM)
```

Then call it directly:

```python
pico_am_radio.play_song(pico_am_radio.DEMO, loop=True)
```

### Option C — Pass a custom song list from the REPL

```python
import pico_am_radio

# Ode to Joy (snippet)
ode = [
    (64,2),(64,2),(65,2),(67,2),
    (67,2),(65,2),(64,2),(62,2),
    (60,2),(60,2),(62,2),(64,2),
    (64,3),(62,1),(62,4),
]

pico_am_radio.play_song(ode)
```

---

## Troubleshooting

**Nothing on the radio**
- Make sure your radio is on AM, not FM.
- Try tuning slowly around 900–1100 kHz.
- Hold the radio's antenna right next to the Pico wire.
- Confirm `TONE_GPIO = 0` is set in the file.

**Garbled or wrong notes**
- Use a simple melody-only MIDI. Drum tracks (channel 10) and dense chords can confuse the single-voice parser.
- Re-export from a DAW as a type-0, single-track MIDI if possible.

**`MidiParseError: SMPTE timecode MIDI not supported`**
- Re-export the file from any DAW or converter (MuseScore, GarageBand, LMMS) choosing **PPQ / ticks-per-beat** timing — this is the default in every common tool.

**Buzzing or jitter on high notes**
- This is normal above ~C6 due to MicroPython's interrupt overhead.
- Notes in the C4–C5 range (middle octave) are cleanest.
- Disabling USB polling helps: add `import usb; usb.active(0)` before calling `play_midi` on builds that support it.

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
